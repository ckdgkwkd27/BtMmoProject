#pragma once
#include "Service.h"
#include <boost/thread/future.hpp>

class ClientService final : public Service
{
public:
	using ConnectHandler = std::function<void(SessionPtr, boost::future<DWORD>)>;
	ClientService(NetAddress address, CoreRuntime& runtime, SessionFactory factory,
	              std::size_t maxSessions, ConnectHandler handler);
	[[nodiscard]] Result Start() override;
private:
	ConnectHandler onConnect;
};
