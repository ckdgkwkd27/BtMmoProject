#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BtMmoClientPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

UCLASS(abstract)
class ABtMmoClientPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input|Combat", meta=(ClampMin="0.0", Units="s"))
	float AttackClickDebounceSeconds = 0.2f;

	double LastAttackClickTime = -1.0;
	bool bSuppressAttackClick = false;

	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	bool ShouldUseTouchControls() const;

};
