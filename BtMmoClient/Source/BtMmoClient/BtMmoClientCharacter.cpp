#include "BtMmoClientCharacter.h"
#include "Combat/BtMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInterface.h"
#include "Network/BtWorldSubsystem.h"
#include "Network/BtPlayerMovementComponent.h"
#include "UI/BtPlayerStatusWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "BtMmoClient.h"
#include "UObject/ConstructorHelpers.h"

ABtMmoClientCharacter::ABtMmoClientCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBtPlayerMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	Melee = CreateDefaultSubobject<UBtMeleeComponent>(TEXT("Melee"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DeathSequence(
		TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Animations/Death.Death"));
	DeathAnimation = DeathSequence.Object;
	OtherPlayerTintAsset = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Aurora/Materials/M_OtherPlayerTint.M_OtherPlayerTint")));
	for (const TCHAR* Letter : {TEXT("A"), TEXT("B"), TEXT("C")})
	{
		const FString Path = FString::Printf(TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Animations/Primary_Attack_%s_Montage.Primary_Attack_%s_Montage"), Letter, Letter);
		NetworkAttackAssets.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(Path)));
	}

	PrimaryActorTick.bCanEverTick = true;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ABtMmoClientCharacter::BeginPlay()
{
	LoadNetworkVisuals();
	Super::BeginPlay();
	if (bRemotePlayer)
	{
		InitializeRemotePlayer(RemotePlayerId);
		return;
	}

	if (UGameInstance* Instance = GetGameInstance())
	{
		if (UBtWorldSubsystem* Network = Instance->GetSubsystem<UBtWorldSubsystem>())
			Network->OnPlayerHealthChanged.AddDynamic(this, &ThisClass::HandlePlayerHealthChanged);
	}

	SynchronizePlayerHealth();
	BindNetworkMontages();
}

void ABtMmoClientCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* Instance = GetGameInstance())
	{
		if (UBtWorldSubsystem* Network = Instance->GetSubsystem<UBtWorldSubsystem>())
			Network->OnPlayerHealthChanged.RemoveDynamic(this, &ThisClass::HandlePlayerHealthChanged);
	}

	if (PlayerStatusWidget) PlayerStatusWidget->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}

void ABtMmoClientCharacter::SynchronizePlayerHealth()
{
	if (!IsLocallyControlled()) return;
	if (UGameInstance* Instance = GetGameInstance())
	{
		if (UBtWorldSubsystem* Network = Instance->GetSubsystem<UBtWorldSubsystem>())
			HandlePlayerHealthChanged(Network->PlayerHealth, Network->PlayerMaxHealth);
	}
}

void ABtMmoClientCharacter::HandlePlayerHealthChanged(int32 Health, int32 MaxHealth)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->IsLocalController()) return;
	if (!PlayerStatusWidget)
	{
		PlayerStatusWidget = CreateWidget<UBtPlayerStatusWidget>(PlayerController);
		if (PlayerStatusWidget) PlayerStatusWidget->AddToPlayerScreen(10);
	}

	if (PlayerStatusWidget) PlayerStatusWidget->SetHealth(Health, MaxHealth);
	const bool bWasDead = bDead;
	bDead = Health <= 0;
	if (bDead)
	{
		if (!bWasDead)
		{
			MovementBeforeDeath = GetCharacterMovement()->MovementMode;
			CustomMovementBeforeDeath = GetCharacterMovement()->CustomMovementMode;
			UE_LOG(LogBtMmoClient, Log, TEXT("[PLAYER] Dead: movement and attacks disabled"));
		}

		Melee->EndSwing();
		StopJumping();
		ConsumeMovementInputVector();
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		DisableInput(PlayerController);
		if (!bWasDead)
		{
			if (UAnimInstance* Animation = GetMesh()->GetAnimInstance()) Animation->Montage_Stop(0.1f);
			if (DeathAnimation) GetMesh()->PlayAnimation(DeathAnimation, false);
		}
	}
	else if (bWasDead)
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		BindNetworkMontages();
		GetCharacterMovement()->SetMovementMode(MovementBeforeDeath, CustomMovementBeforeDeath);
		EnableInput(PlayerController);
		UE_LOG(LogBtMmoClient, Log, TEXT("[PLAYER] Alive: movement and attacks restored"));
	}
}

float ABtMmoClientCharacter::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	return bDead ? 0.0f : Super::PlayAnimMontage(AnimMontage, InPlayRate, StartSectionName);
}

void ABtMmoClientCharacter::BindNetworkMontages()
{
	if (bRemotePlayer) return;
	if (UAnimInstance* Animation = GetMesh()->GetAnimInstance())
		Animation->OnMontageStarted.AddUniqueDynamic(this, &ThisClass::HandleNetworkMontageStarted);
}

void ABtMmoClientCharacter::HandleNetworkMontageStarted(UAnimMontage* Montage)
{
	const int32 Index = NetworkAttackMontages.IndexOfByKey(Montage);
	if (!bRemotePlayer && !bDead && Index != INDEX_NONE)
	{
		VisualAttackIndex = static_cast<uint8>(Index + 1);
		++VisualAttackSequence;
	}
}

void ABtMmoClientCharacter::InitializeRemotePlayer(uint64 Id)
{
	LoadNetworkVisuals();
	bRemotePlayer = true;
	RemotePlayerId = Id;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
	SetReplicates(false);
	SetReplicateMovement(false);
	SetActorEnableCollision(false);
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	FollowCamera->Deactivate();
	GetMesh()->SetOverlayMaterial(OtherPlayerTint);
}

void ABtMmoClientCharacter::LoadNetworkVisuals()
{
	if (!OtherPlayerTint) OtherPlayerTint = OtherPlayerTintAsset.LoadSynchronous();
	if (NetworkAttackMontages.IsEmpty())
	{
		for (const auto& Asset : NetworkAttackAssets) NetworkAttackMontages.Add(Asset.LoadSynchronous());
	}
}

RequestPlayerMove ABtMmoClientCharacter::CaptureNetworkMove() const
{
	FVector Feet = GetActorLocation();
	Feet.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Velocity = bDead ? FVector::ZeroVector : GetVelocity();
	return {Feet.X, Feet.Y, Feet.Z, FRotator::NormalizeAxis(GetActorRotation().Yaw),
		Velocity.X, Velocity.Y, Velocity.Z, static_cast<uint8>(!bDead && GetCharacterMovement()->IsFalling())};
}

RequestPlayerState ABtMmoClientCharacter::CaptureNetworkState() const
{
	uint8 Attack = 0;
	if (!bDead && VisualAttackIndex > 0)
	{
		const UAnimInstance* Animation = GetMesh()->GetAnimInstance();
		if (Animation && Animation->Montage_IsPlaying(NetworkAttackMontages[VisualAttackIndex - 1])) Attack = VisualAttackIndex;
	}

	return {Attack, VisualAttackSequence};
}

void ABtMmoClientCharacter::ApplyLocalMoveCorrection(const RequestPlayerMove& Movement)
{
	if (bRemotePlayer || !IsLocallyControlled()) return;
	const FVector Position = FVector(Movement.x, Movement.y, Movement.z) +
		FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
	ConsumeMovementInputVector();
	UCharacterMovementComponent* Component = GetCharacterMovement();
	Component->StopMovementImmediately();
	Component->bJustTeleported = true;
	if (!bDead && Component->MovementMode != MOVE_None)
	{
		Component->SetMovementMode(Movement.falling ? MOVE_Falling : MOVE_Walking);
		Component->Velocity = FVector(Movement.velocityX, Movement.velocityY, Movement.velocityZ);
		Component->UpdateComponentVelocity();
	}
}

void ABtMmoClientCharacter::ApplyRemoteMove(const NotifyPlayerMove& Info, bool bSnapshot)
{
	if (!bRemotePlayer || Info.id != RemotePlayerId) return;
	RemoteMovement = Info.movement;
	RemoteStart = GetActorLocation();
	RemoteTarget = FVector(RemoteMovement.x, RemoteMovement.y, RemoteMovement.z) +
		FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	RemoteStartRotation = GetActorRotation();
	RemoteTargetRotation = FRotator(0, RemoteMovement.yaw, 0);
	RemoteElapsed = 0;
	if (bSnapshot || FVector::DistSquared(RemoteStart, RemoteTarget) > FMath::Square(1000.0))
	{
		RemoteStart = RemoteTarget;
		RemoteStartRotation = RemoteTargetRotation;
		SetActorLocationAndRotation(RemoteTarget, RemoteTargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	GetCharacterMovement()->SetMovementMode(bDead ? MOVE_None : (RemoteMovement.falling ? MOVE_Falling : MOVE_Walking));
	CastChecked<UBtPlayerMovementComponent>(GetCharacterMovement())->ApplyRemoteVelocity(
		bDead ? FVector::ZeroVector : FVector(RemoteMovement.velocityX, RemoteMovement.velocityY, RemoteMovement.velocityZ));
}

void ABtMmoClientCharacter::ApplyRemoteState(const NotifyPlayerState& State, bool bSnapshot)
{
	if (!bRemotePlayer || State.id != RemotePlayerId) return;
	const bool bWasDead = bDead;
	bDead = State.hp == 0;
	GetCharacterMovement()->SetMovementMode(bDead ? MOVE_None : (RemoteMovement.falling ? MOVE_Falling : MOVE_Walking));
	CastChecked<UBtPlayerMovementComponent>(GetCharacterMovement())->ApplyRemoteVelocity(
		bDead ? FVector::ZeroVector : FVector(RemoteMovement.velocityX, RemoteMovement.velocityY, RemoteMovement.velocityZ));
	if (bDead && !bWasDead)
	{
		if (UAnimInstance* Animation = GetMesh()->GetAnimInstance()) Animation->Montage_Stop(0.1f);
		if (DeathAnimation) GetMesh()->PlayAnimation(DeathAnimation, false);
	}
	else if (!bDead && bWasDead)
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}

	if (!bDead && !bSnapshot && State.state.attackIndex > 0 && State.state.attackSequence != VisualAttackSequence)
	{
		Super::PlayAnimMontage(NetworkAttackMontages[State.state.attackIndex - 1]);
	}

	VisualAttackSequence = State.state.attackSequence;
}

void ABtMmoClientCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRemotePlayer) return;
	RemoteElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(RemoteElapsed / (WorldProtocol::PLAYER_TICK_MS / 1000.0f), 0.0f, 1.0f);
	SetActorLocationAndRotation(FMath::Lerp(RemoteStart, RemoteTarget, Alpha),
		FQuat::Slerp(RemoteStartRotation.Quaternion(), RemoteTargetRotation.Quaternion(), Alpha));
}

void ABtMmoClientCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABtMmoClientCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABtMmoClientCharacter::Look);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABtMmoClientCharacter::Look);
	}
	else
	{
		UE_LOG(LogBtMmoClient, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	SynchronizePlayerHealth();
}

void ABtMmoClientCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	DoMove(MovementVector.X, MovementVector.Y);
}

void ABtMmoClientCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ABtMmoClientCharacter::DoMove(float Right, float Forward)
{
	if (bDead) return;
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ABtMmoClientCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABtMmoClientCharacter::DoJumpStart()
{
	if (bDead) return;
	Jump();
}

void ABtMmoClientCharacter::DoJumpEnd()
{
	StopJumping();
}
