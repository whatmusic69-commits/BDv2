#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BinaryDawnInGameMenu.generated.h"

class UWidgetSwitcher;
class UButton;

UCLASS(Blueprintable)
class BDV2_API UBinaryDawnInGameMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void SetPage(int32 PageIndex);
	void PrepareForOpen() { bIgnoreOpeningKey = true; }

protected:
	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> MainPages;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> NavigationButtons;
	bool bIgnoreOpeningKey = true;

	UFUNCTION()
	void OnHomeClicked();
	UFUNCTION()
	void OnMapClicked();
	UFUNCTION()
	void OnQuestClicked();
	UFUNCTION()
	void OnCodexClicked();
	UFUNCTION()
	void OnContinueClicked();
	UFUNCTION()
	void OnSaveClicked();
	UFUNCTION() void OnLoadClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnExitMainClicked();
	UFUNCTION() void OnExitGameClicked();

	UButton* MakeButton(const FString& Label, const FLinearColor& Color);
	void CloseMenu();
};
