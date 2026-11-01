#include <Pch.h>
#include "ClientPacketHandler.h"
#include "WorldSession.h"

const WorldPacketHandler<WorldSession>& ClientPacketHandler::GetHandlers()
{
	static const auto registeredHandlers = []
	{
		WorldPacketHandler<WorldSession> handlers;
		handlers.Register<RequestPing>(Handle_PKT_CS_PING);
		handlers.Register<RequestPlayerPosition>(Handle_PKT_CS_PLAYER_POSITION);
		handlers.Register<RequestPlayerMove>(Handle_PKT_CS_PLAYER_MOVE);
		handlers.Register<RequestPlayerState>(Handle_PKT_CS_PLAYER_STATE);
		handlers.Register<RequestMeleeHit>(Handle_PKT_CS_MELEE_HIT);
		return handlers;
	}();

	return registeredHandlers;
}

bool ClientPacketHandler::HandlePacket(WorldSession& session, uint16 id, std::span<const BYTE> payload)
{
	return GetHandlers().HandlePacket(session, id, payload);
}

bool ClientPacketHandler::Handle_PKT_CS_PING(WorldSession& session, const RequestPing&)
{
	return session.ReplyPing();
}

bool ClientPacketHandler::Handle_PKT_CS_PLAYER_POSITION(WorldSession& session, const RequestPlayerPosition& packet)
{
	session.UpdatePlayerPosition(packet);
	return true;
}

bool ClientPacketHandler::Handle_PKT_CS_MELEE_HIT(WorldSession& session, const RequestMeleeHit& packet)
{
	session.ReportMeleeHit(packet);
	return true;
}

bool ClientPacketHandler::Handle_PKT_CS_PLAYER_STATE(WorldSession& session, const RequestPlayerState& packet)
{
	session.UpdatePlayerState(packet);
	return true;
}

bool ClientPacketHandler::Handle_PKT_CS_PLAYER_MOVE(WorldSession& session, const RequestPlayerMove& packet)
{
	session.UpdatePlayerMove(packet);
	return true;
}
