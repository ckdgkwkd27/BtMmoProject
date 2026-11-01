#include <Pch.h>
#include <NetworkCore/ServerService.h>
#include <NetworkCore/Listener.h>

ServerService::ServerService(NetAddress address, CoreRuntime& runtime, SessionFactory factory, std::size_t maxSessions)
    : Service(ServiceType::SERVER, address, runtime, std::move(factory), maxSessions)
{
}

Result ServerService::Start()
{
	if (auto result = BeginStart(); !result)
	{
		return result;
	}

	listener = std::make_shared<Listener>(GetRuntime());
	auto result = listener->StartAccept(std::static_pointer_cast<ServerService>(shared_from_this()));
	if (!result)
	{
		CloseService();
	}

	return result;
}

void ServerService::CloseService()
{
	Service::CloseService();
	if (listener)
	{
		listener->CloseSocket();
	}
}

NetAddress ServerService::GetListenAddress() const
{
	return listener ? listener->GetAddress() : NetAddress{};
}
