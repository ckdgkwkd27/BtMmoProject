#pragma once
#include "SendBuffer.h"

inline constexpr std::size_t PACKET_HEADER_SIZE = 4;
inline constexpr std::size_t MAX_PACKET_SIZE = 8192;
struct PacketHeader
{
	uint16 size;
	uint16 id;
};

std::expected<PacketHeader, std::string> DecodeHeader(std::span<const BYTE> data);
SendBufferPtr MakePacket(uint16 id, std::span<const BYTE> payload);
