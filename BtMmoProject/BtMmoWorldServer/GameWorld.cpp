#include <Pch.h>
#include "GameWorld.h"
#include "WorldMetrics.h"
#include <NetworkCore/CoreRuntime.h>
#include <WorldPacketFactory.h>

static NotifyMonsterHealth MakeMonsterHealth(const Monster& monster)
{
	return {monster.GetId(), static_cast<uint32>(monster.GetHp()), static_cast<uint32>(monster.GetMaxHp())};
}

GameWorld::GameWorld(CoreRuntime& runtime, uint32 tileSize, std::shared_ptr<const NavigationMesh> navigation, std::shared_ptr<WorldMetrics> _metrics)
    : JobQueue(runtime.GetGlobalQueue(), runtime.GetJobTimer()), metrics(std::move(_metrics)), playerNavigation(std::move(navigation)), playerTiles(tileSize), monsterTiles(tileSize), monsterDespawner(monsterSpawner)
{
	monsterSpawner.SetLifecycleCallbacks([this](const Monster& monster)
	{
		AddMonsterVisibility(monster);
	}, [this](uint64 id)
	{
		if (monsterBehaviorSystem)
		{
			monsterBehaviorSystem->Remove(id);
		}

		monsterStates.erase(id);
		RemoveMonsterVisibility(id);
	});
}

GameWorld::~GameWorld()
{
	monsterSpawner.SetLifecycleCallbacks({}, {});
}

std::shared_ptr<GameWorld> GameWorld::GetWorldPtr()
{
	return std::static_pointer_cast<GameWorld>(shared_from_this());
}

void GameWorld::EnterWorld(uint64 id, SessionPtr session)
{
	DoAsync([self = GetWorldPtr(), id, session = std::move(session)]
	{
		if (!session->IsConnected())
		{
			return;
		}

		self->sessions.emplace(id, session);
		const auto points = self->monsterSpawnSystem ? self->monsterSpawnSystem->Snapshot()
		                                             : std::vector<MonsterSpawnPointSnapshot>{};
		if (!session->Send(WorldPacketFactory::MakeSendBuffer(ReturnWelcome{id, static_cast<uint16>(points.size())})))
		{
			session->Disconnect();
			return;
		}

		for (const auto& point : points)
		{
			const NotifySpawner info{point.id, point.position.x, point.position.y, point.position.z,
			                         static_cast<uint16>(point.maxAlive), static_cast<uint16>(point.aliveMonsters.size()),
			                         static_cast<uint16>(point.pendingRespawns)};
			if (!session->Send(WorldPacketFactory::MakeSendBuffer(info)))
			{
				session->Disconnect();
				return;
			}
		}

		// Unit visibility starts after the first accepted player position.

	});
}

void GameWorld::LeaveWorld(uint64 id)
{
	DoAsync([self = GetWorldPtr(), id]
	{
		self->RemovePlayerVisibility(id);
		self->sessions.erase(id);
		self->players.erase(id);
		self->playerCells.Remove(id);
		self->playerMoves.erase(id);
		self->meleeAttacks.erase(id);
		self->playerStates.erase(id);
	});
}

void GameWorld::UpdatePlayerMove(uint64 id, RequestPlayerMove movement)
{
	DoAsync([self = GetWorldPtr(), id, movement]
	{
		self->ApplyPlayerMove(id, movement);
	});
}

std::optional<RequestPlayerPosition> GameWorld::FindPlayerSpawn(uint64 id, RequestPlayerPosition requested) const
{
	std::optional<NavPoint> ground;
	if (playerNavigation)
	{
		// PlayerStart can be above the floor. Check the landing column as well as
		// the initial body, otherwise a clear airborne spawn can get stuck later.
		for (int drop = 0; drop <= 1000; drop += 100)
		{
			const auto projected = playerNavigation->ProjectPoint({static_cast<float>(requested.x), static_cast<float>(requested.y), static_cast<float>(requested.z - drop)});
			if (projected && std::hypot(projected->x - requested.x, projected->y - requested.y) <= 0.5 && projected->z <= requested.z + 2)
			{
				ground = *projected;
				break;
			}
		}
	}

	const auto clearColumn = [&](RequestPlayerPosition top, RequestPlayerPosition bottom)
	{
		return playerCells.CanOccupy(id, top) && playerCells.CanOccupy(id, bottom) && playerCells.CanMove(id, top, bottom);
	};

	const RequestPlayerPosition landing = ground ? RequestPlayerPosition{ground->x, ground->y, ground->z + 2.0} : requested;
	if (clearColumn(requested, landing))
	{
		return requested;
	}

	if (!ground)
	{
		return std::nullopt;
	}

	// Search outward on the same connected surface. Never resolve an occupied
	// start by raising Z, crossing a wall, or placing a player over a cliff.
	constexpr double spacing = PlayerCellGrid::BODY_RADIUS * 2 + 16;
	constexpr double fullTurn = 6.28318530717958647692;
	for (int ring = 1; ring <= 10; ++ring)
	{
		const int samples = ring * 8;
		for (int sample = 0; sample < samples; ++sample)
		{
			const double angle = fullTurn * sample / samples;
			const NavPoint target{static_cast<float>(ground->x + std::cos(angle) * spacing * ring),
			                      static_cast<float>(ground->y + std::sin(angle) * spacing * ring), ground->z};
			const auto surface = playerNavigation->TraceSurface(*ground, target);
			if (!surface)
			{
				continue;
			}

			const RequestPlayerPosition candidate{surface->x, surface->y, surface->z + 2.0};
			const RequestPlayerPosition top{candidate.x, candidate.y, std::max(requested.z, candidate.z)};
			if (clearColumn(top, candidate))
			{
				return candidate;
			}
		}
	}

	return std::nullopt;
}

void GameWorld::ApplyPlayerMove(uint64 id, RequestPlayerMove movement)
{
	const auto found = sessions.find(id);
	if (found == sessions.end() || !WorldProtocol::IsValid(movement))
	{
		return;
	}

	const auto session = found->second.lock();
	if (!session || !session->IsConnected())
	{
		return;
	}

	const auto requested = movement;
	if (!players.contains(id))
	{
		const auto spawn = FindPlayerSpawn(id, {movement.x, movement.y, movement.z});
		if (!spawn)
		{
			std::cerr << "Player spawn unavailable id=" << id << '\n';
			session->Disconnect();
			return;
		}

		if (spawn->x != movement.x || spawn->y != movement.y || spawn->z != movement.z)
		{
			movement.x = spawn->x;
			movement.y = spawn->y;
			movement.z = spawn->z;
			movement.velocityX = movement.velocityY = movement.velocityZ = 0;
			movement.falling = 1;
		}
	}

	auto [entry, inserted] = players.try_emplace(id);
	auto& player = entry->second;
	const auto previous = playerMoves.find(id);
	if (player.hp <= 0)
	{
		movement = previous != playerMoves.end() ? previous->second : RequestPlayerMove{player.position.x, player.position.y, player.position.z};
		movement.velocityX = movement.velocityY = movement.velocityZ = 0;
		movement.falling = 0;
	}
	else if (previous != playerMoves.end())
	{
		const auto& before = previous->second;
		if (!playerCells.CanMove(id, {before.x, before.y, before.z}, {movement.x, movement.y, movement.z}))
		{
			// First version stops the rejected step; facing may still change.
			movement.x = before.x;
			movement.y = before.y;
			movement.z = before.z;
			movement.velocityX = movement.velocityY = movement.velocityZ = 0;
			movement.falling = before.falling;
		}
	}

	player.position = {static_cast<float>(movement.x), static_cast<float>(movement.y), static_cast<float>(movement.z)};
	player.lastSeen = MonsterBehaviorSystem::Clock::now();
	if (player.hp > 0)
	{
		playerCells.Update(id, {movement.x, movement.y, movement.z});
	}
	else
	{
		playerCells.Remove(id);
	}

	if (inserted && !session->Send(WorldPacketFactory::MakeSendBuffer(NotifyPlayerHealth{100, 100})))
	{
		session->Disconnect();
		return;
	}

	playerMoves[id] = movement;
	playerStates.try_emplace(id);
	UpdatePlayerVisibility(id);
	const bool corrected = requested.x != movement.x || requested.y != movement.y || requested.z != movement.z ||
	                       requested.velocityX != movement.velocityX || requested.velocityY != movement.velocityY ||
	                       requested.velocityZ != movement.velocityZ || requested.falling != movement.falling;
	if (corrected && !session->Send(WorldPacketFactory::MakeSendBuffer(NotifyPlayerMove{id, movement})))
	{
		session->Disconnect();
	}
}

void GameWorld::UpdatePlayerState(uint64 id, RequestPlayerState state)
{
	DoAsync([self = GetWorldPtr(), id, state]() mutable
	{
		const auto found = self->sessions.find(id);
		const auto previous = self->playerStates.find(id);
		if (found == self->sessions.end() || previous == self->playerStates.end() || !WorldProtocol::IsValid(state))
		{
			return;
		}

		const auto session = found->second.lock();
		if (!session || !session->IsConnected())
		{
			return;
		}

		const auto& player = self->players.at(id);
		if (player.hp <= 0)
		{
			return;
		}

		if (previous->second == state)
		{
			return;
		}

		previous->second = state;
		self->BroadcastPlayer(id, WorldPacketFactory::MakeSendBuffer(NotifyPlayerState{id, state, static_cast<uint32>(player.hp), 100}));
	});
}

void GameWorld::UpdatePlayerPosition(uint64 id, RequestPlayerPosition position)
{
	DoAsync([self = GetWorldPtr(), id, position]
	{
		RequestPlayerMove movement{position.x, position.y, position.z};
		if (const auto previous = self->playerMoves.find(id); previous != self->playerMoves.end())
		{
			movement.yaw = previous->second.yaw;
		}

		self->ApplyPlayerMove(id, movement);
	});
}

void GameWorld::ReportMeleeHit(uint64 playerId, RequestMeleeHit hit)
{
	DoAsync([self = GetWorldPtr(), playerId, hit]
	{
		const auto player = self->players.find(playerId);
		const auto sessionEntry = self->sessions.find(playerId);
		if (!WorldProtocol::IsValid(hit) || player == self->players.end() || sessionEntry == self->sessions.end())
		{
			return;
		}

		const auto session = sessionEntry->second.lock();
		const auto now = std::chrono::steady_clock::now();
		const auto& settings = self->meleeSettings;
		if (!session || !session->IsConnected() || player->second.hp <= 0 || now - player->second.lastSeen > settings.positionTimeout)
		{
			return;
		}

		auto& attack = self->meleeAttacks[playerId];
		if (hit.attackSequence < attack.sequence)
		{
			return;
		}

		if (hit.attackSequence > attack.sequence)
		{
			if (attack.sequence != 0 && now - attack.started < settings.cooldown)
			{
				return;
			}

			attack.sequence = hit.attackSequence;
			attack.started = now;
			attack.targets.clear();
		}

		if (now - attack.started > settings.hitWindow || attack.targets.size() >= settings.maxTargets ||
		    attack.targets.contains(hit.monsterId))
		{
			return;
		}

		const auto monster = self->monsterSpawner.Find(hit.monsterId);
		if (!monster || !monster->IsAlive())
		{
			return;
		}

		const auto position = monster->GetPosition();
		const auto origin = player->second.position;
		const float dx = position.x - origin.x;
		const float dy = position.y - origin.y;
		if (dx * dx + dy * dy > settings.range * settings.range || std::abs(position.z - origin.z) > settings.height)
		{
			return;
		}

		attack.targets.insert(hit.monsterId);
		self->ApplyMonsterDamage(hit.monsterId, settings.damage);
	});
}

void GameWorld::SpawnMonster(MonsterSpawnInfo info, std::function<void(SpawnResult)> onComplete)
{
	DoAsync([self = GetWorldPtr(), info, onComplete = std::move(onComplete)]
	{
		const auto spawned = info.ownerSpawnerId == 0
		    ? self->monsterSpawner.Spawn(info)
		    : std::unexpected(std::string("Managed spawner IDs are assigned only by the server spawn system"));
		if (onComplete)
		{
			onComplete(spawned.transform(&Monster::GetId));
		}
	});
}

void GameWorld::DespawnMonster(uint64 id, std::function<void(bool)> onComplete)
{
	DoAsync([self = GetWorldPtr(), id, onComplete = std::move(onComplete)]
	{
		const bool removed = self->RemoveMonster(id);
		if (onComplete)
		{
			onComplete(removed);
		}
	});
}

void GameWorld::DamageMonster(uint64 id, int damage, std::function<void(int)> onComplete)
{
	DoAsync([self = GetWorldPtr(), id, damage, onComplete = std::move(onComplete)]
	{
		const int applied = self->ApplyMonsterDamage(id, damage);
		if (onComplete)
		{
			onComplete(applied);
		}
	});
}

int GameWorld::ApplyMonsterDamage(uint64 id, int damage)
{
	const auto monster = monsterSpawner.Find(id);
	const int applied = monster ? monster->ApplyDamage(damage) : 0;
	if (applied > 0)
	{
		BroadcastMonster(id, WorldPacketFactory::MakeSendBuffer(MakeMonsterHealth(*monster)));
		if (!monster->IsAlive())
		{
			RemoveMonster(id);
		}
	}

	return applied;
}

void GameWorld::GetMonsterCount(std::function<void(std::size_t)> onComplete)
{
	if (onComplete)
	{
		DoAsync([self = GetWorldPtr(), onComplete = std::move(onComplete)]
		{
			onComplete(self->monsterSpawner.Count());
		});
	}
}

bool GameWorld::RemoveMonster(uint64 id)
{
	if (!monsterDespawner.Despawn(id))
	{
		return false;
	}

	if (monsterSpawnSystem)
	{
		monsterSpawnSystem->OnMonsterRemoved(id, MonsterSpawnSystem::Clock::now());
	}

	return true;
}

std::future<std::expected<void, std::string>> GameWorld::StartMonsterSpawning(std::shared_ptr<const NavigationMesh> navigation, MonsterSpawnConfig config)
{
	auto result = std::make_shared<std::promise<std::expected<void, std::string>>>();
	auto future = result->get_future();
	DoAsync([self = GetWorldPtr(), navigation = std::move(navigation), config = std::move(config), result]
	{
		if (self->monsterSpawnSystem && self->monsterSpawnSystem->IsRunning())
		{
			result->set_value(std::unexpected("Monster spawning is already running"));
			return;
		}

		auto system = std::make_unique<MonsterSpawnSystem>(navigation, self->monsterSpawner, self->monsterDespawner);
		const auto started = system->Start(config, MonsterSpawnSystem::Clock::now());
		if (started)
		{
			self->monsterSpawnSystem = std::move(system);
			self->monsterBehaviorSystem = std::make_unique<MonsterBehaviorSystem>(navigation, self->monsterSpawner, self->players,
			                                                                      [world = self.get()](const MonsterBehaviorSnapshot& info)
			{
				world->UpdateMonsterVisibility(info);
			},
			                                                                      [world = self.get()](uint64 id, int hp)
			{
				if (hp <= 0)
				{
					world->playerCells.Remove(id);
				}

				if (auto state = world->playerStates.find(id); state != world->playerStates.end())
				{
					if (hp <= 0)
					{
						state->second.attackIndex = 0;
						auto& movement = world->playerMoves.at(id);
						movement.velocityX = movement.velocityY = movement.velocityZ = 0;
						movement.falling = 0;
						world->BroadcastPlayer(id, WorldPacketFactory::MakeSendBuffer(NotifyPlayerMove{id, movement}));
					}

					world->BroadcastPlayer(id, WorldPacketFactory::MakeSendBuffer(NotifyPlayerState{id, state->second, static_cast<uint32>(hp), 100}));
				}

				const auto found = world->sessions.find(id);
				if (found == world->sessions.end())
				{
					return;
				}

				if (const auto session = found->second.lock(); session && session->IsConnected() &&
				                                               !session->Send(WorldPacketFactory::MakeSendBuffer(NotifyPlayerHealth{static_cast<uint32>(hp), 100})))
				{
					session->Disconnect();
				}
			}, config.seed);

			self->ScheduleMonsterTick();
			self->ScheduleBehaviorTick();
		}

		result->set_value(started);
	});

	return future;
}

void GameWorld::ScheduleBehaviorTick()
{
	std::weak_ptr weak = GetWorldPtr();
	behaviorTick = DoTimer(WorldProtocol::MONSTER_TICK_MS, [weak]
	{
		if (auto self = weak.lock(); self && self->monsterBehaviorSystem)
		{
			const auto start = MonsterBehaviorSystem::Clock::now();
			self->monsterBehaviorSystem->Tick(start);
			if (self->metrics)
			{
				self->metrics->ObserveAiTick(MonsterBehaviorSystem::Clock::now() - start);
			}
			self->ScheduleBehaviorTick();
		}
	});
}

void GameWorld::ScheduleMonsterTick()
{
	if (!monsterSpawnSystem || !monsterSpawnSystem->IsRunning())
	{
		return;
	}

	std::weak_ptr weak = GetWorldPtr();
	monsterTick = DoTimer(monsterSpawnSystem->TickIntervalMs(), [weak]
	{
		if (auto self = weak.lock(); self && self->monsterSpawnSystem && self->monsterSpawnSystem->IsRunning())
		{
			self->monsterSpawnSystem->Tick(MonsterSpawnSystem::Clock::now());
			self->ScheduleMonsterTick();
		}
	});
}

void GameWorld::StopMonsterSpawning()
{
	DoAsync([self = GetWorldPtr()]
	{
		self->monsterTick.Cancel();
		self->behaviorTick.Cancel();
		self->monsterBehaviorSystem.reset();
		self->monsterStates.clear();
		if (self->monsterSpawnSystem)
		{
			self->monsterSpawnSystem->Stop();
		}
	});
}

void GameWorld::GetMonsterSpawnPoints(std::function<void(std::vector<MonsterSpawnPointSnapshot>)> onComplete)
{
	if (onComplete)
	{
		DoAsync([self = GetWorldPtr(), onComplete = std::move(onComplete)]
		{
			onComplete(self->monsterSpawnSystem ? self->monsterSpawnSystem->Snapshot() : std::vector<MonsterSpawnPointSnapshot>{});
		});
	}
}
