#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BtMonsterHealthWidget.generated.h"

class UTextBlock;
class UProgressBar;

UCLASS()
class UBtMonsterHealthWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetHealth(int32 Health, int32 MaxHealth);
protected:
	virtual void NativeOnInitialized() override;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;
};
