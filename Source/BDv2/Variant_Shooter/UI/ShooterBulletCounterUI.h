// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterBulletCounterUI.generated.h"

class UTextBlock;
class UImage;
class UProgressBar;
class UTextureRenderTarget2D;
class ASceneCapture2D;

/**
 *  Simple bullet counter UI widget for a first person shooter game
 */
UCLASS(abstract)
class BDV2_API UShooterBulletCounterUI : public UUserWidget
{
	GENERATED_BODY()
	
public:

	/** Allows Blueprint to update sub-widgets with the new bullet count */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "UpdateBulletCounter"))
	void BP_UpdateBulletCounter(int32 MagazineSize, int32 BulletCount);

	/** Text widget named AmmoText in the Blueprint designer. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	/** Updates the magazine/reserve presentation, for example "30 / 90". */
	UFUNCTION(BlueprintCallable, Category="Shooter")
	void UpdateAmmo(int32 BulletCount, int32 ReserveBulletCount);

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UImage> IMG_Minimap;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TXT_MinimapPlayer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_Stamina;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Allows Blueprint to update sub-widgets with the new life total and play a damage effect on the HUD */
	UFUNCTION(BlueprintImplementableEvent, Category="Shooter", meta=(DisplayName = "Damaged"))
	void BP_Damaged(float LifePercent);

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MinimapRenderTarget;
	UPROPERTY(Transient)
	TObjectPtr<ASceneCapture2D> MinimapCapture;
};
