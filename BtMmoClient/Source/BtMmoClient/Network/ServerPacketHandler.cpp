#include "ServerPacketHandler.h"
#include "BtWorldSubsystem.h"

const WorldPacketHandler<UBtWorldSubsystem>& FServerPacketHandler::GetHandlers()
{
    static const auto RegisteredHandlers = []
    {
        WorldPacketHandler<UBtWorldSubsystem> Handlers;
        Handlers.Register<ReturnWelcome>(Handle_PKT_SC_WELCOME);
        Handlers.Register<NotifySpawner>(Handle_PKT_SC_SPAWNER);
        Handlers.Register<ReturnPong>(Handle_PKT_SC_PONG);
        Handlers.Register<NotifyMonsterSpawn>(Handle_PKT_SC_MONSTER_SPAWN);
        Handlers.Register<NotifyMonsterDespawn>(Handle_PKT_SC_MONSTER_DESPAWN);
        Handlers.Register<NotifyMonsterMove>(Handle_PKT_SC_MONSTER_MOVE);
        Handlers.Register<NotifyMonsterState>(Handle_PKT_SC_MONSTER_STATE);
        Handlers.Register<NotifyPlayerHealth>(Handle_PKT_SC_PLAYER_HEALTH);
        Handlers.Register<NotifyPlayerSpawn>(Handle_PKT_SC_PLAYER_SPAWN);
        Handlers.Register<NotifyPlayerMove>(Handle_PKT_SC_PLAYER_MOVE);
        Handlers.Register<NotifyPlayerState>(Handle_PKT_SC_PLAYER_STATE);
        Handlers.Register<NotifyPlayerDespawn>(Handle_PKT_SC_PLAYER_DESPAWN);

        Handlers.Register<NotifyMonsterHealth>(Handle_PKT_SC_MONSTER_HEALTH);
        return Handlers;
    }();

    return RegisteredHandlers;
}

bool FServerPacketHandler::HandlePacket(UBtWorldSubsystem& Session, uint16 Id, std::span<const uint8> Payload)
{
    return GetHandlers().HandlePacket(Session, Id, Payload);
}

bool FServerPacketHandler::Handle_PKT_SC_WELCOME(UBtWorldSubsystem& Session, const ReturnWelcome& Packet)
{
    return Session.OnWelcome(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_SPAWNER(UBtWorldSubsystem& Session, const NotifySpawner& Packet)
{
    return Session.OnSpawner(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_PONG(UBtWorldSubsystem& Session, const ReturnPong& Packet)
{
    return Session.OnPong(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_MONSTER_SPAWN(UBtWorldSubsystem& Session, const NotifyMonsterSpawn& Packet)
{
    return Session.OnMonsterSpawn(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_MONSTER_DESPAWN(UBtWorldSubsystem& Session, const NotifyMonsterDespawn& Packet)
{
    return Session.OnMonsterDespawn(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_MONSTER_STATE(UBtWorldSubsystem& Session, const NotifyMonsterState& Packet)
{
    return Session.OnMonsterState(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_PLAYER_HEALTH(UBtWorldSubsystem& Session, const NotifyPlayerHealth& Packet)
{
    return Session.OnPlayerHealth(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_MONSTER_HEALTH(UBtWorldSubsystem& Session, const NotifyMonsterHealth& Packet)
{
    return Session.OnMonsterHealth(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_PLAYER_SPAWN(UBtWorldSubsystem& Session, const NotifyPlayerSpawn& Packet)
{
    return Session.OnPlayerSpawn(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_PLAYER_STATE(UBtWorldSubsystem& Session, const NotifyPlayerState& Packet)
{
    return Session.OnPlayerState(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_PLAYER_DESPAWN(UBtWorldSubsystem& Session, const NotifyPlayerDespawn& Packet)
{
    return Session.OnPlayerDespawn(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_PLAYER_MOVE(UBtWorldSubsystem& Session, const NotifyPlayerMove& Packet)
{
    return Session.OnPlayerMove(Packet);
}

bool FServerPacketHandler::Handle_PKT_SC_MONSTER_MOVE(UBtWorldSubsystem& Session, const NotifyMonsterMove& Packet)
{
    return Session.OnMonsterMove(Packet);
}
