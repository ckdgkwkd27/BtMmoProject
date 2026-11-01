#pragma once
#include "Service.h"

class ServerService final : public Service
{
public:
	ServerService(NetAddress address, CoreRuntime& runtime, SessionFactory factory, std::size_t maxSessions);
	[[nodiscard]] Result Start() override;
	void CloseService() override;
	NetAddress GetListenAddress() const;
private:
	ListenerPtr listener;
};
