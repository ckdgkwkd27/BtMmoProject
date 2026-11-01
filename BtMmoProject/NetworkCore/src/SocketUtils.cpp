#include <Pch.h>
#include <NetworkCore/SocketUtils.h>

static std::mutex runtimeMutex;
static unsigned runtimeUsers = 0;

LPFN_CONNECTEX SocketUtils::ConnectEx = nullptr;
LPFN_DISCONNECTEX SocketUtils::DisconnectEx = nullptr;
LPFN_ACCEPTEX SocketUtils::AcceptEx = nullptr;

bool SocketUtils::Init()
{
	std::scoped_lock lock(runtimeMutex);
	if (runtimeUsers != 0)
	{
		++runtimeUsers;
		return true;
	}

	WSADATA data{};
	const int error = ::WSAStartup(MAKEWORD(2, 2), &data);
	if (error != 0)
	{
		return false;
	}

	SOCKET socket = CreateSocket();
	const bool ready = socket != INVALID_SOCKET &&
	                   BindWindowsFunction(socket, WSAID_CONNECTEX, reinterpret_cast<LPVOID*>(&ConnectEx)) &&
	                   BindWindowsFunction(socket, WSAID_DISCONNECTEX, reinterpret_cast<LPVOID*>(&DisconnectEx)) &&
	                   BindWindowsFunction(socket, WSAID_ACCEPTEX, reinterpret_cast<LPVOID*>(&AcceptEx));
	Close(socket);
	if (!ready)
	{
		::WSACleanup();
		return false;
	}

	runtimeUsers = 1;
	return true;
}

void SocketUtils::Clear()
{
	std::scoped_lock lock(runtimeMutex);
	if (runtimeUsers != 0 && --runtimeUsers == 0)
	{
		::WSACleanup();
	}
}

bool SocketUtils::BindWindowsFunction(SOCKET socket, GUID guid, LPVOID* function)
{
	DWORD bytes = 0;
	return ::WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
	                  function, sizeof(*function), &bytes, nullptr, nullptr) != SOCKET_ERROR;
}

SOCKET SocketUtils::CreateSocket()
{
	return ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
}

bool SocketUtils::SetTcpNoDelay(SOCKET socket, bool flag)
{
	const BOOL value = flag;
	return ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}

bool SocketUtils::SetUpdateAcceptSocket(SOCKET socket, SOCKET listenSocket)
{
	return ::setsockopt(socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
	                    reinterpret_cast<const char*>(&listenSocket), sizeof(listenSocket)) == 0;
}

bool SocketUtils::SetUpdateConnectSocket(SOCKET socket)
{
	return ::setsockopt(socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, nullptr, 0) == 0;
}

bool SocketUtils::Bind(SOCKET socket, const NetAddress& address)
{
	return ::bind(socket, reinterpret_cast<const SOCKADDR*>(&address.GetSockAddr()), sizeof(SOCKADDR_IN)) == 0;
}

bool SocketUtils::BindAnyAddress(SOCKET socket, uint16 port)
{
	return Bind(socket, NetAddress("0.0.0.0", port));
}

bool SocketUtils::Listen(SOCKET socket, int32 backlog)
{
	return ::listen(socket, backlog) == 0;
}

void SocketUtils::Close(SOCKET& socket)
{
	if (socket != INVALID_SOCKET)
	{
		::closesocket(socket);
	}

	socket = INVALID_SOCKET;
}
