#include "BtMonster.h"
#include "BtMmoClient.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "UI/BtMonsterHealthWidget.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

ABtMonster::ABtMonster()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCanEverAffectNavigation(false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	HealthWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidget"));
	HealthWidget->SetupAttachment(RootComponent);
	HealthWidget->SetRelativeLocation(FVector(0, 0, 120));
	HealthWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidget->SetDrawSize(FVector2D(120, 42));
	HealthWidget->SetPivot(FVector2D(0.5f, 1.0f));
	HealthWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthWidget->SetCanEverAffectNavigation(false);
	HealthWidget->SetVisibility(false);
	HealthWidget->SetComponentTickEnabled(false);
}

void ABtMonster::InitializeMonster(uint64 InMonsterId, uint64 InSpawnerId, uint32 InTemplateId)
{
	MonsterId = InMonsterId;
	SpawnerId = InSpawnerId;
	TemplateId = InTemplateId;
}

void ABtMonster::ApplyServerHealth(const NotifyMonsterHealth& State)
{
	if (State.id != MonsterId || !WorldProtocol::IsValid(State) || State.maxHp > MAX_int32) return;
	Health = static_cast<int32>(State.hp);
	MaxHealth = static_cast<int32>(State.maxHp);
	OnHealthChanged.Broadcast(Health, MaxHealth);
	if (auto* Widget = Cast<UBtMonsterHealthWidget>(HealthWidget->GetUserWidgetObject())) Widget->SetHealth(Health, MaxHealth);
	if (Health < MaxHealth)
	{
		UE_LOG(LogBtMmoClient, Log, TEXT("[MONSTER] Health id=%llu hp=%d/%d"),
			static_cast<unsigned long long>(MonsterId), Health, MaxHealth);
	}
}

void ABtMonster::BeginPlay()
{
	Super::BeginPlay();
	PreviousLocation = GetActorLocation();
	UpdateLocomotionAnimation();
}

void ABtMonster::ApplyServerGroundPosition(FVector GroundPosition, bool bTeleport)
{
	SetActorLocation(GroundPosition + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
		false, nullptr, bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None);
	if (bTeleport)
	{
		ServerSamples.Reset();
		bSampleStarted = false;
		bPlayingAttack = false;
		PreviousLocation = GetActorLocation();
		MovementSpeed = 0;
	}
}

void ABtMonster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHealthWidget();
	if (bServerDriven)
	{
		UpdateServerMovement(DeltaSeconds);
	}
	else
	{
		MovementSpeed = DeltaSeconds > SMALL_NUMBER
			? FVector::Dist2D(GetActorLocation(), PreviousLocation) / DeltaSeconds : 0.0f;
	}

	PreviousLocation = GetActorLocation();
	if (bPlayingAttack)
	{
		AttackTimeRemaining -= DeltaSeconds;
		if (AttackTimeRemaining > 0)
		{
			return;
		}

		bPlayingAttack = false;
	}

	UpdateLocomotionAnimation();
}

void ABtMonster::UpdateHealthWidget()
{
	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const bool bNearby = Health > 0 && Pawn && FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(2000.0f);
	if (bNearby && !HealthWidget->GetWidgetClass())
	{
		HealthWidget->SetWidgetClass(UBtMonsterHealthWidget::StaticClass());
		HealthWidget->InitWidget();
		if (auto* Widget = Cast<UBtMonsterHealthWidget>(HealthWidget->GetUserWidgetObject())) Widget->SetHealth(Health, MaxHealth);
	}

	if (HealthWidget->IsVisible() != bNearby)
	{
		HealthWidget->SetVisibility(bNearby);
		HealthWidget->SetComponentTickEnabled(bNearby);
	}
}

void ABtMonster::ApplyServerState(const NotifyMonsterState& State)
{
	if (State.id != MonsterId || !WorldProtocol::IsValid(State)) return;
	LatestServerState = State;
}

void ABtMonster::ApplyServerMove(const NotifyMonsterMove& Movement, bool bSnapshot)
{
	if (Movement.id != MonsterId || !WorldProtocol::IsValid(Movement)) return;
	bServerDriven = true;
	SecondsSinceState = 0;
	const FServerSample Sample{Movement, LatestServerState};
	if (bSnapshot || ServerSamples.Num() >= 16)
	{
		ApplyServerGroundPosition(FVector(Movement.x, Movement.y, Movement.z), true);
		BeginServerSample(Sample);
		return;
	}

	ServerSamples.Add(Sample);
}

void ABtMonster::BeginServerSample(const FServerSample& Sample)
{
	const auto& State = Sample.State;
	const auto& Movement = Sample.Movement;
	SetActorRotation(FRotator(0, Movement.yaw, 0));
	MovementSpeed = State.action == EMonsterAction::WALK || State.action == EMonsterAction::RUN
		? static_cast<float>(Movement.speed) : 0;
	if (State.action == EMonsterAction::ATTACK && State.attackSequence != LastAttackSequence)
	{
		if (PlayAttackAnimation((State.attackSequence % 2) == 0))
		{
			UAnimSingleNodeInstance* Instance = GetMesh()->GetSingleNodeInstance();
			if (Instance) Instance->SetPlayRate(AttackTimeRemaining / WorldProtocol::MONSTER_ATTACK_SECONDS);
			AttackTimeRemaining = WorldProtocol::MONSTER_ATTACK_SECONDS;
		}
	}
	else if (MovementSpeed > 0)
	{
		bPlayingAttack = false;
	}

	LastAttackSequence = State.attackSequence;
	if (!bPlayingAttack) UpdateLocomotionAnimation();
}

void ABtMonster::UpdateServerMovement(float DeltaSeconds)
{
	SecondsSinceState += DeltaSeconds;
	float Remaining = DeltaSeconds;
	constexpr float SampleDuration = WorldProtocol::MONSTER_TICK_MS / 1000.0f;
	while (Remaining > SMALL_NUMBER && !ServerSamples.IsEmpty())
	{
		const auto& Sample = ServerSamples[0];
		const auto& Movement = Sample.Movement;
		if (!bSampleStarted)
		{
			SegmentStart = GetActorLocation();
			SegmentTime = 0;
			bSampleStarted = true;
			BeginServerSample(Sample);
		}

		const float Step = FMath::Min(Remaining, SampleDuration - SegmentTime);
		SegmentTime += Step;
		Remaining -= Step;
		const FVector End = FVector(Movement.x, Movement.y, Movement.z) + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		SetActorLocation(FMath::Lerp(SegmentStart, End, FMath::Clamp(SegmentTime / SampleDuration, 0.0f, 1.0f)), false);
		if (SegmentTime >= SampleDuration - SMALL_NUMBER)
		{
			ServerSamples.RemoveAt(0, 1, EAllowShrinking::No);
			bSampleStarted = false;
		}
	}

	if (ServerSamples.IsEmpty() && SecondsSinceState > 0.35f) MovementSpeed = 0;
}

void ABtMonster::UpdateLocomotionAnimation()
{
	UAnimSequence* Animation = IdleAnimation;
	if (MovementSpeed >= RunSpeedThreshold && RunAnimation)
	{
		Animation = RunAnimation;
	}
	else if (MovementSpeed > 1.0f && WalkAnimation)
	{
		Animation = WalkAnimation;
	}

	UAnimSingleNodeInstance* Instance = GetMesh()->GetSingleNodeInstance();
	if (Animation && (!Instance || Instance->GetCurrentAsset() != Animation || !Instance->IsPlaying()))
	{
		GetMesh()->PlayAnimation(Animation, true);
		if (auto* NewInstance = GetMesh()->GetSingleNodeInstance()) NewInstance->SetPlayRate(1.0f);
	}
}

bool ABtMonster::PlayAttackAnimation(bool bAlternate)
{
	UAnimSequence* Animation = bAlternate ? AlternateAttackAnimation : AttackAnimation;
	if (!Animation || !GetMesh()->GetSkeletalMeshAsset())
	{
		return false;
	}

	bPlayingAttack = true;
	AttackTimeRemaining = Animation->GetPlayLength();
	GetMesh()->PlayAnimation(Animation, false);
	if (auto* Instance = GetMesh()->GetSingleNodeInstance()) Instance->SetPlayRate(1.0f);
	return true;
}
