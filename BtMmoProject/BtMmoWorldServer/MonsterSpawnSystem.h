#pragma once
#include <Types.h>
#include "MonsterSpawnConfig.h"
#include "MonsterSpawner.h"
#include "MonsterDespawner.h"

struct SpawnedMonsterSnapshot
{
	uint64 id;
	NavPoint position;
	int hp;
};

struct MonsterSpawnPointSnapshot
{
	uint64 id;
	NavPoint position;
	unsigned maxAlive;
	std::size_t pendingRespawns;
	std::vector<SpawnedMonsterSnapshot> aliveMonsters;
};

class MonsterSpawnSystem final
{
public:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;
	MonsterSpawnSystem(std::shared_ptr<const NavigationMesh> _navigation, MonsterSpawner& _monsters, MonsterDespawner& _despawner);
	~MonsterSpawnSystem();
	MonsterSpawnSystem(const MonsterSpawnSystem&) = delete;
	std::expected<void, std::string> Start(const MonsterSpawnConfig& _config, TimePoint now);
	void Tick(TimePoint now);
	void OnMonsterRemoved(uint64 id, TimePoint now);
	void Stop();
	bool IsRunning() const { return running; }

	unsigned TickIntervalMs() const { return config.tickMs; }

	std::vector<MonsterSpawnPointSnapshot> Snapshot() const;
private:
	struct SpawnPoint
	{
		uint64 id{};
		NavPoint position{};
		std::unordered_set<uint64> alive;
		std::multiset<TimePoint> pending;
		uint64 failedQueries{};
	};

	std::shared_ptr<const NavigationMesh> navigation;
	MonsterSpawner& monsters;
	MonsterDespawner& despawner;
	MonsterSpawnConfig config;
	std::mt19937 random;
	std::vector<SpawnPoint> points;
	bool running{false};
};
