#include <Pch.h>
#include "MonsterSpawnConfig.h"
#include <boost/json.hpp>

template <typename T>
static bool Read(const boost::json::object& object, const char* name, T& output)
{
	const auto* value = object.if_contains(name);
	if (!value || !value->is_number())
	{
		return false;
	}

	const double number = value->to_number<double>();
	if (!std::isfinite(number) || number < static_cast<double>(std::numeric_limits<T>::lowest()) ||
	    number > static_cast<double>(std::numeric_limits<T>::max()))
	{
		return false;
	}

	if constexpr (std::is_integral_v<T>)
	{
		if (number != std::trunc(number))
		{
			return false;
		}
	}

	output = static_cast<T>(number);
	return true;
}

std::expected<void, std::string> MonsterSpawnConfig::Validate() const
{
	if (map.empty() || spawnerCount == 0 || spawnerCount > 256 || maxAlive == 0 || maxAlive > 1000 ||
	    static_cast<uint64>(spawnerCount) * maxAlive > 10000)
	{
		return std::unexpected("Expected a map, 1..256 spawners, 1..1000 monsters per spawner, at most 10000 monsters total");
	}

	if (!std::isfinite(spawnRadius) || spawnRadius <= 0 || spawnRadius > 100000 ||
	    !std::isfinite(minSpawnerDistance) || minSpawnerDistance < 0 || minSpawnerDistance > 1000000)
	{
		return std::unexpected("Invalid spawn radius or spawner spacing (UE centimetres)");
	}

	if (tickMs < 100 || tickMs > 60000 || respawnDelayMs < 100 || respawnDelayMs > 86400000 || templateId == 0 || maxHp <= 0)
	{
		return std::unexpected("Expected tick 100..60000 ms, respawn 100..86400000 ms, nonzero template ID and positive HP");
	}

	return {};
}

std::expected<MonsterSpawnConfig, std::string> MonsterSpawnConfig::Load(const std::filesystem::path& path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if (!input || input.tellg() <= 0 || input.tellg() > 65536)
	{
		return std::unexpected("Cannot read spawn config (expected 1..65536 bytes)");
	}

	std::string bytes(static_cast<std::size_t>(input.tellg()), '\0');
	input.seekg(0);
	if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
	{
		return std::unexpected("Cannot read spawn config");
	}

	boost::system::error_code error;
	const auto document = boost::json::parse(bytes, error);
	if (error || !document.is_object())
	{
		return std::unexpected("Invalid spawn config JSON");
	}

	const auto& object = document.as_object();
	const auto* mapName = object.if_contains("map");
	unsigned version{};
	MonsterSpawnConfig config;
	if (!mapName || !mapName->is_string() || !Read(object, "version", version) || version != 1 ||
	    !Read(object, "spawnerCount", config.spawnerCount) || !Read(object, "maxAlive", config.maxAlive) ||
	    !Read(object, "spawnRadius", config.spawnRadius) || !Read(object, "minSpawnerDistance", config.minSpawnerDistance) ||
	    !Read(object, "respawnDelayMs", config.respawnDelayMs) || !Read(object, "tickMs", config.tickMs) ||
	    !Read(object, "templateId", config.templateId) || !Read(object, "maxHp", config.maxHp) || !Read(object, "seed", config.seed))
	{
		return std::unexpected("Missing or invalid spawn config field (version 1 required)");
	}

	if (object.size() != 11)
	{
		return std::unexpected("Unknown spawn config field");
	}

	config.map = std::string(mapName->as_string());
	if (const auto valid = config.Validate(); !valid)
	{
		return std::unexpected(valid.error());
	}

	return config;
}
