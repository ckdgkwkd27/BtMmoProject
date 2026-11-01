#pragma once
#include <NetworkCore/Session.h>
#include <Types.h>

class ServerOptions
{
public:
	static std::expected<ServerOptions, std::string> Parse(int argc, wchar_t** argv);
	std::string bindAddress{"127.0.0.1"};
	uint16 port{17777};
	std::string metricsBindAddress{"127.0.0.1"};
	uint16 metricsPort{9108};
	uint32 threads{std::thread::hardware_concurrency()};
	std::size_t maxClients{10000};
	uint32 tileSize{2000};
	uint32 durationSeconds{0};
	std::string navMeshFile;
	std::string spawnConfigFile;
	SessionOptions session;
};
