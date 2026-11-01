#include <Pch.h>
#include "ServerApplication.h"
#include "GameWorld.h"
#include "WorldSession.h"
#include "WorldMetrics.h"
#include <ConsoleStopHandler.h>
#include <prometheus/exposer.h>

ServerApplication::ServerApplication(ServerOptions _options) : options(std::move(_options))
{
}

int ServerApplication::Run()
{
	if (!options.navMeshFile.empty())
	{
		const std::u8string path(options.navMeshFile.begin(), options.navMeshFile.end());
		const auto loaded = navigation->Load(std::filesystem::path(path));
		if (!loaded)
		{
			std::cerr << "NavMesh error (" << options.navMeshFile << "): " << loaded.error() << '\n';
			return 1;
		}

		std::cout << "NAVMESH map=" << navigation->MapName() << " tiles=" << navigation->TileCount()
		          << " polygons=" << navigation->PolygonCount() << " coordinates=UE-cm\n";
	}

	std::optional<MonsterSpawnConfig> spawnConfig;
	if (!options.spawnConfigFile.empty())
	{
		const std::u8string path(options.spawnConfigFile.begin(), options.spawnConfigFile.end());
		const auto config = MonsterSpawnConfig::Load(std::filesystem::path(path));
		if (!config || config->map != navigation->MapName())
		{
			std::cerr << "Spawn config error (" << options.spawnConfigFile << "): "
			          << (config ? "Map does not match NavMesh" : config.error()) << '\n';
			return 1;
		}

		spawnConfig = *config;
	}

	ConsoleStopHandler stopHandler;
	if (!stopHandler.IsValid())
	{
		std::cerr << "Server error: Registering console stop handler failed\n";
		return 1;
	}

	service = std::make_shared<ServerService>(
	    NetAddress(options.bindAddress, options.port), runtime,
	    [this]
	{
		return std::make_shared<WorldSession>(runtime, world, nextSessionId++, options.session, metrics);
	},
	    options.maxClients);
	std::unique_ptr<prometheus::Exposer> exposer;
	if (options.metricsPort != 0)
	{
		const NetAddress address(options.metricsBindAddress, options.metricsPort);
		if (!address.IsValid())
		{
			std::cerr << "Metrics error: Expected a numeric IPv4 address\n";
			return 1;
		}

		const auto endpoint = address.GetIpAddress() + ':' + std::to_string(options.metricsPort);
		try
		{
			metrics = std::make_shared<WorldMetrics>(service);
			exposer = std::make_unique<prometheus::Exposer>(endpoint, 1);
			exposer->RegisterCollectable(metrics);
		}
		catch (const std::exception& error)
		{
			std::cerr << "Metrics error (" << endpoint << "): " << error.what() << '\n';
			return 1;
		}
		std::cout << "METRICS http://" << endpoint << "/metrics\n";
	}

	world = std::make_shared<GameWorld>(runtime, options.tileSize, navigation, metrics);
	
	auto started = runtime.Start(options.threads);
	if (!started)
	{
		Stop();
		std::cerr << "Server error: " << started.error() << '\n';
		return 1;
	}

	if (spawnConfig)
	{
		const auto spawning = world->StartMonsterSpawning(navigation, *spawnConfig).get();
		if (!spawning)
		{
			std::cerr << "Spawn system error: " << spawning.error() << '\n';
			Stop();
			return 1;
		}

		std::cout << "MONSTER_WORLD map=" << spawnConfig->map << " spawners=" << spawnConfig->spawnerCount
		          << " maxAliveEach=" << spawnConfig->maxAlive << " respawnMs=" << spawnConfig->respawnDelayMs
		          << " seed=" << spawnConfig->seed << '\n';
	}

	started = service->Start();
	if (!started)
	{
		Stop();
		std::cerr << "Server error: " << started.error() << '\n';
		return 1;
	}

	const auto address = service->GetListenAddress();
	std::cout << "LISTENING " << address.GetIpAddress() << ':' << address.GetPort()
	          << " threads=" << options.threads << " transport=WinSock/IOCP Boost=" << BOOST_LIB_VERSION
	          << " tileSize=" << options.tileSize << " interestTiles=3x3" << std::endl;
	const auto startTime = std::chrono::steady_clock::now();
	while (!stopHandler.IsStopRequested() && !runtime.HasFailure())
	{
		if (options.durationSeconds != 0 &&
		    std::chrono::steady_clock::now() - startTime >= std::chrono::seconds(options.durationSeconds))
		{
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	Stop();
	if (runtime.HasFailure())
	{
		std::cerr << "Server error: " << runtime.GetFailure() << '\n';
		return 1;
	}

	std::cout << "Server stopped cleanly\n";
	return 0;
}

void ServerApplication::Stop()
{
	if (service)
	{
		service->CloseService();
	}

	if (world)
	{
		world->StopMonsterSpawning();
	}

	runtime.Shutdown();
}
