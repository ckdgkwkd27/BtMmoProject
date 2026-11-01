#pragma once
#include "Platform.h"
#include "Types.h"

class NetAddress
{
public:
	NetAddress() = default;
	explicit NetAddress(SOCKADDR_IN address);
	NetAddress(const std::string& ip, uint16 port);
	const SOCKADDR_IN& GetSockAddr() const
	{
		return sockAddr;
	}

	std::string GetIpAddress() const;
	uint16 GetPort() const
	{
		return ::ntohs(sockAddr.sin_port);
	}

	bool IsValid() const
	{
		return sockAddr.sin_family == AF_INET;
	}
private:
	SOCKADDR_IN sockAddr{};
};
