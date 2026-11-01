#include <Pch.h>
#include <NetworkCore/PacketSession.h>

int32 PacketSession::OnRecv(BYTE* buffer, int32 length)
{
	int32 processLength = 0;
	while (IsConnected())
	{
		const int32 dataSize = length - processLength;
		if (dataSize < static_cast<int32>(PACKET_HEADER_SIZE))
		{
			break;
		}

		const auto header = DecodeHeader({buffer + processLength, PACKET_HEADER_SIZE});
		if (!header)
		{
			return -1;
		}

		if (dataSize < header->size)
		{
			break;
		}

		if (!OnRecvPacket(header->id, {buffer + processLength + PACKET_HEADER_SIZE, header->size - PACKET_HEADER_SIZE}))
		{
			return -1;
		}

		processLength += header->size;
	}

	return processLength;
}
