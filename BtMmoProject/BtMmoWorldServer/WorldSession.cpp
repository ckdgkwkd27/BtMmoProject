#include <Pch.h>
#include "WorldSession.h"
#include "ClientPacketHandler.h"
#include "GameWorld.h"
#include "WorldMetrics.h"
#include <WorldPacketFactory.h>

WorldSession::WorldSession(CoreRuntime& runtime, std::shared_ptr<GameWorld> _world,
                           uint64 _id, SessionOptions options, std::shared_ptr<WorldMetrics> _metrics)
    : PacketSession(runtime, options), world(std::move(_world)), metrics(std::move(_metrics)), id(_id)
{
}

void WorldSession::OnConnected()
{
	world->EnterWorld(id, GetSessionPtr());
}

bool WorldSession::OnRecvPacket(uint16 _id, std::span<const BYTE> payload)
{
	const auto now = ::GetTickCount64();
	if (now - rateWindow >= 1000)
	{
		rateWindow = now;
		packetsInWindow = 0;
	}

	return ++packetsInWindow <= 200 && ClientPacketHandler::HandlePacket(*this, _id, payload);
}

bool WorldSession::ReplyPing()
{
	return Send(WorldPacketFactory::MakeSendBuffer(ReturnPong{}));
}

void WorldSession::OnSend(uint32 bytes)
{
	if (metrics)
	{
		metrics->RecordSentBytes(bytes);
	}
}

void WorldSession::OnDisconnected(DWORD)
{
	world->LeaveWorld(id);
}

void WorldSession::UpdatePlayerPosition(RequestPlayerPosition position) const
{
	world->UpdatePlayerPosition(id, position);
}

void WorldSession::ReportMeleeHit(RequestMeleeHit hit) const
{
	world->ReportMeleeHit(id, hit);
}

void WorldSession::UpdatePlayerState(RequestPlayerState _state) const
{
	world->UpdatePlayerState(id, _state);
}

void WorldSession::UpdatePlayerMove(RequestPlayerMove movement) const
{
	world->UpdatePlayerMove(id, movement);
}
