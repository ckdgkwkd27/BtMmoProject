#include <Pch.h>
#include <NetworkCore/NetAddress.h>

NetAddress::NetAddress(SOCKADDR_IN address) : sockAddr(address)
{
}

NetAddress::NetAddress(const std::string& ip, uint16 port)
{
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = ::htons(port);
	if (::InetPtonA(AF_INET, ip.c_str(), &sockAddr.sin_addr) != 1)
	{
		sockAddr = {};
	}
}

std::string NetAddress::GetIpAddress() const
{
	char buffer[INET_ADDRSTRLEN]{};
	if (!IsValid() || ::InetNtopA(AF_INET, &sockAddr.sin_addr, buffer, sizeof(buffer)) == nullptr)
	{
		return {};
	}

	return buffer;
}
