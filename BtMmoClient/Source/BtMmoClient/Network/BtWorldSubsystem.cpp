#include "BtWorldSubsystem.h"
#include "ServerPacketHandler.h"
#include "BtMmoClient.h"
#include "BtMmoClientCharacter.h"
#include "Monsters/BtMonster.h"
#include "Monsters/BtMonsterCatalog.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "IPAddress.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

void UBtWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FParse::Value(FCommandLine::Get(), TEXT("WorldHost="), ServerHost);
    FParse::Value(FCommandLine::Get(), TEXT("WorldPort="), ServerPort);
    if (ServerPort < 1 || ServerPort > 65535)
    {
        UE_LOG(LogBtMmoClient, Error, TEXT("[WORLD] Invalid server port: %d"), ServerPort);
        return;
    }

    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
    Connect();
}

void UBtWorldSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    TickHandle.Reset();
    Close();
    Super::Deinitialize();
}

void UBtWorldSubsystem::Connect()
{
    Close();
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Sockets)
    {
        Retry(TEXT("Socket subsystem unavailable"));
        return;
    }

    const TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
    bool bValid = false;
    Address->SetIp(*ServerHost, bValid);
    Address->SetPort(ServerPort);
    if (!bValid)
    {
        Retry(TEXT("Invalid server IP address"));
        return;
    }

    UE_LOG(LogBtMmoClient, Log, TEXT("[WORLD] Connecting to %s:%d"), *ServerHost, ServerPort);
    Socket = Sockets->CreateSocket(NAME_Stream, TEXT("BtWorldConnection"), Address->GetProtocolType());
    if (!Socket || !Socket->SetNonBlocking(true) || !Socket->SetNoDelay(true) || !Socket->Connect(*Address))
    {
        Retry(TEXT("Connection attempt failed"));
        return;
    }

    ConnectStarted = FPlatformTime::Seconds();
}

void UBtWorldSubsystem::Close()
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }

    bConnected = false;
    ReceiveBuffer.Reset();
    Snapshot = {};
    ClearMonsterActors();
    ClearPlayerActors();
    RemotePlayerStates.Reset();
    PlayerWorld.Reset();
    LocalPlayerId = 0;
    MonsterStates.Reset();
    MonsterBehaviorStates.Reset();
    MonsterMoves.Reset();
    MonsterHealthStates.Reset();
    ActiveAttackSequence = 0;
    PendingMonsterSpawns.Reset();
    SpawnerCenters.Reset();
    MonsterWorld.Reset();
    SendBuffer.Reset();
    SendOffset = 0;
    NextPlayerMoveTime = 0;
    LastSentPlayerState.Reset();
    PlayerHealth = 100;
    PlayerMaxHealth = 100;
}

void UBtWorldSubsystem::Retry(const TCHAR* Reason)
{
    UE_LOG(LogBtMmoClient, Warning, TEXT("[WORLD] %s (%s:%d). Retrying in 3 seconds."), Reason, *ServerHost, ServerPort);
    Close();
    NextConnectTime = FPlatformTime::Seconds() + 3.0;
}

bool UBtWorldSubsystem::Tick(float)
{
    const double Now = FPlatformTime::Seconds();
    if (!Socket)
    {
        if (Now >= NextConnectTime) Connect();
        return true;
    }

    if (!bConnected)
    {
        if (Socket->GetConnectionState() == SCS_ConnectionError)
        {
            Retry(TEXT("Connection refused or failed"));
            return true;
        }

        if (Socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::Zero()))
        {
            bConnected = true;
            SnapshotDeadline = Now + 10.0;
            LastReceiveTime = Now;
            NextHeartbeatTime = Now;
        }
        else
        {
            if (Now - ConnectStarted >= 5.0) Retry(TEXT("Connection timed out"));
            return true;
        }
    }

    if (!ReceivePackets()) return true;
    SynchronizeMonsters();
    SynchronizePlayers();
    if (!Snapshot.IsComplete() && Now >= SnapshotDeadline)
    {
        Retry(TEXT("Spawner snapshot timed out"));
        return true;
    }

    if (Now - LastReceiveTime >= 30.0)
    {
        Retry(TEXT("Server heartbeat timed out"));
        return true;
    }

    if (Snapshot.IsWelcomed() && Now >= NextHeartbeatTime)
    {
        QueuePacket(RequestPing{});
        NextHeartbeatTime = Now + 10.0;
    }

    if (Snapshot.IsComplete() && Now >= NextPlayerMoveTime && SendBuffer.Num() - SendOffset < 1024)
    {
        SendPlayerMove();
        NextPlayerMoveTime = Now + WorldProtocol::PLAYER_TICK_MS / 1000.0;
    }

    FlushSendBuffer();
    return true;
}

bool UBtWorldSubsystem::ReceivePackets()
{
    uint8 Bytes[8192];
    for (int32 Batch = 0; Batch < 8; ++Batch)
    {
        int32 Read = 0;
        if (!Socket->Recv(Bytes, UE_ARRAY_COUNT(Bytes), Read))
        {
            Retry(TEXT("Server disconnected"));
            return false;
        }

        if (Read == 0) break;
        ReceiveBuffer.Append(Bytes, Read);
        int32 Offset = 0;
        while (ReceiveBuffer.Num() - Offset >= static_cast<int32>(WorldProtocol::HEADER_SIZE))
        {
            const uint8* Header = ReceiveBuffer.GetData() + Offset;
            const uint16 Size = WorldProtocol::ReadU16(Header);
            const uint16 Id = WorldProtocol::ReadU16(Header + 2);
            if (Size < WorldProtocol::HEADER_SIZE || Size > WorldProtocol::MAX_PACKET_SIZE)
            {
                Retry(TEXT("Invalid packet length"));
                return false;
            }

            if (ReceiveBuffer.Num() - Offset < Size) break;
            if (!FServerPacketHandler::HandlePacket(*this, Id, {Header + WorldProtocol::HEADER_SIZE, Size - WorldProtocol::HEADER_SIZE}))
            {
                Retry(TEXT("Invalid world packet"));
                return false;
            }

            LastReceiveTime = FPlatformTime::Seconds();
            Offset += Size;
        }

        if (Offset > 0) ReceiveBuffer.RemoveAt(0, Offset, EAllowShrinking::No);
    }

    return true;
}

bool UBtWorldSubsystem::OnWelcome(const ReturnWelcome& Info)
{
    if (!Snapshot.Accept(Info)) return false;
    LocalPlayerId = Info.sessionId;
    UE_LOG(LogBtMmoClient, Log, TEXT("[WORLD] Connected session=%llu spawners=%u"),
        static_cast<unsigned long long>(Info.sessionId), static_cast<unsigned>(Info.spawnerCount));
    if (Snapshot.IsComplete()) UE_LOG(LogBtMmoClient, Log, TEXT("[WORLD] Spawner snapshot complete (empty world)"));
    return true;
}

bool UBtWorldSubsystem::OnSpawner(const NotifySpawner& Info)
{
    if (!Snapshot.Accept(Info)) return false;
    SpawnerCenters.Add(Info.id, FVector(Info.x, Info.y, Info.z));
    UE_LOG(LogBtMmoClient, Log, TEXT("[SPAWNER] id=%llu center=(%.2f, %.2f, %.2f) alive=%u/%u pending=%u"),
        static_cast<unsigned long long>(Info.id), Info.x, Info.y, Info.z,
        static_cast<unsigned>(Info.alive), static_cast<unsigned>(Info.maxAlive), static_cast<unsigned>(Info.pending));
    if (Snapshot.IsComplete()) UE_LOG(LogBtMmoClient, Log, TEXT("[WORLD] Spawner snapshot complete"));
    return true;
}

bool UBtWorldSubsystem::OnPong(const ReturnPong&)
{
    return Snapshot.IsWelcomed();
}

bool UBtWorldSubsystem::OnMonsterSpawn(const NotifyMonsterSpawn& Info)
{
    if (!Snapshot.IsComplete()) return false;
    MonsterStates.Add(Info.id, Info);
    MonsterBehaviorStates.Remove(Info.id);
    MonsterMoves.Remove(Info.id);
    MonsterHealthStates.Remove(Info.id);
    PendingMonsterSpawns.Add(Info.id);
    return true;
}

bool UBtWorldSubsystem::OnMonsterDespawn(const NotifyMonsterDespawn& Info)
{
    if (!Snapshot.IsComplete()) return false;
    MonsterStates.Remove(Info.id);
    MonsterBehaviorStates.Remove(Info.id);
    MonsterMoves.Remove(Info.id);
    MonsterHealthStates.Remove(Info.id);
    PendingMonsterSpawns.Remove(Info.id);
    if (const auto* Actor = MonsterActors.Find(Info.id); Actor && Actor->IsValid())
    {
        Actor->Get()->Destroy();
    }

    MonsterActors.Remove(Info.id);
    UE_LOG(LogBtMmoClient, Log, TEXT("[MONSTER] Despawned id=%llu active=%d"),
        static_cast<unsigned long long>(Info.id), MonsterActors.Num());
    return true;
}

bool UBtWorldSubsystem::OnMonsterState(const NotifyMonsterState& Info)
{
    if (!Snapshot.IsComplete() || !MonsterStates.Contains(Info.id)) return false;
    MonsterBehaviorStates.Add(Info.id, Info);
    if (!PendingMonsterSpawns.Contains(Info.id))
    {
        if (const auto* Actor = MonsterActors.Find(Info.id); Actor && Actor->IsValid())
            Actor->Get()->ApplyServerState(Info);
    }

    return true;
}

bool UBtWorldSubsystem::OnMonsterMove(const NotifyMonsterMove& Info)
{
    if (!Snapshot.IsComplete()) return false;
    auto* Spawn = MonsterStates.Find(Info.id);
    if (!Spawn) return false;
    Spawn->x = Info.x;
    Spawn->y = Info.y;
    Spawn->z = Info.z;
    MonsterMoves.Add(Info.id, Info);
    if (!PendingMonsterSpawns.Contains(Info.id))
    {
        if (const auto* Actor = MonsterActors.Find(Info.id); Actor && Actor->IsValid())
            Actor->Get()->ApplyServerMove(Info);
    }

    return true;
}

bool UBtWorldSubsystem::OnPlayerHealth(const NotifyPlayerHealth& Info)
{
    if (!Snapshot.IsComplete() || Info.maxHp > MAX_int32) return false;
    PlayerHealth = static_cast<int32>(Info.hp);
    PlayerMaxHealth = static_cast<int32>(Info.maxHp);
    if (PlayerHealth <= 0) ActiveAttackSequence = 0;
    OnPlayerHealthChanged.Broadcast(PlayerHealth, PlayerMaxHealth);
    UE_LOG(LogBtMmoClient, Log, TEXT("[PLAYER] Health=%d/%u"), PlayerHealth, Info.maxHp);
    return true;
}

bool UBtWorldSubsystem::OnMonsterHealth(const NotifyMonsterHealth& Info)
{
    if (!Snapshot.IsComplete() || Info.maxHp > MAX_int32 ||
        !MonsterStates.Contains(Info.id)) return false;
    MonsterHealthStates.Add(Info.id, Info);
    if (const auto* Actor = MonsterActors.Find(Info.id); Actor && Actor->IsValid())
        Actor->Get()->ApplyServerHealth(Info);
    return true;
}

void UBtWorldSubsystem::ClearMonsterActors()
{
    const int32 Count = MonsterActors.Num();
    for (const auto& [Id, Actor] : MonsterActors)
    {
        if (Actor.IsValid()) Actor->Destroy();
    }

    MonsterActors.Reset();
    if (Count > 0)
    {
        UE_LOG(LogBtMmoClient, Log, TEXT("[MONSTER] Cleared actors=%d"), Count);
    }
}

void UBtWorldSubsystem::SynchronizeMonsters()
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || World->bIsTearingDown) return;
    if (!bCatalogLoadAttempted)
    {
        bCatalogLoadAttempted = true;
        MonsterCatalog = MonsterCatalogAsset.LoadSynchronous();
        if (!MonsterCatalog)
            UE_LOG(LogBtMmoClient, Error, TEXT("[MONSTER] Cannot load MonsterCatalogAsset: %s"), *MonsterCatalogAsset.ToString());
    }

    if (!MonsterCatalog) return;
    if (MonsterWorld.Get() != World)
    {
        ClearMonsterActors();
        MonsterWorld = World;
        for (const auto& [Id, Info] : MonsterStates) PendingMonsterSpawns.Add(Id);
    }

    for (auto It = PendingMonsterSpawns.CreateIterator(); It; ++It)
    {
        const NotifyMonsterSpawn& Info = MonsterStates.FindChecked(*It);
        const FVector GroundPosition(Info.x, Info.y, Info.z);
        const TSubclassOf<ABtMonster> MonsterClass = MonsterCatalog->FindMonsterClass(Info.templateId);
        if (!MonsterClass)
        {
            if (!MissingMonsterTemplates.Contains(Info.templateId))
            {
                MissingMonsterTemplates.Add(Info.templateId);
                UE_LOG(LogBtMmoClient, Warning, TEXT("[MONSTER] No valid Blueprint registered for template=%u in %s; spawn skipped"),
                    Info.templateId, *MonsterCatalog->GetPathName());
            }

            if (const auto* Existing = MonsterActors.Find(Info.id); Existing && Existing->IsValid()) Existing->Get()->Destroy();
            MonsterActors.Remove(Info.id);
            It.RemoveCurrent();
            continue;
        }

        if (const auto* Existing = MonsterActors.Find(Info.id); Existing && Existing->IsValid() && Existing->Get()->GetClass() != MonsterClass.Get())
        {
            Existing->Get()->Destroy();
            MonsterActors.Remove(Info.id);
        }

        if (const auto* Existing = MonsterActors.Find(Info.id); Existing && Existing->IsValid())
        {
            Existing->Get()->InitializeMonster(Info.id, Info.ownerSpawnerId, Info.templateId);
            Existing->Get()->ApplyServerGroundPosition(GroundPosition, true);
            if (const auto* Health = MonsterHealthStates.Find(Info.id)) Existing->Get()->ApplyServerHealth(*Health);
            if (const auto* State = MonsterBehaviorStates.Find(Info.id)) Existing->Get()->ApplyServerState(*State);
            if (const auto* Movement = MonsterMoves.Find(Info.id)) Existing->Get()->ApplyServerMove(*Movement, true);
            It.RemoveCurrent();
            continue;
        }

        const float HalfHeight = MonsterClass->GetDefaultObject<ABtMonster>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FTransform Transform(FRotator::ZeroRotator, GroundPosition + FVector(0, 0, HalfHeight));
        ABtMonster* Monster = World->SpawnActorDeferred<ABtMonster>(
            MonsterClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Monster) continue;
        Monster->InitializeMonster(Info.id, Info.ownerSpawnerId, Info.templateId);
        Monster->FinishSpawning(Transform);
        Monster->ApplyServerGroundPosition(GroundPosition, true);
        MonsterActors.Add(Info.id, Monster);
        if (const auto* Health = MonsterHealthStates.Find(Info.id)) Monster->ApplyServerHealth(*Health);
        if (const auto* State = MonsterBehaviorStates.Find(Info.id)) Monster->ApplyServerState(*State);
        if (const auto* Movement = MonsterMoves.Find(Info.id)) Monster->ApplyServerMove(*Movement, true);
        It.RemoveCurrent();

        const FVector* Center = SpawnerCenters.Find(Info.ownerSpawnerId);
        const FString Region = Info.ownerSpawnerId == 0 ? TEXT("Manual")
            : FString::Printf(TEXT("Spawner:%llu"), static_cast<unsigned long long>(Info.ownerSpawnerId));
        const auto* Mesh = Monster->GetMesh();
        auto* Animation = Monster->GetMesh()->GetSingleNodeInstance();
        UE_LOG(LogBtMmoClient, Log,
            TEXT("[MONSTER] Spawned Monster id=%llu template=%u map=%s region=%s center=(%s) ground=(%.2f, %.2f, %.2f) actor=(%s) mesh=%s anim=%s class=%s active=%d"),
            static_cast<unsigned long long>(Info.id), static_cast<unsigned>(Info.templateId),
            *World->GetOutermost()->GetName(), *Region, Center ? *Center->ToString() : TEXT("unknown"),
            Info.x, Info.y, Info.z, *Monster->GetActorLocation().ToString(),
            *GetNameSafe(Mesh->GetSkeletalMeshAsset()), *GetNameSafe(Animation ? Animation->GetCurrentAsset() : nullptr),
            *Monster->GetClass()->GetPathName(),
            MonsterActors.Num());
    }
}

void UBtWorldSubsystem::SendPlayerMove()
{
    UWorld* World = GetWorld();
    if (!World || !World->HasBegunPlay() || World->bIsTearingDown) return;
    APlayerController* Controller = World->GetFirstPlayerController();
    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn || !Pawn->IsLocallyControlled()) return;
    if (const auto* Character = Cast<ABtMmoClientCharacter>(Pawn))
    {
        if (!QueuePacket(Character->CaptureNetworkMove())) return;
        const auto State = Character->CaptureNetworkState();
        if ((!LastSentPlayerState.IsSet() || LastSentPlayerState.GetValue() != State) && QueuePacket(State))
            LastSentPlayerState = State;
    }
    else
    {
        const FVector Position = Pawn->GetActorLocation();
        QueuePacket(RequestPlayerPosition{Position.X, Position.Y, Position.Z});
    }
}

bool UBtWorldSubsystem::OnPlayerSpawn(const NotifyPlayerSpawn& Info)
{
    if (!Snapshot.IsComplete()) return false;
    if (Info.id == LocalPlayerId) return true;
    RemotePlayerStates.Add(Info.id, Info);
    if (ABtMmoClientCharacter* Actor = FindRemotePlayer(Info.id))
    {
        Actor->ApplyRemoteState({Info.id, Info.state, Info.hp, Info.maxHp}, true);
        Actor->ApplyRemoteMove({Info.id, Info.movement}, true);
    }

    return true;
}

bool UBtWorldSubsystem::OnPlayerState(const NotifyPlayerState& Info)
{
    if (!Snapshot.IsComplete()) return false;
    if (Info.id == LocalPlayerId) return true;
    auto* Cached = RemotePlayerStates.Find(Info.id);
    if (!Cached) return false;
    Cached->state = Info.state;
    Cached->hp = Info.hp;
    Cached->maxHp = Info.maxHp;
    if (ABtMmoClientCharacter* Actor = FindRemotePlayer(Info.id)) Actor->ApplyRemoteState(Info);
    return true;
}

bool UBtWorldSubsystem::OnPlayerMove(const NotifyPlayerMove& Info)
{
    if (!Snapshot.IsComplete()) return false;
    if (Info.id == LocalPlayerId)
    {
        UWorld* World = GetWorld();
        APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        if (Pawn && Pawn->IsLocallyControlled())
        {
            if (auto* Character = Cast<ABtMmoClientCharacter>(Pawn))
                Character->ApplyLocalMoveCorrection(Info.movement);
            else
                Pawn->SetActorLocation(FVector(Info.movement.x, Info.movement.y, Info.movement.z), false, nullptr, ETeleportType::TeleportPhysics);
        }

        return true;
    }

    auto* Cached = RemotePlayerStates.Find(Info.id);
    if (!Cached) return false;
    Cached->movement = Info.movement;
    if (ABtMmoClientCharacter* Actor = FindRemotePlayer(Info.id)) Actor->ApplyRemoteMove(Info);
    return true;
}

bool UBtWorldSubsystem::OnPlayerDespawn(const NotifyPlayerDespawn& Info)
{
    if (!Snapshot.IsComplete()) return false;
    RemotePlayerStates.Remove(Info.id);
    if (ABtMmoClientCharacter* Actor = FindRemotePlayer(Info.id)) Actor->Destroy();
    RemotePlayerActors.Remove(Info.id);
    UE_LOG(LogBtMmoClient, Log, TEXT("[OTHER] Despawned id=%llu active=%d"),
        static_cast<unsigned long long>(Info.id), RemotePlayerActors.Num());
    return true;
}

ABtMmoClientCharacter* UBtWorldSubsystem::FindRemotePlayer(uint64 Id) const
{
    const auto* Actor = RemotePlayerActors.Find(Id);
    return Actor ? Actor->Get() : nullptr;
}

void UBtWorldSubsystem::ClearPlayerActors()
{
    const int32 Count = RemotePlayerActors.Num();
    for (const auto& [Id, Actor] : RemotePlayerActors)
    {
        if (Actor.IsValid()) Actor->Destroy();
    }

    RemotePlayerActors.Reset();
    if (Count > 0) UE_LOG(LogBtMmoClient, Log, TEXT("[OTHER] Cleared actors=%d"), Count);
}

void UBtWorldSubsystem::SynchronizePlayers()
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || World->bIsTearingDown) return;
    if (PlayerWorld.Get() != World)
    {
        ClearPlayerActors();
        PlayerWorld = World;
    }

    APlayerController* Controller = World->GetFirstPlayerController();
    auto* LocalPlayer = Controller ? Cast<ABtMmoClientCharacter>(Controller->GetPawn()) : nullptr;
    if (!LocalPlayer || !LocalPlayer->IsLocallyControlled()) return;
    for (const auto& [Id, State] : RemotePlayerStates)
    {
        if (FindRemotePlayer(Id)) continue;
        const FVector Location = FVector(State.movement.x, State.movement.y, State.movement.z) +
            FVector(0, 0, LocalPlayer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FTransform Transform(FRotator(0, State.movement.yaw, 0), Location);
        auto* Actor = World->SpawnActorDeferred<ABtMmoClientCharacter>(LocalPlayer->GetClass(), Transform,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Actor) continue;
        Actor->InitializeRemotePlayer(Id);
        Actor->FinishSpawning(Transform);
        Actor->ApplyRemoteState({State.id, State.state, State.hp, State.maxHp}, true);
        Actor->ApplyRemoteMove({State.id, State.movement}, true);
        RemotePlayerActors.Add(Id, Actor);
        UE_LOG(LogBtMmoClient, Log, TEXT("[OTHER] Spawned id=%llu ground=(%.2f, %.2f, %.2f) active=%d"),
            static_cast<unsigned long long>(Id), State.movement.x, State.movement.y, State.movement.z, RemotePlayerActors.Num());
    }
}

uint64 UBtWorldSubsystem::BeginMeleeAttack(APawn* Attacker)
{
    if (!bConnected || !Snapshot.IsComplete() || PlayerHealth <= 0 || !IsValid(Attacker) ||
        !Attacker->IsLocallyControlled() || Attacker->GetWorld() != GetWorld()) return 0;
    ActiveAttackSequence = ++NextAttackSequence;
    if (ActiveAttackSequence == 0) ActiveAttackSequence = ++NextAttackSequence;
    return ActiveAttackSequence;
}

bool UBtWorldSubsystem::ReportMeleeHit(APawn* Attacker, uint64 AttackSequence, ABtMonster* Monster)
{
    if (!bConnected || !Snapshot.IsComplete() || PlayerHealth <= 0 || !IsValid(Attacker) ||
        !Attacker->IsLocallyControlled() || Attacker->GetWorld() != GetWorld() || !IsValid(Monster) ||
        AttackSequence == 0 || AttackSequence != ActiveAttackSequence || SendBuffer.Num() - SendOffset > 16384) return false;
    const auto* Registered = MonsterActors.Find(Monster->GetMonsterId());
    if (!Registered || Registered->Get() != Monster) return false;
    SendPlayerMove();
    if (!QueuePacket(RequestMeleeHit{AttackSequence, Monster->GetMonsterId()})) return false;
    UE_LOG(LogBtMmoClient, Log, TEXT("[MELEE] Contact attack=%llu monster=%llu"),
        static_cast<unsigned long long>(AttackSequence), static_cast<unsigned long long>(Monster->GetMonsterId()));
    return true;
}

void UBtWorldSubsystem::FlushSendBuffer()
{
    if (!Socket || SendBuffer.IsEmpty()) return;
    int32 Sent = 0;
    if (!Socket->Send(SendBuffer.GetData() + SendOffset, SendBuffer.Num() - SendOffset, Sent))
    {
        if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() != SE_EWOULDBLOCK)
            Retry(TEXT("World packet send failed"));
        return;
    }

    SendOffset += Sent;
    if (SendOffset == SendBuffer.Num())
    {
        SendBuffer.Reset();
        SendOffset = 0;
    }
}
