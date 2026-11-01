#include <Pch.h>
#include "MonsterDespawner.h"
#include "MonsterSpawner.h"

MonsterDespawner::MonsterDespawner(MonsterSpawner& _spawner) : spawner(_spawner)
{
}

bool MonsterDespawner::Despawn(uint64 id)
{
	const auto found = spawner.monsters.find(id);
	if (found == spawner.monsters.end())
	{
		return false;
	}

	found->second->MarkDespawned();
	spawner.monsters.erase(found);
	if (spawner.onDespawn)
	{
		spawner.onDespawn(id);
	}

	return true;
}

std::size_t MonsterDespawner::DespawnDead()
{
	std::size_t count = 0;
	for (const auto& monster : spawner.Snapshot())
	{
		if (monster->GetState() == MonsterState::DEAD)
		{
			count += Despawn(monster->GetId());
		}
	}

	return count;
}

std::size_t MonsterDespawner::DespawnAll()
{
	const auto count = spawner.Count();
	while (!spawner.monsters.empty())
	{
		Despawn(spawner.monsters.begin()->first);
	}

	return count;
}
