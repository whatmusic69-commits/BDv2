// Copyright Epic Games, Inc. All Rights Reserved.


#include "BDv2PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "BDv2CameraManager.h"
#include "Blueprint/UserWidget.h"
#include "BDv2.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "InputCoreTypes.h"
#include "Variant_Shooter/UI/BinaryDawnInGameMenu.h"

ABDv2PlayerController::ABDv2PlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ABDv2CameraManager::StaticClass();
}

void ABDv2PlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogBDv2, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ABDv2PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ABDv2PlayerController::ToggleInGameMenu);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
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

void ABDv2PlayerController::ToggleInGameMenu()
{
	if (!IsLocalController()) return;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("P PRESSED"));
	if (bInGameMenuOpen) { CloseInGameMenu(); return; }
	if (!InGameMenuWidget)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("CREATING MENU"));
		InGameMenuWidget = CreateWidget<UBinaryDawnInGameMenu>(this, UBinaryDawnInGameMenu::StaticClass());
	}
	if (!InGameMenuWidget) return;
	InGameMenuWidget->PrepareForOpen();
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("MENU CREATED"));
	InGameMenuWidget->AddToViewport(1000);
	InGameMenuWidget->SetDesiredSizeInViewport(FVector2D(1920.0f, 1080.0f));
	InGameMenuWidget->SetVisibility(ESlateVisibility::Visible);
	InGameMenuWidget->SetIsEnabled(true);
	InGameMenuWidget->SetRenderOpacity(1.0f);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("ADDED TO VIEWPORT"));
	InGameMenuWidget->SetPage(0);
	bInGameMenuOpen = true;
	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(InGameMenuWidget->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	SetPause(true);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, TEXT("GAME PAUSED"));
}

void ABDv2PlayerController::CloseInGameMenu()
{
	if (InGameMenuWidget) InGameMenuWidget->RemoveFromParent();
	bInGameMenuOpen = false;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	SetPause(false);
}

bool ABDv2PlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
