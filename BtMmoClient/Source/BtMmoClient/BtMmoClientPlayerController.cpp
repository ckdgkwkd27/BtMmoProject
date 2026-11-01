#include "BtMmoClientPlayerController.h"
#include "BtMmoClientCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "BtMmoClient.h"
#include "Widgets/Input/SVirtualJoystick.h"

bool ABtMmoClientPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Key == EKeys::LeftMouseButton)
	{
		if (Params.Event == IE_Pressed || Params.Event == IE_DoubleClick)
		{
			const auto* PlayerCharacter = Cast<ABtMmoClientCharacter>(GetPawn());
			bSuppressAttackClick = false;
			if (PlayerCharacter && !PlayerCharacter->IsDead() && !PlayerCharacter->IsRemotePlayer())
			{
				const double Now = GetWorld()->GetRealTimeSeconds();
				bSuppressAttackClick = LastAttackClickTime >= 0.0 && Now - LastAttackClickTime < AttackClickDebounceSeconds;
				LastAttackClickTime = Now;
			}
			else
			{
				LastAttackClickTime = -1.0;
			}

			if (bSuppressAttackClick) return true;
		}
		else if (bSuppressAttackClick)
		{
			if (Params.Event == IE_Released) bSuppressAttackClick = false;
			return true;
		}
	}

	return Super::InputKey(Params);
}

void ABtMmoClientPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogBtMmoClient, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ABtMmoClientPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ABtMmoClientPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
