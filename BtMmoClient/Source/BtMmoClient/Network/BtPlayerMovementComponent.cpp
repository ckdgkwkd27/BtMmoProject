#include "Network/BtPlayerMovementComponent.h"

void UBtPlayerMovementComponent::ApplyRemoteVelocity(const FVector& InVelocity)
{
    Velocity = InVelocity;
    Acceleration = FVector(Velocity.X, Velocity.Y, 0).GetSafeNormal() * GetMaxAcceleration();
    UpdateComponentVelocity();
}
