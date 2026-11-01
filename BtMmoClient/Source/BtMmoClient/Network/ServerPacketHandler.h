#pragma once
#include "CoreMinimal.h"
#include "WorldPacketHandler.h"

class UBtWorldSubsystem;

class FServerPacketHandler
{
public:
    static bool HandlePacket(UBtWorldSubsystem& Session, uint16 Id, std::span<const uint8> Payload);
private:
    static bool Handle_PKT_SC_PLAYER_DESPAWN(UBtWorldSubsystem& Session, const NotifyPlayerDespawn& Packet);
    static bool Handle_PKT_SC_PLAYER_MOVE(UBtWorldSubsystem& Session, const NotifyPlayerMove& Packet);
    static bool Handle_PKT_SC_PLAYER_STATE(UBtWorldSubsystem& Session, const NotifyPlayerState& Packet);
    static bool Handle_PKT_SC_PLAYER_SPAWN(UBtWorldSubsystem& Session, const NotifyPlayerSpawn& Packet);
    static const WorldPacketHandler<UBtWorldSubsystem>& GetHandlers();
    static bool Handle_PKT_SC_WELCOME(UBtWorldSubsystem& Session, const ReturnWelcome& Packet);
    static bool Handle_PKT_SC_SPAWNER(UBtWorldSubsystem& Session, const NotifySpawner& Packet);
    static bool Handle_PKT_SC_PONG(UBtWorldSubsystem& Session, const ReturnPong& Packet);
    static bool Handle_PKT_SC_MONSTER_SPAWN(UBtWorldSubsystem& Session, const NotifyMonsterSpawn& Packet);
    static bool Handle_PKT_SC_MONSTER_DESPAWN(UBtWorldSubsystem& Session, const NotifyMonsterDespawn& Packet);
    static bool Handle_PKT_SC_MONSTER_MOVE(UBtWorldSubsystem& Session, const NotifyMonsterMove& Packet);
    static bool Handle_PKT_SC_MONSTER_STATE(UBtWorldSubsystem& Session, const NotifyMonsterState& Packet);
    static bool Handle_PKT_SC_PLAYER_HEALTH(UBtWorldSubsystem& Session, const NotifyPlayerHealth& Packet);
    static bool Handle_PKT_SC_MONSTER_HEALTH(UBtWorldSubsystem& Session, const NotifyMonsterHealth& Packet);
};
