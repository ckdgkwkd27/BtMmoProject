#include <Pch.h>
#include "MonsterBehaviorSystem.h"

static float Distance2D(NavPoint a, NavPoint b)
{
	return std::hypot(a.x - b.x, a.y - b.y);
}

static auto After(MonsterBehaviorSystem::TimePoint now, float seconds)
{
	return now + std::chrono::duration_cast<MonsterBehaviorSystem::Clock::duration>(std::chrono::duration<float>(seconds));
}

static uint64 CrowdCell(int32 x, int32 y)
{
	return (static_cast<uint64>(static_cast<uint32>(x)) << 32) | static_cast<uint32>(y);
}

static uint64 CrowdCell(NavPoint point)
{
	return CrowdCell(static_cast<int32>(std::floor(point.x / 100)), static_cast<int32>(std::floor(point.y / 100)));
}

MonsterBehaviorSystem::MonsterBehaviorSystem(std::shared_ptr<const NavigationMesh> _navigation, MonsterSpawner& _monsters,
                                             Players& _players, StateCallback _onState, DamageCallback _onDamage, uint32 seed)
    : navigation(std::move(_navigation)), monsters(_monsters), players(_players), onState(std::move(_onState)),
      onDamage(std::move(_onDamage)), random(seed), tree(CreateBehaviorTree())
{
}

void MonsterBehaviorSystem::Tick(TimePoint now)
{
	const float delta = previousTick == TimePoint{} ? 0.1f : std::clamp(std::chrono::duration<float>(now - previousTick).count(), 0.0f, 0.2f);
	previousTick = now;
	const auto snapshot = monsters.Snapshot();

	crowdPositions.clear();
	crowdCells.clear();

	for (const auto& monster : snapshot)
	{
		if (monster->IsAlive() && monster->GetTemplateId() == 1)
		{
			UpdateCrowdPosition(monster->GetId(), monster->GetPosition());
		}
	}

	for (auto it = blackboards.begin(); it != blackboards.end();)
	{
		if (!crowdPositions.contains(it->first))
		{
			ReleaseTarget(it->first, it->second);
			it = blackboards.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (const auto& monster : snapshot)
	{
		if (!monster->IsAlive() || monster->GetTemplateId() != 1)
		{
			continue;
		}

		auto [entry, inserted] = blackboards.try_emplace(monster->GetId());
		auto& brain = entry->second;

		if (inserted)
		{
			brain.home = monster->GetPosition();
			brain.nextPatrol = After(now, std::uniform_real_distribution<float>(2, 4)(random));
		}

		Context context{*monster, brain, now, delta};
		SelectTarget(context);
		UpdateBattleSlot(context);

		brain.speed = 0;
		brain.action = EMonsterAction::IDLE;
		tree.Tick(context);

		if (onState)
		{
			onState(Snapshot(*monster));
		}
	}
}

void MonsterBehaviorSystem::Remove(uint64 id)
{
	if (const auto found = blackboards.find(id); found != blackboards.end())
	{
		ReleaseTarget(id, found->second);
	}

	blackboards.erase(id);
}

MonsterBehaviorSnapshot MonsterBehaviorSystem::Snapshot(const Monster& monster) const
{
	const auto p = monster.GetPosition();
	MonsterBehaviorSnapshot result{{monster.GetId(), p.x, p.y, p.z}, {monster.GetId()}};
	if (const auto found = blackboards.find(monster.GetId()); found != blackboards.end())
	{
		const auto& brain = found->second;
		result.movement.yaw = brain.yaw;
		result.movement.speed = brain.speed;
		result.state.action = brain.action;
		result.state.attackSequence = brain.attackSequence;
		result.state.targetId = brain.targetId;
	}

	return result;
}

std::optional<BattleSlot::Key> MonsterBehaviorSystem::GetBattleSlot(uint64 monsterId) const
{
	const auto brain = blackboards.find(monsterId);
	if (brain == blackboards.end())
	{
		return std::nullopt;
	}

	const auto slots = battleSlots.find(brain->second.targetId);
	return slots == battleSlots.end() ? std::nullopt : slots->second.FindSlot(monsterId);
}

MonsterBehaviorSystem::Node MonsterBehaviorSystem::CreateBehaviorTree()
{
	const auto attack = Node::Leaf([this](Context& context)
	{
		return Attack(context);
	});

	const auto hasTarget = Node::Leaf([this](Context& context)
	{
		return HasTarget(context) ? BehaviorStatus::SUCCESS : BehaviorStatus::FAILURE;
	});

	const auto chase = Node::Leaf([this](Context& context)
	{
		return Chase(context);
	});

	const auto patrol = Node::Leaf([this](Context& context)
	{
		return Patrol(context);
	});

	return Node::Selector({attack, Node::Sequence({hasTarget, chase}), patrol});
}

bool MonsterBehaviorSystem::HasTarget(const Context& context) const
{
	const auto found = players.find(context.brain.targetId);
	return found != players.end() && found->second.hp > 0 &&
	       context.now - found->second.lastSeen < std::chrono::seconds(2) &&
	       Distance2D(found->second.position, context.brain.home) <= settings.leashRange &&
	       Distance2D(context.monster.GetPosition(), context.brain.home) <= settings.leashRange;
}

void MonsterBehaviorSystem::SelectTarget(Context& context)
{
	auto& brain = context.brain;
	if (HasTarget(context))
	{
		return;
	}

	if (brain.targetId != 0)
	{
		ReleaseTarget(context.monster.GetId(), brain);
		brain.attackUntil = context.now;
		brain.nextPatrol = context.now;
	}

	if (context.now < brain.nextTarget)
	{
		return;
	}

	brain.nextTarget = After(context.now, 0.5f);
	std::vector<std::pair<float, uint64>> candidates;
	for (const auto& [id, player] : players)
	{
		const float distance = Distance2D(context.monster.GetPosition(), player.position);
		if (player.hp > 0 && context.now - player.lastSeen < std::chrono::seconds(2) &&
		    distance <= settings.detectionRange && Distance2D(brain.home, player.position) <= settings.leashRange)
		{
			candidates.emplace_back(distance, id);
		}
	}

	std::ranges::sort(candidates);
	for (const auto& id : candidates | std::views::values)
	{
		if (const auto path = navigation->FindPath(context.monster.GetPosition(), players.at(id).position); not path)
		{
			continue;
		}

		brain.targetId = id;
		brain.nextPath = context.now;
		break;
	}
}

void MonsterBehaviorSystem::ReleaseTarget(uint64 monsterId, Blackboard& brain)
{
	const auto found = battleSlots.find(brain.targetId);
	if (found != battleSlots.end())
	{
		found->second.ReleaseSlot(monsterId);
		if (found->second.GetOccupiedCount() == 0)
		{
			battleSlots.erase(found);
		}
	}

	brain.targetId = 0;
	brain.slotPosition.reset();
	brain.path.clear();
}

void MonsterBehaviorSystem::UpdateBattleSlot(Context& context)
{
	auto& brain = context.brain;
	if (!HasTarget(context) || context.now < brain.nextPath)
	{
		return;
	}

	brain.nextPath = After(context.now, 0.3f);
	auto& slots = battleSlots[brain.targetId];
	auto slot = slots.GetBestSlot(*navigation, players.at(brain.targetId).position, context.monster.GetPosition(),
	                              settings.bodyRadius, settings.surroundRange, context.monster.GetId());
	if (!slot || Distance2D(slot->targetPos, brain.home) > settings.leashRange ||
	    !slots.AcquireSlot(slot->slotKey, context.monster.GetId()))
	{
		slots.ReleaseSlot(context.monster.GetId());
		brain.slotPosition.reset();
		brain.path.clear();
		return;
	}

	if (!brain.slotPosition || brain.path.empty() || Distance2D(brain.path.back(), slot->targetPos) > 12)
	{
		brain.path = std::move(slot->path);
		brain.waypoint = 1;
	}

	brain.slotPosition = slot->targetPos;
}

BehaviorStatus MonsterBehaviorSystem::Attack(Context& context)
{
	auto& brain = context.brain;
	if (context.now < brain.attackUntil)
	{
		brain.action = EMonsterAction::ATTACK;
		return BehaviorStatus::RUNNING;
	}

	if (!HasTarget(context))
	{
		return BehaviorStatus::FAILURE;
	}

	auto& player = players.at(brain.targetId);
	const auto position = context.monster.GetPosition();
	const auto slot = GetBattleSlot(context.monster.GetId());
	if (!slot || slot->ringIndex != 0 || !brain.slotPosition ||
	    Distance2D(position, *brain.slotPosition) > settings.slotTolerance ||
	    Distance2D(position, player.position) > settings.attackRange ||
	    std::abs(position.z - player.position.z) > settings.attackHeight ||
	    !navigation->TraceSurface(position, player.position))
	{
		return BehaviorStatus::FAILURE;
	}

	brain.path.clear();
	FaceTarget(context, player.position);

	if (context.now >= brain.nextAttack)
	{
		++brain.attackSequence;
		if (brain.attackSequence == 0)
		{
			++brain.attackSequence;
		}

		brain.nextAttack = After(context.now, settings.attackCooldown);
		brain.attackUntil = After(context.now, settings.attackDuration);
		brain.action = EMonsterAction::ATTACK;
		player.hp = std::max(0, player.hp - settings.attackDamage);
		if (onDamage)
		{
			onDamage(brain.targetId, player.hp);
		}
	}

	return BehaviorStatus::RUNNING;
}

BehaviorStatus MonsterBehaviorSystem::Chase(Context& context)
{
	auto& brain = context.brain;
	if (!brain.slotPosition)
	{
		return BehaviorStatus::RUNNING;
	}

	if (Distance2D(context.monster.GetPosition(), *brain.slotPosition) > 2)
	{
		FollowPath(context, settings.runSpeed);
	}
	else
	{
		FaceTarget(context, players.at(brain.targetId).position);
	}

	return BehaviorStatus::RUNNING;
}

BehaviorStatus MonsterBehaviorSystem::Patrol(Context& context)
{
	auto& brain = context.brain;
	if (context.now < brain.nextPatrol)
	{
		return BehaviorStatus::RUNNING;
	}

	if (brain.path.empty())
	{
		const auto destination = navigation->RandomReachablePoint(brain.home, settings.patrolRadius, random);
		if (!destination)
		{
			PausePatrol(context);
			return BehaviorStatus::FAILURE;
		}

		auto path = navigation->FindPath(context.monster.GetPosition(), *destination);
		if (!path)
		{
			PausePatrol(context);
			return BehaviorStatus::FAILURE;
		}

		brain.path = std::move(*path);
		brain.waypoint = 1;
	}

	if (!FollowPath(context, settings.walkSpeed))
	{
		PausePatrol(context);
	}

	return BehaviorStatus::RUNNING;
}

void MonsterBehaviorSystem::PausePatrol(Context& context)
{
	context.brain.path.clear();
	context.brain.nextPatrol = After(context.now, std::uniform_real_distribution<float>(2, 4)(random));
}

bool MonsterBehaviorSystem::FollowPath(Context& context, float speed)
{
	auto& brain = context.brain;
	if (brain.waypoint >= brain.path.size() || context.delta <= 0)
	{
		return false;
	}

	const auto position = context.monster.GetPosition();
	const auto destination = brain.path[brain.waypoint];
	const float distance = Distance2D(position, destination);
	if (distance < 0.5f)
	{
		++brain.waypoint;
		return brain.waypoint < brain.path.size();
	}

	const float alpha = distance > 0.001f ? std::min(1.0f, speed * context.delta / distance) : 1.0f;
	const NavPoint candidate{position.x + (destination.x - position.x) * alpha,
	                         position.y + (destination.y - position.y) * alpha,
	                         position.z + (destination.z - position.z) * alpha};
	auto grounded = navigation->TraceSurface(position, candidate);
	if (!grounded || Distance2D(*grounded, brain.home) > settings.leashRange)
	{
		brain.path.clear();
		return false;
	}

	if (!CanMove(context.monster.GetId(), position, *grounded))
	{
		if (brain.slotPosition && context.now >= brain.nextAvoidance)
		{
			brain.nextAvoidance = After(context.now, 0.5f);
			if (FindCrowdPath(context))
			{
				return true;
			}
		}

		// Steer around nearby bodies instead of sharing the same chase path/endpoint.
		bool avoided = false;
		const float dx = candidate.x - position.x;
		const float dy = candidate.y - position.y;
		const float side = context.monster.GetId() % 2 ? 1.0f : -1.0f;
		for (float degrees : {45.0f, -45.0f, 90.0f, -90.0f, 135.0f, -135.0f})
		{
			const float angle = degrees * side * 0.01745329252f;
			NavPoint detour{position.x + dx * std::cos(angle) - dy * std::sin(angle),
			                position.y + dx * std::sin(angle) + dy * std::cos(angle), position.z};
			auto trial = navigation->TraceSurface(position, detour);
			if (trial && Distance2D(*trial, brain.home) <= settings.leashRange && CanMove(context.monster.GetId(), position, *trial))
			{
				grounded = trial;
				avoided = true;
				break;
			}
		}

		if (!avoided)
		{
			return true;
		}
	}

	FaceTarget(context, *grounded);
	if (!context.monster.SetPosition(*grounded))
	{
		return false;
	}

	UpdateCrowdPosition(context.monster.GetId(), *grounded);
	brain.speed = Distance2D(position, *grounded) / context.delta;
	brain.action = brain.speed > 1 ? (speed == settings.runSpeed ? EMonsterAction::RUN : EMonsterAction::WALK) : EMonsterAction::IDLE;
	if (Distance2D(*grounded, destination) < 0.5f)
	{
		++brain.waypoint;
	}

	return brain.waypoint < brain.path.size();
}

void MonsterBehaviorSystem::FaceTarget(Context& context, NavPoint target)
{
	const auto position = context.monster.GetPosition();
	if (Distance2D(position, target) > 0.01f)
	{
		context.brain.yaw = std::atan2(target.y - position.y, target.x - position.x) * (180.0f / 3.14159265358979323846f);
	}
}

bool MonsterBehaviorSystem::FindCrowdPath(Context& context)
{
	const auto start = context.monster.GetPosition();
	const auto goal = *context.brain.slotPosition;
	struct Obstacle
	{
		NavPoint position;
		float radius;
	};

	std::vector<Obstacle> obstacles;
	for (const auto& [id, position] : crowdPositions)
	{
		if (id != context.monster.GetId() && std::abs(position.z - start.z) < settings.attackHeight &&
		    (Distance2D(position, start) < 700 || Distance2D(position, goal) < 700))
		{
			obstacles.push_back({position, settings.bodyRadius * 2 + 8});
		}
	}

	for (const auto& [id, player] : players)
	{
		if (player.hp > 0 && previousTick - player.lastSeen < std::chrono::seconds(2) && Distance2D(player.position, start) < 1000)
		{
			obstacles.push_back({player.position, settings.bodyRadius + 42 + 8});
		}
	}

	// A bounded local visibility graph routes around whole groups, avoiding oscillation in tight lanes.
	if (obstacles.size() > 64)
	{
		return false;
	}

	const auto clear = [&](NavPoint a, NavPoint b)
	{
		const float dx = b.x - a.x;
		const float dy = b.y - a.y;
		const float length = dx * dx + dy * dy;
		for (const auto& obstacle : obstacles)
		{
			const float t = length > 0.0001f ? std::clamp(((obstacle.position.x - a.x) * dx + (obstacle.position.y - a.y) * dy) / length, 0.0f, 1.0f) : 0;
			if (Distance2D({a.x + dx * t, a.y + dy * t, a.z}, obstacle.position) < obstacle.radius + 0.5f)
			{
				return false;
			}
		}

		return true;
	};

	if (!clear(goal, goal))
	{
		return false;
	}

	std::vector<NavPoint> nodes{start, goal};
	for (const auto& obstacle : obstacles)
	{
		for (int i = 0; i < 12; ++i)
		{
			const float angle = i * 6.2831853f / 12;
			const float radius = obstacle.radius / 0.9659258f + 3;
			NavPoint point{obstacle.position.x + std::cos(angle) * radius, obstacle.position.y + std::sin(angle) * radius, obstacle.position.z};
			if (!clear(point, point))
			{
				continue;
			}

			auto projected = navigation->ProjectPoint(point);
			if (projected && Distance2D(point, *projected) < 0.5f && Distance2D(*projected, context.brain.home) <= settings.leashRange && clear(*projected, *projected))
			{
				nodes.push_back(*projected);
			}
		}
	}

	std::vector<float> cost(nodes.size(), std::numeric_limits<float>::max());
	std::vector<std::size_t> parent(nodes.size(), nodes.size());
	using Entry = std::pair<float, std::size_t>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
	cost[0] = 0;
	open.emplace(Distance2D(start, goal), 0);
	while (!open.empty())
	{
		const auto [estimate, current] = open.top();
		open.pop();
		if (current == 1)
		{
			std::vector<NavPoint> path;
			for (std::size_t i = 1; i < nodes.size(); i = parent[i])
			{
				path.push_back(nodes[i]);
			}

			std::reverse(path.begin(), path.end());
			context.brain.path = std::move(path);
			context.brain.waypoint = 1;
			return true;
		}

		if (estimate > cost[current] + Distance2D(nodes[current], goal) + 0.01f)
		{
			continue;
		}

		for (std::size_t next = 1; next < nodes.size(); ++next)
		{
			const float distance = Distance2D(nodes[current], nodes[next]);
			const float candidate = cost[current] + distance;
			if (candidate >= cost[next] || !clear(nodes[current], nodes[next]) || !navigation->TraceSurface(nodes[current], nodes[next]))
			{
				continue;
			}

			cost[next] = candidate;
			parent[next] = current;
			open.emplace(candidate + Distance2D(nodes[next], goal), next);
		}
	}

	return false;
}

bool MonsterBehaviorSystem::CanMove(uint64 id, NavPoint from, NavPoint to) const
{
	const auto avoids = [&](NavPoint other, float clearance)
	{
		if (std::abs(to.z - other.z) > settings.attackHeight)
		{
			return true;
		}

		const float before = Distance2D(from, other);
		const float dx = to.x - from.x;
		const float dy = to.y - from.y;
		const float lengthSquared = dx * dx + dy * dy;
		const float t = lengthSquared > 0.0001f ? std::clamp(((other.x - from.x) * dx + (other.y - from.y) * dy) / lengthSquared, 0.0f, 1.0f) : 0;
		const NavPoint nearest{from.x + dx * t, from.y + dy * t, from.z};
		// Existing spawn overlaps may separate, but must never get worse.
		return Distance2D(nearest, other) + 0.01f >= std::min(before, clearance) &&
		       (before >= clearance || Distance2D(to, other) > before + 0.01f);
	};

	const float clearance = settings.bodyRadius * 2 + 8;
	for (int32 x = static_cast<int32>(std::floor((to.x - clearance - 64) / 100));
	     x <= static_cast<int32>(std::floor((to.x + clearance + 64) / 100)); ++x)
	{
		for (int32 y = static_cast<int32>(std::floor((to.y - clearance - 64) / 100));
		     y <= static_cast<int32>(std::floor((to.y + clearance + 64) / 100)); ++y)
		{
			const auto cell = crowdCells.find(CrowdCell(x, y));
			if (cell == crowdCells.end())
			{
				continue;
			}

			for (uint64 other : cell->second)
			{
				if (other != id && !avoids(crowdPositions.at(other), clearance))
				{
					return false;
				}
			}
		}
	}

	for (const auto& [playerId, player] : players)
	{
		if (player.hp > 0 && previousTick - player.lastSeen < std::chrono::seconds(2) && !avoids(player.position, settings.bodyRadius + 42 + 8))
		{
			return false;
		}
	}

	return true;
}

void MonsterBehaviorSystem::UpdateCrowdPosition(uint64 id, NavPoint position)
{
	const auto old = crowdPositions.find(id);
	if (old != crowdPositions.end())
	{
		if (CrowdCell(old->second) == CrowdCell(position))
		{
			old->second = position;
			return;
		}

		std::erase(crowdCells[CrowdCell(old->second)], id);
	}

	crowdPositions.insert_or_assign(id, position);
	crowdCells[CrowdCell(position)].push_back(id);
}
