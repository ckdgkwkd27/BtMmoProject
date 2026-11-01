#pragma once
#include <Types.h>

struct MonsterSpawnConfig
{
	std::string map{"/Game/Maps/Forest"};
	unsigned spawnerCount{64};
	unsigned maxAlive{10};
	float spawnRadius{800};
	float minSpawnerDistance{2000};
	unsigned respawnDelayMs{5000};
	unsigned tickMs{250};
	uint32 templateId{1};
	int maxHp{100};
	uint32 seed{20261023};

	std::expected<void, std::string> Validate() const;
	static std::expected<MonsterSpawnConfig, std::string> Load(const std::filesystem::path& path);
};
