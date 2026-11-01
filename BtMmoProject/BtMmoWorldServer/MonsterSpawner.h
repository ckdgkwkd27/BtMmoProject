#pragma once
#include <Types.h>
#include "Monster.h"

class MonsterSpawner final
{
public:
	MonsterSpawner() = default;
	~MonsterSpawner();
	MonsterSpawner(const MonsterSpawner&) = delete;
	MonsterSpawner& operator=(const MonsterSpawner&) = delete;
	std::expected<MonsterPtr, std::string> Spawn(const MonsterSpawnInfo& info);
	MonsterPtr Find(uint64 id) const;
	std::size_t Count() const;
	std::vector<MonsterPtr> Snapshot() const;
	void SetLifecycleCallbacks(std::function<void(const Monster&)> onSpawn, std::function<void(uint64)> onDespawn);
private:
	friend class MonsterDespawner;
	uint64 nextId{1};
	std::unordered_map<uint64, MonsterPtr> monsters;
	std::function<void(const Monster&)> onSpawn;
	std::function<void(uint64)> onDespawn;
};
