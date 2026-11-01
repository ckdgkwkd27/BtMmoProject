#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WorldProtocol.h"
#include "BtMonster.generated.h"

class UAnimSequence;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBtMonsterHealthChanged, int32, Health, int32, MaxHealth);

UCLASS(Abstract, Blueprintable)
class BTMMOCLIENT_API ABtMonster : public ACharacter
{
	GENERATED_BODY()
public:
	ABtMonster();
	virtual void Tick(float DeltaSeconds) override;
	void InitializeMonster(uint64 InMonsterId, uint64 InSpawnerId, uint32 InTemplateId);
	void ApplyServerState(const NotifyMonsterState& State);
	void ApplyServerMove(const NotifyMonsterMove& Movement, bool bSnapshot = false);
	void ApplyServerHealth(const NotifyMonsterHealth& State);
	UPROPERTY(BlueprintReadOnly, Category="Monster|Health")
	int32 Health = 100;
	UPROPERTY(BlueprintReadOnly, Category="Monster|Health")
	int32 MaxHealth = 100;
	UPROPERTY(BlueprintAssignable, Category="Monster|Health")
	FBtMonsterHealthChanged OnHealthChanged;

	UFUNCTION(BlueprintCallable, Category="Monster")
	void ApplyServerGroundPosition(FVector GroundPosition, bool bTeleport = false);

	UFUNCTION(BlueprintCallable, Category="Monster|Animation")
	bool PlayAttackAnimation(bool bAlternate = false);

	uint64 GetMonsterId() const { return MonsterId; }

	uint64 GetSpawnerId() const { return SpawnerId; }

	uint32 GetTemplateId() const { return TemplateId; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Monster|Health")
	TObjectPtr<UWidgetComponent> HealthWidget;
	void UpdateHealthWidget();
	virtual void BeginPlay() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Animation")
	TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Animation")
	TObjectPtr<UAnimSequence> WalkAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Animation")
	TObjectPtr<UAnimSequence> RunAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Animation")
	TObjectPtr<UAnimSequence> AttackAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Animation")
	TObjectPtr<UAnimSequence> AlternateAttackAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Monster|Animation", meta=(ClampMin="1.0"))
	float RunSpeedThreshold = 250.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Monster|Animation")
	float MovementSpeed = 0.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Monster|Animation")
	bool bPlayingAttack = false;

private:
	void UpdateLocomotionAnimation();
	struct FServerSample
	{
		NotifyMonsterMove Movement;
		NotifyMonsterState State;
	};

	NotifyMonsterState LatestServerState{};
	void BeginServerSample(const FServerSample& Sample);
	void UpdateServerMovement(float DeltaSeconds);
	TArray<FServerSample> ServerSamples;
	FVector SegmentStart = FVector::ZeroVector;
	float SegmentTime = 0;
	float SecondsSinceState = 0;
	uint32 LastAttackSequence = 0;
	bool bServerDriven = false;
	bool bSampleStarted = false;
	UPROPERTY(Transient)
	uint64 MonsterId = 0;
	UPROPERTY(Transient)
	uint64 SpawnerId = 0;
	UPROPERTY(Transient)
	uint32 TemplateId = 0;
	FVector PreviousLocation = FVector::ZeroVector;
	float AttackTimeRemaining = 0.0f;
};
