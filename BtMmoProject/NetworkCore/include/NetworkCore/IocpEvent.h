#pragma once
#include "Platform.h"
#include "Types.h"

enum class EventType
{
	CONNECT,
	DISCONNECT,
	ACCEPT,
	RECV,
	SEND
};

class IocpEvent : public OVERLAPPED
{
public:
	explicit IocpEvent(EventType type);
	void Init();
	EventType eventType;
	IocpObjectPtr owner;
};

class ConnectEvent : public IocpEvent
{
public:
	ConnectEvent() : IocpEvent(EventType::CONNECT)
	{
	}
};

class DisconnectEvent : public IocpEvent
{
public:
	DisconnectEvent() : IocpEvent(EventType::DISCONNECT)
	{
	}
};

class AcceptEvent : public IocpEvent
{
public:
	AcceptEvent() : IocpEvent(EventType::ACCEPT)
	{
	}

	SessionPtr session;
	std::array<BYTE, 2 * (sizeof(SOCKADDR_IN) + 16)> addresses{};
};

class RecvEvent : public IocpEvent
{
public:
	RecvEvent() : IocpEvent(EventType::RECV)
	{
	}
};

class SendEvent : public IocpEvent
{
public:
	SendEvent() : IocpEvent(EventType::SEND)
	{
	}
};
