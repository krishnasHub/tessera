#include "STSMenus.h"
#include "TSAssets.h"
#include "TSData.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

// =============================================================================================
// Title screen
// =============================================================================================

void STSTitle::Construct(const FArguments& Args)
{
	OnStart = Args._OnStart;
	OnQuit = Args._OnQuit;
	const UObject* Ctx = Args._World.Get();
	if (UTexture2D* Fade = TSAssets::Get<UTexture2D>(Ctx, TEXT("titleFade")))
	{
		FadeTex.Reset(Fade);
		FadeBrush.SetResourceObject(Fade);
		FadeBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	const FTSUIStyle& St = FTSUIStyle::Get();
	auto Item = [this, &St](int32 I, const FString& Label)
	{
		return SNew(SButton).IsFocusable(false).ContentPadding(FMargin(18, 10)).HAlign(HAlign_Left)
			.ButtonColorAndOpacity_Lambda([this, I]() { return Fx.Color(I, Highlight); })
			.OnHovered_Lambda([this, I]() { if (!Fx.Busy()) Highlight = I; })
			.OnClicked_Lambda([this, I]() { Activate(I); return FReply::Handled(); })
			[
				SNew(STextBlock).Font(TSUI::Font(20, TEXT("Bold"))).Text(TSUI::Text(Label)).ColorAndOpacity(St.Text)
			];
	};
	ChildSlot
	[
		SNew(SOverlay)
		// A soft dark band down the left so the text reads over the world.
		+ SOverlay::Slot().HAlign(HAlign_Left)[ SNew(SBox).WidthOverride(1100)[ SNew(SImage).Image(&FadeBrush).ColorAndOpacity(FLinearColor(1, 1, 1, 0.85f)) ] ]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(90, 0, 0, 40)
		[
			SNew(SBox).WidthOverride(560)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Font(TSUI::Font(64, TEXT("Bold"))).ColorAndOpacity(St.Accent).ShadowOffset(FVector2D(3, 3)).Text(TSUI::Text(Args._Title)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(4, 2, 0, 46)[ SNew(STextBlock).Font(TSUI::Font(16)).ColorAndOpacity(FLinearColor(0.88f, 0.86f, 0.8f)).ShadowOffset(FVector2D(1, 1)).Text(TSUI::Text(Args._Tagline)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[ SNew(SBox).WidthOverride(340)[ Item(0, TSText::Get(Ctx, TEXT("startNewGame"), TEXT("Start New Game"))) ] ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)[ SNew(SBox).WidthOverride(340)[ Item(1, TSText::Get(Ctx, TEXT("quit"), TEXT("Quit"))) ] ]
				+ SVerticalBox::Slot().AutoHeight().Padding(4, 30, 0, 0)
				[ SNew(STextBlock).Font(TSUI::Font(11)).ColorAndOpacity(St.Muted).Text(TSUI::Text(TSText::Get(Ctx, TEXT("menuHint"), TEXT("↑↓ or wheel, Enter to choose")))) ]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0, 0, 24, 16)
		[ SNew(STextBlock).Font(TSUI::Font(10)).ColorAndOpacity(St.Muted).Text(TSUI::Text(Args._Footer)) ]
	];
}

void STSTitle::Activate(int32 Index)
{
	Highlight = Index;
	Fx.Start(Index, [this, Index]() { if (Index == 0) OnStart.ExecuteIfBound(); else OnQuit.ExecuteIfBound(); });
}

FReply STSTitle::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	if (Fx.Busy()) return FReply::Handled();
	const FKey K = E.GetKey();
	if (K == EKeys::Up || K == EKeys::W || K == EKeys::Gamepad_DPad_Up || K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down) { Highlight = 1 - Highlight; return FReply::Handled(); }
	if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::Gamepad_FaceButton_Bottom) { Activate(Highlight); return FReply::Handled(); }
	return FReply::Unhandled();
}

FReply STSTitle::OnMouseWheel(const FGeometry& G, const FPointerEvent& E)
{
	if (!Fx.Busy()) Highlight = 1 - Highlight;
	return FReply::Handled();
}

void STSTitle::Tick(const FGeometry& G, const double Time, const float Dt)
{
	SCompoundWidget::Tick(G, Time, Dt);
	SetRenderOpacity(Fx.Tick(Dt));
	if (!HasKeyboardFocus() && GetVisibility().IsVisible()) FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);
}

// =============================================================================================
// Pause menu
// =============================================================================================

FString STSPauseMenu::Word(const TCHAR* Key, const TCHAR* Default) const { return TSText::Get(World.Get(), Key, Default); }

void STSPauseMenu::Construct(const FArguments& Args)
{
	World = Args._World;
	OnResume = Args._OnResume;
	OnNewGame = Args._OnNewGame;
	OnQuit = Args._OnQuit;
	const FTSUIStyle& St = FTSUIStyle::Get();
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[ SNew(SImage).Image(TSUI::White()).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.02f, 0.55f)) ]   // dim the paused world
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(440)
			[
				SNew(SBorder).BorderImage(TSUI::White()).BorderBackgroundColor(St.Panel).Padding(FMargin(30, 24))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock).Font(TSUI::Font(26, TEXT("Bold"))).ColorAndOpacity(St.Accent)
						.Text_Lambda([this]() { return TSUI::Text(Page == EPage::Main ? Word(TEXT("paused"), TEXT("Paused"))
							: Page == EPage::ConfirmNew ? Word(TEXT("confirmNew"), TEXT("Start a new game?")) : Word(TEXT("confirmQuit"), TEXT("Quit the game?"))); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 6, 0, 18)
					[
						SNew(STextBlock).Font(TSUI::Font(12)).ColorAndOpacity(St.Muted).Justification(ETextJustify::Center).AutoWrapText(true)
						.Text_Lambda([this]() { return TSUI::Text(Page == EPage::Main ? Word(TEXT("pausedNote"), TEXT("The world waits for you."))
							: Word(TEXT("progressLost"), TEXT("Your current progress will be lost."))); })
					]
					+ SVerticalBox::Slot().AutoHeight()[ SAssignNew(List, SVerticalBox) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 16, 0, 0)
					[
						SNew(STextBlock).Font(TSUI::Font(10)).ColorAndOpacity(St.Muted)
						.Text_Lambda([this]() { return TSUI::Text(Page == EPage::Main ? Word(TEXT("pauseHint"), TEXT("Esc to resume   ·   ↑↓ or wheel, Enter to choose"))
							: Word(TEXT("confirmHint"), TEXT("Esc to go back   ·   ↑↓ or wheel, Enter to choose"))); })
					]
				]
			]
		]
	];
	Rebuild();
}

TArray<FString> STSPauseMenu::Items() const
{
	if (Page == EPage::ConfirmNew) return { Word(TEXT("confirmNewYes"), TEXT("Yes, start over")), Word(TEXT("cancel"), TEXT("Cancel")) };
	if (Page == EPage::ConfirmQuit) return { Word(TEXT("confirmQuitYes"), TEXT("Yes, quit")), Word(TEXT("cancel"), TEXT("Cancel")) };
	return { Word(TEXT("resume"), TEXT("Resume")), Word(TEXT("newGame"), TEXT("New Game")), Word(TEXT("quitGame"), TEXT("Quit Game")) };
}

void STSPauseMenu::Rebuild()
{
	List->ClearChildren();
	const TArray<FString> Names = Items();
	for (int32 I = 0; I < Names.Num(); ++I)
	{
		List->AddSlot().AutoHeight().Padding(0, 4)
		[
			SNew(SButton).IsFocusable(false).HAlign(HAlign_Center).ContentPadding(FMargin(10, 9))
			.ButtonColorAndOpacity_Lambda([this, I]() { return Fx.Color(I, Highlight); })
			.OnHovered_Lambda([this, I]() { if (!Fx.Busy()) Highlight = I; })
			.OnClicked_Lambda([this, I]() { Choose(I); return FReply::Handled(); })
			[
				SNew(STextBlock).Font(TSUI::Font(16, TEXT("Bold"))).Text(TSUI::Text(Names[I])).ColorAndOpacity(FTSUIStyle::Get().Text)
			]
		];
	}
}

void STSPauseMenu::Open()
{
	Fx.Reset();
	Page = EPage::Main;
	Highlight = 0;
	Rebuild();
}

void STSPauseMenu::Back()
{
	if (Page == EPage::Main) { OnResume.ExecuteIfBound(); return; }
	Highlight = Page == EPage::ConfirmNew ? 1 : 2;   // back onto the item you came from
	Page = EPage::Main;
	Rebuild();
}

void STSPauseMenu::Activate(int32 Index)
{
	if (Page == EPage::Main)
	{
		if (Index == 0) { OnResume.ExecuteIfBound(); return; }
		Page = Index == 1 ? EPage::ConfirmNew : EPage::ConfirmQuit;
		Highlight = 1;   // default to Cancel: a slip of Enter shouldn't throw progress away
		Rebuild();
		return;
	}
	if (Index != 0) { Back(); return; }
	if (Page == EPage::ConfirmNew) OnNewGame.ExecuteIfBound();
	else OnQuit.ExecuteIfBound();
}

FReply STSPauseMenu::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	if (Fx.Busy()) return FReply::Handled();
	const FKey K = E.GetKey();
	const int32 N = Items().Num();
	if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right || K == EKeys::Gamepad_Special_Right) { Back(); return FReply::Handled(); }
	if (K == EKeys::Up || K == EKeys::W || K == EKeys::Gamepad_DPad_Up) { Highlight = (Highlight + N - 1) % N; return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down) { Highlight = (Highlight + 1) % N; return FReply::Handled(); }
	if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::E || K == EKeys::Gamepad_FaceButton_Bottom) { Choose(Highlight); return FReply::Handled(); }
	return FReply::Unhandled();
}

FReply STSPauseMenu::OnMouseWheel(const FGeometry& G, const FPointerEvent& E)
{
	const int32 N = Items().Num();
	Highlight = (Highlight + (E.GetWheelDelta() > 0.f ? N - 1 : 1)) % N;
	return FReply::Handled();
}

void STSPauseMenu::Tick(const FGeometry& G, const double Time, const float Dt)
{
	SCompoundWidget::Tick(G, Time, Dt);
	SetRenderOpacity(Fx.Tick(Dt));
	// Modal: keep keyboard focus while shown (a click elsewhere mustn't strand the keys).
	if (GetVisibility() == EVisibility::Visible && !HasKeyboardFocus()) FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);
}
