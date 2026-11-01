#include <Pch.h>
#include "ServerPacketHandler.h"
#include "BotSession.h"

const WorldPacketHandler<BotSession>& ServerPacketHandler::GetHandlers()
{
	static const auto registeredHandlers = []
	{
		WorldPacketHandler<BotSession> handlers;
		handlers.Register<ReturnWelcome>(Handle_PKT_SC_WELCOME);
		handlers.Register<NotifySpawner>(Handle_PKT_SC_SPAWNER);
		handlers.Register<ReturnPong>(Handle_PKT_SC_PONG);
		handlers.Register<NotifyMonsterSpawn>(Handle_PKT_SC_MONSTER_SPAWN);
		handlers.Register<NotifyMonsterDespawn>(Handle_PKT_SC_MONSTER_DESPAWN);
		handlers.Register<NotifyMonsterMove>(Handle_PKT_SC_MONSTER_MOVE);
		handlers.Register<NotifyMonsterState>(Handle_PKT_SC_MONSTER_STATE);
		handlers.Register<NotifyPlayerHealth>(Handle_PKT_SC_PLAYER_HEALTH);
		handlers.Register<NotifyPlayerSpawn>(Handle_PKT_SC_PLAYER_SPAWN);
		handlers.Register<NotifyPlayerMove>(Handle_PKT_SC_PLAYER_MOVE);
		handlers.Register<NotifyPlayerState>(Handle_PKT_SC_PLAYER_STATE);
		handlers.Register<NotifyPlayerDespawn>(Handle_PKT_SC_PLAYER_DESPAWN);

		handlers.Register<NotifyMonsterHealth>(Handle_PKT_SC_MONSTER_HEALTH);
		return handlers;
	}();

	return registeredHandlers;
}

bool ServerPacketHandler::HandlePacket(BotSession& session, uint16 id, std::span<const BYTE> payload)
{
	return GetHandlers().HandlePacket(session, id, payload);
}

bool ServerPacketHandler::Handle_PKT_SC_WELCOME(BotSession& session, const ReturnWelcome& packet)
{
	return session.OnWelcome(packet);
}

bool ServerPacketHandler::Handle_PKT_SC_SPAWNER(BotSession& session, const NotifySpawner& packet)
{
	return session.OnSpawner(packet);
}

bool ServerPacketHandler::Handle_PKT_SC_PONG(BotSession& session, const ReturnPong&)
{
	return session.IsWelcomed();
}

bool ServerPacketHandler::Handle_PKT_SC_MONSTER_SPAWN(BotSession& session, const NotifyMonsterSpawn&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_MONSTER_DESPAWN(BotSession& session, const NotifyMonsterDespawn&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_MONSTER_STATE(BotSession& session, const NotifyMonsterState&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_PLAYER_HEALTH(BotSession& session, const NotifyPlayerHealth&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_MONSTER_HEALTH(BotSession& session, const NotifyMonsterHealth&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_PLAYER_SPAWN(BotSession& session, const NotifyPlayerSpawn&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_PLAYER_STATE(BotSession& session, const NotifyPlayerState&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_PLAYER_DESPAWN(BotSession& session, const NotifyPlayerDespawn&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_PLAYER_MOVE(BotSession& session, const NotifyPlayerMove&)
{
	return session.IsSnapshotComplete();
}

bool ServerPacketHandler::Handle_PKT_SC_MONSTER_MOVE(BotSession& session, const NotifyMonsterMove&)
{
	return session.IsSnapshotComplete();
}
