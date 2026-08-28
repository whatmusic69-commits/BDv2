// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/ShooterPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "ShooterCharacter.h"
#include "ShooterBulletCounterUI.h"
#include "UI/BinaryDawnInGameMenu.h"
#include "BDv2.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "InputCoreTypes.h"
#include "UI/BinaryDawnSaveGame.h"

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AShooterPlayerController::ToggleInGameMenu);
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping contexts
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

		if (ShouldUseTouchControls())
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

		// create the bullet counter widget and add it to the screen
		BulletCounterUI = CreateWidget<UShooterBulletCounterUI>(this, BulletCounterUIClass);

		if (BulletCounterUI)
		{
			BulletCounterUI->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogBDv2, Error, TEXT("Could not spawn bullet counter widget."));

		}
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	InPawn->OnDestroyed.AddDynamic(this, &AShooterPlayerController::OnPawnDestroyed);

	// is this a shooter character?
	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn))
	{
		// add the player tag
		ShooterCharacter->Tags.Add(PlayerPawnTag);

		// set the team
		ShooterCharacter->SetTeam(TeamByte);

		// subscribe to the pawn's delegates
		ShooterCharacter->OnBulletCountUpdated.AddDynamic(this, &AShooterPlayerController::OnBulletCountUpdated);
		ShooterCharacter->OnDamaged.AddDynamic(this, &AShooterPlayerController::OnPawnDamaged);

		// force update the life bar
		ShooterCharacter->OnDamaged.Broadcast(1.0f);
		ShooterCharacter->RefreshWeaponHUD();
	}
}

void AShooterPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// reset the bullet counter HUD
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_UpdateBulletCounter(0, 0);
		BulletCounterUI->UpdateAmmo(0, 0);
	}

	if (TeamByte < TeamTags.Num())
	{
		// find the player start
		TArray<AActor*> ActorList;
		UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), TeamTags[TeamByte], ActorList);

		if (ActorList.Num() > 0)
		{
			// select a random player start
			AActor* RandomPlayerStart = ActorList[FMath::RandRange(0, ActorList.Num() - 1)];

			// spawn a character at the player start
			const FTransform SpawnTransform = RandomPlayerStart->GetActorTransform();

			if (AShooterCharacter* RespawnedCharacter = GetWorld()->SpawnActor<AShooterCharacter>(CharacterClass, SpawnTransform))
			{
				// possess the character
				Possess(RespawnedCharacter);
			}
		}
	}
}

void AShooterPlayerController::OnBulletCountUpdated(int32 MagazineSize, int32 Bullets, int32 ReserveBullets)
{
	// update the UI
	if (BulletCounterUI)
	{
		BulletCounterUI->BP_UpdateBulletCounter(MagazineSize, Bullets);
		BulletCounterUI->UpdateAmmo(Bullets, ReserveBullets);
	}
}

void AShooterPlayerController::OnPawnDamaged(float LifePercent)
{
	if (IsValid(BulletCounterUI))
	{
		BulletCounterUI->BP_Damaged(LifePercent);
	}
}

bool AShooterPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AShooterPlayerController::SetTeam(uint8 Team)
{
	TeamByte = Team;

	// if we already have a pawn, set its team
	if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterCharacter->SetTeam(Team);
	}
}

void AShooterPlayerController::ToggleInGameMenu()
{
	if (!IsLocalController()) return;
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, TEXT("P PRESSED"));
	if (bInGameMenuOpen)
	{
		CloseInGameMenu();
		return;
	}

	if (!InGameMenuWidget)
	{
		InGameMenuWidget = CreateWidget<UBinaryDawnInGameMenu>(this, UBinaryDawnInGameMenu::StaticClass());
	}
	if (!InGameMenuWidget) return;
	InGameMenuWidget->PrepareForOpen();

	InGameMenuWidget->AddToViewport(1000);
	InGameMenuWidget->SetDesiredSizeInViewport(FVector2D(1920.0f, 1080.0f));
	InGameMenuWidget->SetVisibility(ESlateVisibility::Visible);
	InGameMenuWidget->SetIsEnabled(true);
	InGameMenuWidget->SetRenderOpacity(1.0f);
	InGameMenuWidget->SetPage(0);
	bInGameMenuOpen = true;
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(InGameMenuWidget->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	SetPause(true);
}

void AShooterPlayerController::CloseInGameMenu()
{
	if (!bInGameMenuOpen && !InGameMenuWidget) return;
	if (InGameMenuWidget)
	{
		InGameMenuWidget->RemoveFromParent();
	}
	bInGameMenuOpen = false;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	SetPause(false);
}

bool AShooterPlayerController::SaveBinaryDawnGame(const FString& SlotName)
{
	if (!GetWorld() || !GetPawn()) return false;
	UBinaryDawnSaveGame* Save = Cast<UBinaryDawnSaveGame>(UGameplayStatics::CreateSaveGameObject(UBinaryDawnSaveGame::StaticClass()));
	if (!Save) return false;
	Save->MapName = GetWorld()->GetMapName();
	Save->PlayerTransform = GetPawn()->GetActorTransform();
	Save->SavedAt = FDateTime::Now();
	Save->GameTimeSeconds = GetWorld()->GetTimeSeconds();
	return UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

bool AShooterPlayerController::LoadBinaryDawnGame(const FString& SlotName)
{
	if (!GetWorld() || !UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	UBinaryDawnSaveGame* Save = Cast<UBinaryDawnSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save || !GetPawn()) return false;
	GetPawn()->SetActorTransform(Save->PlayerTransform);
	return true;
}
