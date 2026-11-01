#include "BtMeleeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Monsters/BtMonster.h"
#include "Network/BtWorldSubsystem.h"

UBtMeleeComponent::UBtMeleeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBtMeleeComponent::BeginSwing(USkeletalMeshComponent* Mesh)
{
	EndSwing();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !Mesh || Mesh->GetOwner() != Pawn ||
		!Mesh->DoesSocketExist(BaseSocket) || !Mesh->DoesSocketExist(TipSocket)) return;
	UGameInstance* Instance = Pawn->GetGameInstance();
	UBtWorldSubsystem* Network = Instance ? Instance->GetSubsystem<UBtWorldSubsystem>() : nullptr;
	if (!Network) return;
	AttackSequence = Network->BeginMeleeAttack(Pawn);
	if (AttackSequence == 0) return;
	AttackMesh = Mesh;
	PreviousBase = Mesh->GetSocketLocation(BaseSocket);
	PreviousTip = Mesh->GetSocketLocation(TipSocket);
	UpdateSwing();
}

TArray<ABtMonster*> UBtMeleeComponent::SweepBlade(const FVector& OldBase, const FVector& OldTip,
	const FVector& NewBase, const FVector& NewTip) const
{
	TArray<ABtMonster*> Monsters;
	if (!GetWorld()) return Monsters;
	const float Radius = FMath::Clamp(SweepRadius, 1.0f, 50.0f);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BtMelee), false, GetOwner());
	const auto Sweep = [&](const FVector& Start, const FVector& End)
	{
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
			FCollisionShape::MakeSphere(Radius), Query);
		for (const FHitResult& Hit : Hits)
		{
			if (ABtMonster* Monster = Cast<ABtMonster>(Hit.GetActor())) Monsters.AddUnique(Monster);
		}

		if (bDrawDebug)
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Cyan, false, 0.15f);
			DrawDebugSphere(GetWorld(), End, Radius, 8, FColor::Yellow, false, 0.15f);
		}
	};

	Sweep(OldBase, OldTip);
	Sweep(NewBase, NewTip);
	const float Length = FMath::Max(FVector::Distance(OldBase, OldTip), FVector::Distance(NewBase, NewTip));
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(Length / Radius), 1, 32);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / Segments;
		Sweep(FMath::Lerp(OldBase, OldTip, Alpha), FMath::Lerp(NewBase, NewTip, Alpha));
	}

	return Monsters;
}

void UBtMeleeComponent::UpdateSwing()
{
	USkeletalMeshComponent* Mesh = AttackMesh.Get();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Mesh || !Pawn || !Pawn->IsLocallyControlled() || AttackSequence == 0) return;
	UGameInstance* Instance = Pawn->GetGameInstance();
	UBtWorldSubsystem* Network = Instance ? Instance->GetSubsystem<UBtWorldSubsystem>() : nullptr;
	if (!Network) return;
	const FVector Base = Mesh->GetSocketLocation(BaseSocket);
	const FVector Tip = Mesh->GetSocketLocation(TipSocket);
	if (FVector::DistSquared(Base, PreviousBase) < FMath::Square(500.0f) &&
		FVector::DistSquared(Tip, PreviousTip) < FMath::Square(500.0f))
	{
		for (ABtMonster* Monster : SweepBlade(PreviousBase, PreviousTip, Base, Tip))
		{
			if (HitMonsters.Num() >= 64) break;
			if (!HitMonsters.Contains(Monster->GetMonsterId()) && Network->ReportMeleeHit(Pawn, AttackSequence, Monster))
				HitMonsters.Add(Monster->GetMonsterId());
		}
	}

	PreviousBase = Base;
	PreviousTip = Tip;
}

void UBtMeleeComponent::EndSwing()
{
	AttackSequence = 0;
	AttackMesh.Reset();
	HitMonsters.Reset();
}

void UBtMeleeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndSwing();
	Super::EndPlay(EndPlayReason);
}
