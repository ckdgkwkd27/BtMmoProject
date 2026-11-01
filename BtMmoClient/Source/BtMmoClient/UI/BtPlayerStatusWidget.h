#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BtPlayerStatusWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class UBtPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetHealth(int32 Health, int32 MaxHealth);

protected:
	virtual void NativeOnInitialized() override;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeathMessage;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;
};
