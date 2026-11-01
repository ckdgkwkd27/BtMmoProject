#include <Pch.h>
#include <NetworkCore/Packet.h>
#include <NetworkCore/BufferWriter.h>

std::expected<PacketHeader, std::string> DecodeHeader(std::span<const BYTE> data)
{
	if (data.size() != PACKET_HEADER_SIZE)
	{
		return std::unexpected("Invalid header length");
	}

	const auto size = static_cast<uint16>((data[0] << 8) | data[1]);
	const auto id = static_cast<uint16>((data[2] << 8) | data[3]);
	if (size < PACKET_HEADER_SIZE || size > MAX_PACKET_SIZE)
	{
		return std::unexpected("Invalid packet size");
	}

	return PacketHeader{size, id};
}

SendBufferPtr MakePacket(uint16 id, std::span<const BYTE> payload)
{
	if (payload.size() > MAX_PACKET_SIZE - PACKET_HEADER_SIZE)
	{
		return {};
	}

	BufferWriter writer;
	if (!writer.U16(static_cast<uint16>(PACKET_HEADER_SIZE + payload.size())) || !writer.U16(id) ||
	    !writer.Write(payload.data(), payload.size()))
	{
		return {};
	}

	auto result = std::make_shared<SendBuffer>(static_cast<int32>(writer.Data().size()));
	if (!result->CopyData(writer.Data().data(), static_cast<int32>(writer.Data().size())))
	{
		return {};
	}

	return result;
}
