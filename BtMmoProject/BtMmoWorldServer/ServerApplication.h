#pragma once
#include "ServerOptions.h"
#include "NavigationMesh.h"
#include <NetworkCore/CoreRuntime.h>
#include <NetworkCore/ServerService.h>

class GameWorld;
class WorldMetrics;
class ServerApplication
{
public:
	explicit ServerApplication(ServerOptions _options);
	int Run();
private:
	void Stop();
	ServerOptions options;
	std::shared_ptr<NavigationMesh> navigation{std::make_shared<NavigationMesh>()};
	CoreRuntime runtime;
	std::shared_ptr<GameWorld> world;
	ServerServicePtr service;
	std::shared_ptr<WorldMetrics> metrics;
	std::atomic<uint64> nextSessionId{1};
};
