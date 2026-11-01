#include <Pch.h>
#include "MonsterSpawnSystem.h"

MonsterSpawnSystem::MonsterSpawnSystem(std::shared_ptr<const NavigationMesh> _navigation, MonsterSpawner& _monsters, MonsterDespawner& _despawner)
    : navigation(std::move(_navigation)), monsters(_monsters), despawner(_despawner)
{
}

MonsterSpawnSystem::~MonsterSpawnSystem()
{
	Stop();
}

std::expected<void, std::string> MonsterSpawnSystem::Start(const MonsterSpawnConfig& _config, TimePoint now)
{
	if (running)
	{
		return std::unexpected("Monster spawning is already running");
	}

	if (const auto valid = _config.Validate(); !valid)
	{
		return valid;
	}

	if (!navigation || navigation->MapName() != _config.map || !navigation->PolygonCount())
	{
		return std::unexpected("Spawn config map does not match the loaded NavMesh");
	}

	std::mt19937 _random(_config.seed);
	std::vector<SpawnPoint> _points;
	for (unsigned i = 0; i < _config.spawnerCount; ++i)
	{
		bool placed = false;
		for (unsigned attempt = 0; attempt < 512; ++attempt)
		{
			const auto candidate = navigation->RandomPoint(_random);
			if (!candidate)
			{
				return std::unexpected(candidate.error());
			}

			const bool spaced = std::ranges::all_of(_points, [&](const SpawnPoint& other)
			{
				const double dx = static_cast<double>(candidate->x) - other.position.x;
				const double dy = static_cast<double>(candidate->y) - other.position.y;
				return dx * dx + dy * dy >= static_cast<double>(_config.minSpawnerDistance) * _config.minSpawnerDistance;
			});

			if (!spaced)
			{
				continue;
			}

			if (!navigation->RandomReachablePoint(*candidate, _config.spawnRadius, _random))
			{
				continue;
			}

			SpawnPoint point;
			point.id = i + 1;
			point.position = *candidate;
			for (unsigned slot = 0; slot < _config.maxAlive; ++slot)
			{
				point.pending.insert(now);
			}

			_points.push_back(std::move(point));
			placed = true;
			break;
		}

		if (!placed)
		{
			return std::unexpected("Cannot place all spawners: reduce spawnerCount/minSpawnerDistance or increase spawnRadius");
		}
	}

	config = _config;
	random = _random;
	points = std::move(_points);
	running = true;
	Tick(now);
	return {};
}

void MonsterSpawnSystem::OnMonsterRemoved(uint64 id, TimePoint now)
{
	if (!running)
	{
		return;
	}

	for (auto& point : points)
	{
		if (point.alive.erase(id))
		{
			point.pending.insert(now + std::chrono::milliseconds(config.respawnDelayMs));
			return;
		}
	}
}

void MonsterSpawnSystem::Tick(TimePoint now)
{
	if (!running)
	{
		return;
	}

	for (auto& point : points)
	{
		for (auto it = point.alive.begin(); it != point.alive.end();)
		{
			const auto monster = monsters.Find(*it);
			if (monster && monster->IsAlive())
			{
				++it;
				continue;
			}

			if (monster)
			{
				despawner.Despawn(*it);
			}

			it = point.alive.erase(it);
			point.pending.insert(now + std::chrono::milliseconds(config.respawnDelayMs));
		}

		unsigned spawned = 0;
		for (unsigned budget = 0; budget < 16 && !point.pending.empty() && *point.pending.begin() <= now && point.alive.size() < config.maxAlive; ++budget)
		{
			const auto position = navigation->RandomReachablePoint(point.position, config.spawnRadius, random);
			if (!position)
			{
				++point.failedQueries;
				if (point.failedQueries == 1 || point.failedQueries % 100 == 0)
				{
					std::cerr << "SPAWN_RETRY spawner=" << point.id << " failures=" << point.failedQueries << " reason=" << position.error() << '\n';
				}

				break;
			}

			const auto monster = monsters.Spawn({config.templateId, *position, config.maxHp, point.id});
			if (!monster)
			{
				break;
			}

			point.alive.insert((*monster)->GetId());
			point.pending.erase(point.pending.begin());
			++spawned;
		}

		if (spawned)
		{
			std::ostringstream line;
			line << "SPAWNER id=" << point.id << " center=(" << point.position.x << ',' << point.position.y << ',' << point.position.z
			     << ") spawned=" << spawned << " alive=" << point.alive.size() << '/' << config.maxAlive << " pending=" << point.pending.size() << '\n';
			std::cout << line.str();
		}
	}
}

void MonsterSpawnSystem::Stop()
{
	running = false;
	for (auto& point : points)
	{
		for (const auto id : point.alive)
		{
			despawner.Despawn(id);
		}
	}

	points.clear();
}

std::vector<MonsterSpawnPointSnapshot> MonsterSpawnSystem::Snapshot() const
{
	std::vector<MonsterSpawnPointSnapshot> result;
	for (const auto& point : points)
	{
		MonsterSpawnPointSnapshot snapshot{point.id, point.position, config.maxAlive, point.pending.size(), {}};
		for (const auto id : point.alive)
		{
			if (const auto monster = monsters.Find(id); monster && monster->IsAlive())
			{
				snapshot.aliveMonsters.push_back({id, monster->GetPosition(), monster->GetHp()});
			}
		}

		std::ranges::sort(snapshot.aliveMonsters, {}, &SpawnedMonsterSnapshot::id);
		result.push_back(std::move(snapshot));
	}

	return result;
}
