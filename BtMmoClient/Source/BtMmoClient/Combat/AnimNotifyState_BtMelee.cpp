#include "AnimNotifyState_BtMelee.h"
#include "BtMeleeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

static UBtMeleeComponent* FindMelee(USkeletalMeshComponent* Mesh)
{
	return Mesh && Mesh->GetOwner() ? Mesh->GetOwner()->FindComponentByClass<UBtMeleeComponent>() : nullptr;
}

void UAnimNotifyState_BtMelee::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (UBtMeleeComponent* Melee = FindMelee(Mesh)) Melee->BeginSwing(Mesh);
}

void UAnimNotifyState_BtMelee::NotifyTick(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	if (UBtMeleeComponent* Melee = FindMelee(Mesh)) Melee->UpdateSwing();
}

void UAnimNotifyState_BtMelee::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UBtMeleeComponent* Melee = FindMelee(Mesh))
	{
		Melee->UpdateSwing();
		Melee->EndSwing();
	}
}
