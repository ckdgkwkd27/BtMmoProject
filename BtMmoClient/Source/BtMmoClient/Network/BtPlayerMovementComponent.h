#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BtPlayerMovementComponent.generated.h"

UCLASS()
class UBtPlayerMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    void ApplyRemoteVelocity(const FVector& InVelocity);
};
