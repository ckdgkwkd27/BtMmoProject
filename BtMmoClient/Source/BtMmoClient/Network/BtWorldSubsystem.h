#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldProtocol.h"
#include "BtWorldSubsystem.generated.h"

class FServerPacketHandler;
class FSocket;
class ABtMonster;
class UBtMonsterCatalog;
class APawn;
class ABtMmoClientCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBtPlayerHealthChanged, int32, Health, int32, MaxHealth);

UCLASS(Config=Game)
class BTMMOCLIENT_API UBtWorldSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UPROPERTY(BlueprintReadOnly, Category="World|Player")
    int32 PlayerHealth = 100;
    UPROPERTY(BlueprintReadOnly, Category="World|Player")
    int32 PlayerMaxHealth = 100;
    UPROPERTY(BlueprintAssignable, Category="World|Player")
    FBtPlayerHealthChanged OnPlayerHealthChanged;
    uint64 BeginMeleeAttack(APawn* Attacker);
    bool ReportMeleeHit(APawn* Attacker, uint64 AttackSequence, ABtMonster* Monster);
    uint64 GetLocalPlayerId() const { return LocalPlayerId; }

    ABtMmoClientCharacter* FindRemotePlayer(uint64 Id) const;
    int32 GetRemotePlayerCount() const { return RemotePlayerActors.Num(); }
private:
    friend class FServerPacketHandler;
    bool OnWelcome(const ReturnWelcome& Info);
    bool OnSpawner(const NotifySpawner& Info);
    bool OnPong(const ReturnPong& Info);
    bool OnMonsterSpawn(const NotifyMonsterSpawn& Info);
    bool OnMonsterDespawn(const NotifyMonsterDespawn& Info);
    bool OnMonsterMove(const NotifyMonsterMove& Info);
    bool OnMonsterState(const NotifyMonsterState& Info);
    bool OnPlayerHealth(const NotifyPlayerHealth& Info);
    bool OnMonsterHealth(const NotifyMonsterHealth& Info);
    bool OnPlayerSpawn(const NotifyPlayerSpawn& Info);
    bool OnPlayerMove(const NotifyPlayerMove& Info);
    bool OnPlayerState(const NotifyPlayerState& Info);
    bool OnPlayerDespawn(const NotifyPlayerDespawn& Info);

    bool Tick(float DeltaTime);
    void Connect();
    void Close();
    void Retry(const TCHAR* Reason);
    bool ReceivePackets();
    template <typename PacketType>
    bool QueuePacket(const PacketType& Packet)
    {
        const auto Bytes = WorldProtocol::MakePacket(Packet);
        if (!Bytes) return false;
        SendBuffer.Append(Bytes->data(), static_cast<int32>(Bytes->size()));
        return true;
    }

    void SendPlayerMove();
    void FlushSendBuffer();
    void SynchronizeMonsters();
    void ClearMonsterActors();
    void SynchronizePlayers();
    void ClearPlayerActors();

    UPROPERTY(Config)
    FString ServerHost = TEXT("127.0.0.1");
    UPROPERTY(Config)
    int32 ServerPort = 17777;

    UPROPERTY(Config)
    TSoftObjectPtr<UBtMonsterCatalog> MonsterCatalogAsset;
    UPROPERTY(Transient)
    TObjectPtr<UBtMonsterCatalog> MonsterCatalog;
    bool bCatalogLoadAttempted = false;
    TSet<uint32> MissingMonsterTemplates;

    FSocket* Socket = nullptr;
    FTSTicker::FDelegateHandle TickHandle;
    TArray<uint8> ReceiveBuffer;
    WorldSnapshotState Snapshot;
    TMap<uint64, NotifyMonsterSpawn> MonsterStates;
    TMap<uint64, NotifyMonsterState> MonsterBehaviorStates;
    TMap<uint64, NotifyMonsterMove> MonsterMoves;
    TMap<uint64, NotifyMonsterHealth> MonsterHealthStates;
    TMap<uint64, FVector> SpawnerCenters;
    TMap<uint64, TWeakObjectPtr<ABtMonster>> MonsterActors;
    TSet<uint64> PendingMonsterSpawns;
    TWeakObjectPtr<UWorld> MonsterWorld;
    bool bConnected = false;
    double NextConnectTime = 0;
    double ConnectStarted = 0;
    double SnapshotDeadline = 0;
    double LastReceiveTime = 0;
    double NextHeartbeatTime = 0;
    double NextPlayerMoveTime = 0;
    TOptional<RequestPlayerState> LastSentPlayerState;
    TArray<uint8> SendBuffer;
    int32 SendOffset = 0;
    uint64 NextAttackSequence = 0;
    uint64 ActiveAttackSequence = 0;
    uint64 LocalPlayerId = 0;
    TMap<uint64, NotifyPlayerSpawn> RemotePlayerStates;
    TMap<uint64, TWeakObjectPtr<ABtMmoClientCharacter>> RemotePlayerActors;
    TWeakObjectPtr<UWorld> PlayerWorld;
};
