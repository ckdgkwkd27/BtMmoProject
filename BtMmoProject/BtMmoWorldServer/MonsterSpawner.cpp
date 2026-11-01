#include <Pch.h>
#include "MonsterSpawner.h"
#include "MonsterDespawner.h"

MonsterSpawner::~MonsterSpawner()
{
	SetLifecycleCallbacks({}, {});
	MonsterDespawner(*this).DespawnAll();
}

std::expected<MonsterPtr, std::string> MonsterSpawner::Spawn(const MonsterSpawnInfo& info)
{
	if (info.templateId == 0)
	{
		return std::unexpected("Monster template ID must be nonzero");
	}

	if (info.maxHp <= 0)
	{
		return std::unexpected("Monster max HP must be positive");
	}

	if (!std::isfinite(info.position.x) || !std::isfinite(info.position.y) || !std::isfinite(info.position.z))
	{
		return std::unexpected("Monster position must be finite");
	}

	if (nextId == 0)
	{
		return std::unexpected("Monster IDs exhausted");
	}

	auto monster = MonsterPtr(new Monster(nextId, info));
	monsters.emplace(nextId, monster);
	++nextId;
	if (onSpawn)
	{
		onSpawn(*monster);
	}

	return monster;
}

MonsterPtr MonsterSpawner::Find(uint64 id) const
{
	const auto found = monsters.find(id);
	return found == monsters.end() ? nullptr : found->second;
}

std::size_t MonsterSpawner::Count() const
{
	return monsters.size();
}

std::vector<MonsterPtr> MonsterSpawner::Snapshot() const
{
	std::vector<MonsterPtr> result;
	result.reserve(monsters.size());
	for (const auto& monster : monsters | std::views::values)
	{
		result.push_back(monster);
	}

	std::ranges::sort(result, {}, [](const MonsterPtr& monster)
	{
		return monster->GetId();
	});

	return result;
}

void MonsterSpawner::SetLifecycleCallbacks(std::function<void(const Monster&)> _onSpawn, std::function<void(uint64)> _onDespawn)
{
	onSpawn = std::move(_onSpawn);
	onDespawn = std::move(_onDespawn);
}
