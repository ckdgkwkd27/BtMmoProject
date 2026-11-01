#include <Pch.h>
#include <NetworkCore/ClientService.h>
#include <NetworkCore/Session.h>

ClientService::ClientService(NetAddress address, CoreRuntime& runtime, SessionFactory factory,
                             std::size_t maxSessions, ConnectHandler handler)
    : Service(ServiceType::CLIENT, address, runtime, std::move(factory), maxSessions), onConnect(std::move(handler))
{
}

Result ClientService::Start()
{
	if (!onConnect)
	{
		return std::unexpected("ClientService requires a connect handler");
	}

	if (auto result = BeginStart(); !result)
	{
		return result;
	}

	for (std::size_t i = 0; i < GetMaxSessionCount(); ++i)
	{
		auto session = CreateSession();
		if (!session || !AddSession(session))
		{
			CloseService();
			return std::unexpected("Creating or registering client session failed");
		}

		onConnect(session, session->Connect());
	}

	return {};
}
