#include "BinaryDawnInGameMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterPlayerController.h"

namespace
{
	static UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(FText::FromString(Value));
		Label->SetColorAndOpacity(Color);
		Label->SetFont(FSlateFontInfo(static_cast<const UObject*>(nullptr), static_cast<float>(Size)));
		return Label;
	}
}

void UBinaryDawnInGameMenu::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (!WidgetTree) return;

	const FLinearColor Graphite(0.025f, 0.035f, 0.04f, 0.96f);
	const FLinearColor Amber(1.0f, 0.48f, 0.08f, 1.0f);
	const FLinearColor TextColor(0.88f, 0.91f, 0.88f, 1.0f);
	const FLinearColor Muted(0.42f, 0.63f, 0.62f, 1.0f);

	UCanvasPanel* FullScreen = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = FullScreen;
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Background->SetBrushColor(Graphite);
	Background->SetVisibility(ESlateVisibility::Visible);
	Background->SetRenderOpacity(1.0f);
	if (UCanvasPanelSlot* BackgroundSlot = FullScreen->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackgroundSlot->SetOffsets(FMargin(0.f));
	}
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Background->SetContent(Layout);

	UTextBlock* Title = Text(WidgetTree, TEXT("BINARY DAWN  //  PAUSE / JOURNAL"), 28, Amber);
	Title->SetJustification(ETextJustify::Center);
	Layout->AddChildToVerticalBox(Title);

	UHorizontalBox* Nav = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Layout->AddChildToVerticalBox(Nav);
	const TArray<FString> Labels = { TEXT("ГЛАВНОЕ"), TEXT("КАРТА"), TEXT("ЗАДАНИЯ"), TEXT("СПРАВОЧНИК") };
	for (const FString& Label : Labels)
	{
		UButton* Button = MakeButton(Label, TextColor);
		Nav->AddChildToHorizontalBox(Button);
		NavigationButtons.Add(Button);
	}
	NavigationButtons[0]->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnHomeClicked);
	NavigationButtons[1]->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnMapClicked);
	NavigationButtons[2]->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnQuestClicked);
	NavigationButtons[3]->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnCodexClicked);

	MainPages = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	Layout->AddChildToVerticalBox(MainPages);

	UVerticalBox* Home = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Home->AddChildToVerticalBox(Text(WidgetTree, TEXT("ГЛАВНОЕ"), 24, TextColor));
	for (const FString& Label : { TEXT("ПРОДОЛЖИТЬ"), TEXT("СОХРАНИТЬ ИГРУ"), TEXT("ЗАГРУЗИТЬ ИГРУ"), TEXT("НАСТРОЙКИ"), TEXT("ВЫЙТИ В ГЛАВНОЕ МЕНЮ"), TEXT("ВЫЙТИ ИЗ ИГРЫ") })
	{
		UButton* Button = MakeButton(Label, TextColor);
		Home->AddChildToVerticalBox(Button);
		if (Label == TEXT("ПРОДОЛЖИТЬ")) Button->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnContinueClicked);
		if (Label == TEXT("СОХРАНИТЬ ИГРУ")) Button->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnSaveClicked);
		if (Label == TEXT("ЗАГРУЗИТЬ ИГРУ")) Button->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnLoadClicked);
		if (Label == TEXT("НАСТРОЙКИ")) Button->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnSettingsClicked);
		if (Label == TEXT("ВЫЙТИ В ГЛАВНОЕ МЕНЮ")) Button->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnExitMainClicked);
		if (Label == TEXT("ВЫЙТИ ИЗ ИГРЫ")) Button->OnClicked.AddDynamic(this, &UBinaryDawnInGameMenu::OnExitGameClicked);
	}
	MainPages->AddChild(Home);

	UVerticalBox* Map = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Map->AddChildToVerticalBox(Text(WidgetTree, TEXT("КАРТА МИРА"), 24, Amber));
	Map->AddChildToVerticalBox(Text(WidgetTree, TEXT("LV_BinaryDawn_World\nКарта и маркер игрока будут обновляться при открытии раздела."), 18, Muted));
	MainPages->AddChild(Map);

	UVerticalBox* Quests = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Quests->AddChildToVerticalBox(Text(WidgetTree, TEXT("ЗАДАНИЯ"), 24, Amber));
	Quests->AddChildToVerticalBox(Text(WidgetTree, TEXT("Журнал заданий Binary Dawn\nАктивные и завершённые задания появятся здесь."), 18, Muted));
	MainPages->AddChild(Quests);

	UVerticalBox* Codex = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Codex->AddChildToVerticalBox(Text(WidgetTree, TEXT("СПРАВОЧНИК"), 24, Amber));
	Codex->AddChildToVerticalBox(Text(WidgetTree, TEXT("Районы  •  Фракции  •  Оружие  •  Механики\nЗаблокированные записи отображаются как ???"), 18, Muted));
	MainPages->AddChild(Codex);

	SetPage(0);
	SetVisibility(ESlateVisibility::Visible);
	SetRenderOpacity(1.0f);
	SetKeyboardFocus();
}

UButton* UBinaryDawnInGameMenu::MakeButton(const FString& Label, const FLinearColor& Color)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(FButtonStyle());
	UTextBlock* TextBlock = Text(WidgetTree, Label, 18, Color);
	TextBlock->SetJustification(ETextJustify::Center);
	Button->SetContent(TextBlock);
	return Button;
}

void UBinaryDawnInGameMenu::SetPage(int32 PageIndex)
{
	if (MainPages) MainPages->SetActiveWidgetIndex(PageIndex);
}

void UBinaryDawnInGameMenu::OnHomeClicked() { SetPage(0); }
void UBinaryDawnInGameMenu::OnMapClicked() { SetPage(1); }
void UBinaryDawnInGameMenu::OnQuestClicked() { SetPage(2); }
void UBinaryDawnInGameMenu::OnCodexClicked() { SetPage(3); }
void UBinaryDawnInGameMenu::OnContinueClicked() { CloseMenu(); }
void UBinaryDawnInGameMenu::OnSaveClicked()
{
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		if (PC->SaveBinaryDawnGame(TEXT("QuickSave")))
		{
			UE_LOG(LogTemp, Log, TEXT("BINARY DAWN: ИГРА СОХРАНЕНА (QuickSave)"));
		}
	}
}

void UBinaryDawnInGameMenu::OnLoadClicked()
{
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		if (PC->LoadBinaryDawnGame(TEXT("QuickSave")))
		{
			UE_LOG(LogTemp, Log, TEXT("BINARY DAWN: QuickSave загружен"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BINARY DAWN: сохранение QuickSave не найдено"));
		}
	}
}

void UBinaryDawnInGameMenu::OnSettingsClicked()
{
	UE_LOG(LogTemp, Log, TEXT("BINARY DAWN: настройки доступны через WBP_Settings"));
}

void UBinaryDawnInGameMenu::OnExitMainClicked()
{
	CloseMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("LV_MainMenu")));
}

void UBinaryDawnInGameMenu::OnExitGameClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = GetOwningPlayer()) PC->ConsoleCommand(TEXT("quit"));
	}
}

void UBinaryDawnInGameMenu::CloseMenu()
{
	if (AShooterPlayerController* PC = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		PC->CloseInGameMenu();
		return;
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		RemoveFromParent();
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetPause(false);
	}
}

FReply UBinaryDawnInGameMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::P)
	{
		if (bIgnoreOpeningKey)
		{
			bIgnoreOpeningKey = false;
			return FReply::Handled();
		}
		CloseMenu();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
