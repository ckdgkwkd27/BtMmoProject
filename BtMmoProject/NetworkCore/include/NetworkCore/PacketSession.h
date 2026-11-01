#pragma once
#include "Session.h"
#include "Packet.h"

class PacketSession : public Session
{
public:
	using Session::Session;
protected:
	int32 OnRecv(BYTE* buffer, int32 length) final;
	virtual bool OnRecvPacket(uint16 id, std::span<const BYTE> payload) = 0;
};
