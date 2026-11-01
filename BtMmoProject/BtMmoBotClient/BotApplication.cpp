#include <Pch.h>
#include "BotApplication.h"
#include "BotScenario.h"
#include "BotSession.h"
#include <ConsoleStopHandler.h>

BotApplication::BotApplication(BotOptions _options) : options(std::move(_options))
{
}

int BotApplication::Run()
{
	ConsoleStopHandler stopHandler;
	if (!stopHandler.IsValid())
	{
		std::cerr << "Bot error: Registering console stop handler failed\n";
		return 1;
	}

	scenario = std::make_shared<BotScenario>(runtime, options);
	auto completion = scenario->GetCompletion();
	service = std::make_shared<ClientService>(
	    NetAddress(options.host, static_cast<uint16>(options.port)), runtime,
	    [this]
	{
		const auto index = nextIndex++;
		auto session = std::make_shared<BotSession>(runtime, scenario, index);
		scenario->AddBot(session);
		return session;
	}, options.clients,
	    [](SessionPtr session, boost::future<DWORD> connected)
	{
		std::static_pointer_cast<BotSession>(session)->AttachConnection(std::move(connected));
	});

	auto started = service->Start();
	if (started)
	{
		scenario->Start();
		started = runtime.Start(options.threads);
	}

	if (!started)
	{
		Stop();
		std::cerr << "Bot error: " << started.error() << '\n';
		return 1;
	}

	while (completion.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
	{
		if (stopHandler.IsStopRequested() || runtime.HasFailure())
		{
			scenario->RequestStop("Interrupted or worker failure");
			break;
		}
	}

	Stop();
	if (runtime.HasFailure())
	{
		std::cerr << "Bot error: " << runtime.GetFailure() << '\n';
		return 1;
	}

	scenario->Report();
	return completion.get() ? 0 : 1;
}

void BotApplication::Stop()
{
	if (stopped)
	{
		return;
	}

	stopped = true;
	if (scenario)
	{
		scenario->RequestStop("Application stopped");
	}

	if (service)
	{
		service->CloseService();
	}

	runtime.Shutdown();
}
