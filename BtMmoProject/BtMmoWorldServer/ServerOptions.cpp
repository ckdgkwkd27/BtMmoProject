#include <Pch.h>
#include "ServerOptions.h"
#include <Options.h>

std::expected<ServerOptions, std::string> ServerOptions::Parse(int argc, wchar_t** argv)
{
	Options args(argc, argv, {"--bind", "--port", "--metrics-bind", "--metrics-port", "--threads", "--max-clients", "--duration", "--idle-seconds", "--navmesh", "--spawn-config", "--tile-size"});
	ServerOptions result;
	result.navMeshFile = args.Text("--navmesh", "");
	result.spawnConfigFile = args.Text("--spawn-config", "");
	result.bindAddress = args.Text("--bind", "127.0.0.1");
	result.port = args.Number<uint16>("--port", result.port, 0, 65535);
	result.metricsBindAddress = args.Text("--metrics-bind", result.metricsBindAddress);
	result.metricsPort = args.Number<uint16>("--metrics-port", result.metricsPort, 0, 65535);
	result.threads = args.Number("--threads", result.threads, 1, 256);
	result.maxClients = args.Number<std::size_t>("--max-clients", result.maxClients, 1, 100000);
	result.tileSize = args.Number("--tile-size", result.tileSize, 100, 10000000);
	result.durationSeconds = args.Number("--duration", result.durationSeconds, 0, 86400);
	result.session.idleTimeoutMs = 1000ULL * args.Number("--idle-seconds", 60, 1, 3600);
	if (!args.Error().empty())
	{
		return std::unexpected(args.Error());
	}

	if ((args.Has("--navmesh") && result.navMeshFile.empty()) ||
	    (args.Has("--spawn-config") && result.spawnConfigFile.empty()))
	{
		return std::unexpected("World data file paths cannot be empty");
	}

	if (!result.spawnConfigFile.empty() && result.navMeshFile.empty())
	{
		return std::unexpected("--spawn-config requires --navmesh");
	}

	if (!args.Has("--navmesh") && !args.Has("--spawn-config"))
	{
		constexpr DWORD PATH_CAPACITY = 32768;
		std::vector<wchar_t> executable(PATH_CAPACITY);
		const DWORD length = GetModuleFileNameW(nullptr, executable.data(), PATH_CAPACITY);
		if (length == 0 || length >= executable.size())
		{
			return std::unexpected("Cannot resolve the server executable path");
		}

		const auto data = std::filesystem::path(executable.data(), executable.data() + length).parent_path() / "data";
		const auto navMesh = (data / "navigation" / "Forest.nav.json").u8string();
		const auto spawnConfig = (data / "spawns" / "Forest.json").u8string();
		result.navMeshFile.assign(navMesh.begin(), navMesh.end());
		result.spawnConfigFile.assign(spawnConfig.begin(), spawnConfig.end());
	}

	return result;
}
