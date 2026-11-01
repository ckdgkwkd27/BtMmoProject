#pragma once
#include "WorldProtocol.h"
#include <functional>
#include <unordered_map>
#include <utility>

template <typename SessionType>
class WorldPacketHandler
{
public:
	template <typename PacketType, typename Handler>
	void Register(Handler handler)
	{
		handlers.emplace(static_cast<uint16>(PacketType::ID), [handler = std::move(handler)](SessionType& session, std::span<const BYTE> payload) mutable
		{
			PacketType packet;
			if (!WorldProtocol::Decode(payload, packet))
			{
				return false;
			}

			return handler(session, std::as_const(packet));
		});
	}

	bool HandlePacket(SessionType& session, uint16 id, std::span<const BYTE> payload) const
	{
		const auto found = handlers.find(id);
		if (found == handlers.end())
		{
			return false;
		}

		return found->second(session, payload);
	}

private:
	using HandlerFunc = std::function<bool(SessionType&, std::span<const BYTE>)>;
	std::unordered_map<uint16, HandlerFunc> handlers;
};
