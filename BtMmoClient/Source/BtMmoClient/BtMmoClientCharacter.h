#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "WorldPackets.h"
#include "BtMmoClientCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAnimSequence;
class UBtMeleeComponent;
class UBtPlayerStatusWidget;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class ABtMmoClientCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UBtMeleeComponent> Melee;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UAnimSequence> DeathAnimation;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	ABtMmoClientCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaSeconds) override;
	void InitializeRemotePlayer(uint64 Id);
	void ApplyRemoteState(const NotifyPlayerState& State, bool bSnapshot = false);
	RequestPlayerState CaptureNetworkState() const;
	RequestPlayerMove CaptureNetworkMove() const;
	void ApplyRemoteMove(const NotifyPlayerMove& Info, bool bSnapshot = false);
	void ApplyLocalMoveCorrection(const RequestPlayerMove& Movement);

	UFUNCTION(BlueprintPure, Category="World|Player")
	bool IsRemotePlayer() const { return bRemotePlayer; }

	uint64 GetRemotePlayerId() const { return RemotePlayerId; }

	virtual float PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate = 1.f,
		FName StartSectionName = NAME_None) override;
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsDead() const { return bDead; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION()
	void HandlePlayerHealthChanged(int32 Health, int32 MaxHealth);
	void SynchronizePlayerHealth();
	void BindNetworkMontages();
	void LoadNetworkVisuals();
	UFUNCTION()
	void HandleNetworkMontageStarted(UAnimMontage* Montage);
	UPROPERTY(Transient)
	TObjectPtr<UBtPlayerStatusWidget> PlayerStatusWidget;
	bool bDead = false;
	bool bRemotePlayer = false;
	uint64 RemotePlayerId = 0;
	uint64 VisualAttackSequence = 0;
	uint8 VisualAttackIndex = 0;
	RequestPlayerMove RemoteMovement{};
	FVector RemoteStart = FVector::ZeroVector;
	FVector RemoteTarget = FVector::ZeroVector;
	FRotator RemoteStartRotation = FRotator::ZeroRotator;
	FRotator RemoteTargetRotation = FRotator::ZeroRotator;
	float RemoteElapsed = 0;
	UPROPERTY()
	TObjectPtr<class UMaterialInterface> OtherPlayerTint;
	UPROPERTY()
	TSoftObjectPtr<class UMaterialInterface> OtherPlayerTintAsset;
	UPROPERTY()
	TArray<TSoftObjectPtr<UAnimMontage>> NetworkAttackAssets;
	UPROPERTY()
	TArray<TObjectPtr<UAnimMontage>> NetworkAttackMontages;
	TEnumAsByte<EMovementMode> MovementBeforeDeath = MOVE_Walking;
	uint8 CustomMovementBeforeDeath = 0;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	void Move(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);

public:

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

