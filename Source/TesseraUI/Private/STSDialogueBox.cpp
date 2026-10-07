#include "STSDialogueBox.h"
#include "TSAssets.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

void STSDialogueBox::Construct(const FArguments& Args)
{
	World = Args._World;
	View = Args._View;
	if (UTexture2D* Fade = TSAssets::Get<UTexture2D>(World.Get(), TEXT("dialogueFade")))
	{
		FadeTex.Reset(Fade);
		FadeBrush.SetResourceObject(Fade);
		FadeBrush.DrawAs = ESlateBrushDrawType::Image;
	}
	const FTSUIStyle& St = FTSUIStyle::Get();
	constexpr float PortraitH = 580.f;
	auto Portrait = [](FSlateBrush* Brush) -> TSharedRef<SWidget>
	{
		return SNew(SBox).HeightOverride(PortraitH).WidthOverride(PortraitH * 1.1f)
			[ SNew(SScaleBox).Stretch(EStretch::ScaleToFit).VAlign(VAlign_Bottom)[ SNew(SImage).Image(Brush) ] ];
	};
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(10, 0, 0, 0)
		[ SAssignNew(NpcPortrait, SBox).Visibility(EVisibility::HitTestInvisible)[ Portrait(&NpcBrush) ] ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0, 0, 10, 0)
		[ SAssignNew(HeroPortrait, SBox).Visibility(EVisibility::HitTestInvisible).RenderTransformPivot(FVector2D(0.5f, 0.5f))[ Portrait(&HeroBrush) ] ]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 140)
		[
			SAssignNew(DialogBox, SBox).WidthOverride(820)
			[
				SNew(SBorder).BorderImage(TSUI::White()).BorderBackgroundColor(St.Panel).Padding(FMargin(22, 16))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
					[
						SNew(STextBlock).Font(TSUI::Font(15, TEXT("Bold")))
						.Text_Lambda([this]() { return View.SpeakerName ? TSUI::Text(View.SpeakerName()) : FText::GetEmpty(); })
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(View.SpeakerColor ? View.SpeakerColor() : FLinearColor::White); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
					[
						SNew(STextBlock).Font(TSUI::Font(13)).AutoWrapText(true).ColorAndOpacity(St.Body)
						.Text_Lambda([this]() { return View.Text ? TSUI::Text(View.Text()) : FText::GetEmpty(); })
					]
					+ SVerticalBox::Slot().AutoHeight()[ SAssignNew(Choices, SVerticalBox) ]
				]
			]
		]
	];
}

void STSDialogueBox::SetPortrait(FSlateBrush& Brush, TStrongObjectPtr<UTexture2D>& Keep, bool bHero)
{
	UTexture2D* Tex = View.Portrait ? View.Portrait(bHero) : nullptr;
	Keep.Reset(Tex);
	TSUI::SetTextureBrush(Brush, Tex);
}

void STSDialogueBox::Refresh()
{
	Choices->ClearChildren();
	if (!IsOpen()) return;
	if (!Fx.Busy()) Fx.Reset();   // a new line fades in
	if (!bWasOpen)   // just opened: who's talking, and slide the portraits in
	{
		SetPortrait(NpcBrush, NpcTex, false);
		SetPortrait(HeroBrush, HeroTex, true);
		Appear = 0.f;
	}
	bWasOpen = true;
	Shown = View.Choices ? View.Choices() : TArray<FTSDialogueChoice>();
	Highlight = -1;
	MoveHighlight(1);   // first enabled reply
	const FTSUIStyle& St = FTSUIStyle::Get();
	for (int32 I = 0; I < Shown.Num(); ++I)
	{
		const FTSDialogueChoice& V = Shown[I];
		Choices->AddSlot().AutoHeight().Padding(0, 3)
		[
			SNew(SButton).IsFocusable(false).IsEnabled(V.bEnabled)
			.ButtonColorAndOpacity_Lambda([this, I]() { return Fx.Color(I, Highlight); })
			.OnHovered_Lambda([this, I, bOn = V.bEnabled]() { if (bOn && !Fx.Busy()) Highlight = I; })
			.OnClicked_Lambda([this, I]() { Confirm(I); return FReply::Handled(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4, 2)[ SNew(STextBlock).Font(TSUI::Font(12)).ColorAndOpacity(St.Muted).Text(TSUI::Text(FString::Printf(TEXT("%d."), I + 1))) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2, 2)
				[ SNew(STextBlock).Font(TSUI::Font(12, TEXT("Bold"))).ColorAndOpacity(V.VerbColor).Text(TSUI::Text(V.Verb.IsEmpty() ? FString() : TEXT("[") + V.Verb + TEXT("]"))) ]
				+ SHorizontalBox::Slot().FillWidth(1).Padding(4, 2)[ SNew(STextBlock).Font(TSUI::Font(12)).AutoWrapText(true).Text(TSUI::Text(V.Text)) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6, 2)[ SNew(STextBlock).Font(TSUI::Font(11)).ColorAndOpacity(St.Muted).Text(TSUI::Text(V.Odds)) ]
			]
		];
	}
}

int32 STSDialogueBox::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	// Behind the conversation, the world darkens around the dialogue box and fades back to clear away from it.
	if (DialogBox && FadeTex)
	{
		const FGeometry& BG = DialogBox->GetPaintSpaceGeometry();
		const FVector2D Size = BG.GetLocalSize();
		if (Size.X > 1.f)
		{
			const FVector2D Centre = G.AbsoluteToLocal(BG.LocalToAbsolute(Size * 0.5f));
			const FVector2D FadeSize(Size.X * 2.6f, Size.Y * 4.2f);
			FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FadeSize, FSlateLayoutTransform(Centre - FadeSize * 0.5f)), &FadeBrush,
				ESlateDrawEffect::None, FLinearColor(1, 1, 1, FadeIn));
		}
	}
	return SCompoundWidget::OnPaint(Args, G, Cull, Out, Layer + 1, Style, bParentEnabled);
}

FVector2D STSDialogueBox::ChoiceScreenCenter(int32 I) const
{
	FChildren* Kids = Choices->GetChildren();
	if (!Kids || I < 0 || I >= Kids->Num()) return FVector2D::ZeroVector;
	return Kids->GetChildAt(I)->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
}

void STSDialogueBox::Confirm(int32 Index)
{
	if (Fx.Busy() || !IsOpen() || !Shown.IsValidIndex(Index) || !Shown[Index].bEnabled) return;
	Highlight = Index;
	Fx.Start(Index, [this, Index]() { if (IsOpen() && View.Choose) View.Choose(Index); });
}

void STSDialogueBox::Tick(const FGeometry& G, const double Time, const float Dt)
{
	SCompoundWidget::Tick(G, Time, Dt);
	SetRenderOpacity(Fx.Tick(Dt));
	const bool bOpen = IsOpen();
	if (!bOpen) bWasOpen = false;
	Appear = FMath::Min(1.f, Appear + Dt * 4.f);
	const float Ease = 1.f - FMath::Square(1.f - Appear);
	const float Rise = (1.f - Ease) * 220.f;
	FadeIn = Ease;
	if (NpcPortrait) { NpcPortrait->SetRenderTransform(FSlateRenderTransform(FVector2D(-Rise * 0.5f, Rise))); NpcPortrait->SetRenderOpacity(Ease); }
	if (HeroPortrait)   // mirrored so the hero faces the person they're talking to
	{
		HeroPortrait->SetRenderTransform(FSlateRenderTransform(FScale2D(-1.f, 1.f), FVector2D(Rise * 0.5f, Rise)));
		HeroPortrait->SetRenderOpacity(Ease);
	}
	// Modal: if a click (or anything else) took keyboard focus away, take it back so 1-9 / Esc keep working.
	if (bOpen && !HasKeyboardFocus()) FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);
}

FReply STSDialogueBox::OnKeyDown(const FGeometry& G, const FKeyEvent& E)
{
	if (!IsOpen()) return FReply::Unhandled();
	static const FKey Digits[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	if (Fx.Busy()) return FReply::Handled();
	for (int32 I = 0; I < 9; ++I) if (E.GetKey() == Digits[I]) { Confirm(I); return FReply::Handled(); }
	const FKey K = E.GetKey();
	if (K == EKeys::Up || K == EKeys::W || K == EKeys::Gamepad_DPad_Up) { MoveHighlight(-1); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down) { MoveHighlight(1); return FReply::Handled(); }
	if ((K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::E || K == EKeys::Gamepad_FaceButton_Bottom) && Shown.IsValidIndex(Highlight))
	{
		Confirm(Highlight);
		return FReply::Handled();
	}
	if ((K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right) && View.Close) { View.Close(); return FReply::Handled(); }
	return FReply::Unhandled();
}

FReply STSDialogueBox::OnMouseWheel(const FGeometry& G, const FPointerEvent& E)
{
	if (!IsOpen()) return FReply::Unhandled();
	MoveHighlight(E.GetWheelDelta() > 0.f ? -1 : 1);
	return FReply::Handled();
}

void STSDialogueBox::MoveHighlight(int32 Step)
{
	const int32 N = Shown.Num();
	for (int32 I = 1; I <= N; ++I)
	{
		const int32 C = ((Highlight + Step * I) % N + N) % N;   // wraps around
		if (Shown[C].bEnabled) { Highlight = C; return; }
	}
}
