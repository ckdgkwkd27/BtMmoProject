#include <Pch.h>
#include "BotSession.h"
#include "ServerPacketHandler.h"

BotSession::BotSession(CoreRuntime& runtime, std::weak_ptr<IBotObserver> _observer, unsigned _index)
    : PacketSession(runtime), observer(std::move(_observer)), index(_index)
{
}

void BotSession::AttachConnection(boost::future<DWORD> connection)
{
	auto weak = std::weak_ptr(std::static_pointer_cast<BotSession>(GetSessionPtr()));
	continuation = connection.then(boost::launch::sync, [weak](boost::future<DWORD> connected)
	{
		const DWORD error = connected.get();
		if (auto self = weak.lock(); self && error != ERROR_SUCCESS)
		{
			if (auto _observer = self->observer.lock())
			{
				_observer->OnError("Connect/then: Native connect error " + std::to_string(error));
			}
		}
	});
}

void BotSession::OnConnected()
{
	wasConnected = true;
}

bool BotSession::OnRecvPacket(uint16 id, std::span<const BYTE> payload)
{
	return ServerPacketHandler::HandlePacket(*this, id, payload);
}

bool BotSession::OnWelcome(ReturnWelcome info)
{
	if (!snapshot.Accept(info))
	{
		return false;
	}

	if (auto _observer = observer.lock())
	{
		_observer->OnWelcome(index, info);
		return true;
	}

	return false;
}

bool BotSession::OnSpawner(NotifySpawner info)
{
	if (!snapshot.Accept(info))
	{
		return false;
	}

	if (auto _observer = observer.lock())
	{
		_observer->OnSpawner(index, info);
		return true;
	}

	return false;
}

void BotSession::OnDisconnected(DWORD error)
{
	if (!wasConnected)
	{
		return; // AttachConnection reports failures before the connection is established.
	}

	if (auto _observer = observer.lock())
	{
		_observer->OnError("Client " + std::to_string(index + 1) + " disconnected (I/O:" + std::to_string(error) + ')');
	}
}
