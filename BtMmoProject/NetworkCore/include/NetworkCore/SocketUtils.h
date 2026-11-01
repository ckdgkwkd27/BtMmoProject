#pragma once
#include "NetAddress.h"

class SocketUtils
{
public:
	static LPFN_CONNECTEX ConnectEx;
	static LPFN_DISCONNECTEX DisconnectEx;
	static LPFN_ACCEPTEX AcceptEx;
	[[nodiscard]] static bool Init();
	static void Clear();
	static bool BindWindowsFunction(SOCKET socket, GUID guid, LPVOID* function);
	static SOCKET CreateSocket();
	static bool SetTcpNoDelay(SOCKET socket, bool flag);
	static bool SetUpdateAcceptSocket(SOCKET socket, SOCKET listenSocket);
	static bool SetUpdateConnectSocket(SOCKET socket);
	static bool Bind(SOCKET socket, const NetAddress& address);
	static bool BindAnyAddress(SOCKET socket, uint16 port);
	static bool Listen(SOCKET socket, int32 backlog = SOMAXCONN);
	static void Close(SOCKET& socket);
};
