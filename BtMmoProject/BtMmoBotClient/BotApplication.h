#pragma once
#include "BotOptions.h"
#include <NetworkCore/CoreRuntime.h>
#include <NetworkCore/ClientService.h>

class BotScenario;
class BotApplication
{
public:
	explicit BotApplication(BotOptions _options);
	int Run();
private:
	void Stop();
	BotOptions options;
	CoreRuntime runtime;
	std::shared_ptr<BotScenario> scenario;
	ClientServicePtr service;
	unsigned nextIndex{0};
	bool stopped{false};
};
