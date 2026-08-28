// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterBulletCounterUI.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Variant_Shooter/ShooterCharacter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UShooterBulletCounterUI::NativeConstruct()
{
	Super::NativeConstruct();
	if (!GetWorld() || !IMG_Minimap) return;
	MinimapRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	MinimapRenderTarget->InitAutoFormat(512, 512);
	MinimapRenderTarget->ClearColor = FLinearColor(0.04f, 0.04f, 0.04f, 1.0f);
	MinimapRenderTarget->UpdateResourceImmediate(true);
	MinimapCapture = GetWorld()->SpawnActor<ASceneCapture2D>();
	if (MinimapCapture)
	{
		USceneCaptureComponent2D* Capture = MinimapCapture->GetCaptureComponent2D();
		Capture->ProjectionType = ECameraProjectionMode::Orthographic;
		Capture->OrthoWidth = 3000.0f;
		Capture->TextureTarget = MinimapRenderTarget;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = true;
		Capture->bCaptureOnMovement = true;
	}
	FSlateBrush Brush;
	Brush.SetResourceObject(MinimapRenderTarget);
	Brush.ImageSize = FVector2D(220.0f, 220.0f);
	IMG_Minimap->SetBrush(Brush);
}

void UShooterBulletCounterUI::NativeDestruct()
{
	if (MinimapCapture)
	{
		MinimapCapture->Destroy();
		MinimapCapture = nullptr;
	}
	MinimapRenderTarget = nullptr;
	Super::NativeDestruct();
}

void UShooterBulletCounterUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn) return;
	if (PB_Stamina)
	{
		if (AShooterCharacter* Shooter = Cast<AShooterCharacter>(Pawn))
		{
			PB_Stamina->SetPercent(Shooter->GetStaminaPercent());
		}
	}
	if (MinimapCapture)
	{
		MinimapCapture->SetActorLocation(Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 2500.0f));
		MinimapCapture->SetActorRotation(FRotator(-90.0f, 0.0f, 0.0f));
	}
	if (TXT_MinimapPlayer)
	{
		const float Yaw = Pawn->GetControlRotation().Yaw;
		TXT_MinimapPlayer->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.0f, 1.0f), FVector2D::ZeroVector, -Yaw));
	}
}

void UShooterBulletCounterUI::UpdateAmmo(int32 BulletCount, int32 ReserveBulletCount)
{
	if (AmmoText)
	{
		AmmoText->SetVisibility((BulletCount == 0 && ReserveBulletCount == 0) ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		AmmoText->SetText(FText::Format(
			NSLOCTEXT("ShooterHUD", "AmmoFormat", "{0} / {1}"),
			FText::AsNumber(BulletCount),
			FText::AsNumber(ReserveBulletCount)));
	}
}
