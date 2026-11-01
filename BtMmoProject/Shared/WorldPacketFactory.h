#pragma once
#include "WorldProtocol.h"
#include <NetworkCore/SendBuffer.h>

class WorldPacketFactory
{
public:
	template <typename PacketType>
	static SendBufferPtr MakeSendBuffer(const PacketType& packet)
	{
		const auto bytes = WorldProtocol::MakePacket(packet);
		if (!bytes)
		{
			return {};
		}

		auto buffer = std::make_shared<SendBuffer>(static_cast<int32>(bytes->size()));
		if (not buffer->CopyData(bytes->data(), static_cast<int32>(bytes->size())))
		{
			return {};
		}

		return buffer;
	}
};
