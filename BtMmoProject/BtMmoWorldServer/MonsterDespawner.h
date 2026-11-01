#pragma once
#include <Types.h>

class MonsterSpawner;

class MonsterDespawner final
{
public:
	explicit MonsterDespawner(MonsterSpawner& _spawner);
	bool Despawn(uint64 id);
	std::size_t DespawnDead();
	std::size_t DespawnAll();
private:
	MonsterSpawner& spawner;
};
