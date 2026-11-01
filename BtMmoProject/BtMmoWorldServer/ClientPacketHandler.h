#pragma once
#include <WorldPacketHandler.h>

class WorldSession;
class ClientPacketHandler
{
public:
	static bool HandlePacket(WorldSession& session, uint16 id, std::span<const BYTE> payload);
private:
	static const WorldPacketHandler<WorldSession>& GetHandlers();
	static bool Handle_PKT_CS_PING(WorldSession& session, const RequestPing& packet);
	static bool Handle_PKT_CS_PLAYER_POSITION(WorldSession& session, const RequestPlayerPosition& packet);
	static bool Handle_PKT_CS_PLAYER_MOVE(WorldSession& session, const RequestPlayerMove& packet);
	static bool Handle_PKT_CS_PLAYER_STATE(WorldSession& session, const RequestPlayerState& packet);
	static bool Handle_PKT_CS_MELEE_HIT(WorldSession& session, const RequestMeleeHit& packet);
};
