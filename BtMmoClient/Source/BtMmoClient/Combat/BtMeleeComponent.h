#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BtMeleeComponent.generated.h"

class USkeletalMeshComponent;
class ABtMonster;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class BTMMOCLIENT_API UBtMeleeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UBtMeleeComponent();
	void BeginSwing(USkeletalMeshComponent* Mesh);
	void UpdateSwing();
	void EndSwing();

	TArray<ABtMonster*> SweepBlade(const FVector& PreviousBase, const FVector& PreviousTip,
		const FVector& CurrentBase, const FVector& CurrentTip) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	FName BaseSocket = TEXT("Sword_Base");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	FName TipSocket = TEXT("Sword_Tip");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee", meta=(ClampMin="1", ClampMax="50"))
	float SweepRadius = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	bool bDrawDebug = false;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<USkeletalMeshComponent> AttackMesh;
	FVector PreviousBase = FVector::ZeroVector;
	FVector PreviousTip = FVector::ZeroVector;
	TSet<uint64> HitMonsters;
	uint64 AttackSequence = 0;
};
