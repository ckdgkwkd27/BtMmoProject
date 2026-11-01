#pragma once
#include <WorldPacketHandler.h>

class BotSession;
class ServerPacketHandler
{
public:
	static bool HandlePacket(BotSession& session, uint16 id, std::span<const BYTE> payload);
private:
	static bool Handle_PKT_SC_PLAYER_DESPAWN(BotSession& session, const NotifyPlayerDespawn& packet);
	static bool Handle_PKT_SC_PLAYER_MOVE(BotSession& Session, const NotifyPlayerMove& Packet);
	static bool Handle_PKT_SC_PLAYER_STATE(BotSession& session, const NotifyPlayerState& packet);
	static bool Handle_PKT_SC_PLAYER_SPAWN(BotSession& session, const NotifyPlayerSpawn& packet);
	static const WorldPacketHandler<BotSession>& GetHandlers();
	static bool Handle_PKT_SC_WELCOME(BotSession& session, const ReturnWelcome& packet);
	static bool Handle_PKT_SC_SPAWNER(BotSession& session, const NotifySpawner& packet);
	static bool Handle_PKT_SC_PONG(BotSession& session, const ReturnPong& packet);
	static bool Handle_PKT_SC_MONSTER_SPAWN(BotSession& session, const NotifyMonsterSpawn& packet);
	static bool Handle_PKT_SC_MONSTER_DESPAWN(BotSession& session, const NotifyMonsterDespawn& packet);
	static bool Handle_PKT_SC_MONSTER_MOVE(BotSession& Session, const NotifyMonsterMove& Packet);
	static bool Handle_PKT_SC_MONSTER_STATE(BotSession& session, const NotifyMonsterState& packet);
	static bool Handle_PKT_SC_PLAYER_HEALTH(BotSession& session, const NotifyPlayerHealth& packet);
	static bool Handle_PKT_SC_MONSTER_HEALTH(BotSession& session, const NotifyMonsterHealth& packet);
};
