#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "TSChoose.h"

class UWorld;
class UTexture2D;
class SVerticalBox;

/** One reply as the dialogue box shows it: "1. [Verb] Text   odds". */
struct FTSDialogueChoice
{
	FString Text;
	FString Verb;                    // "" = no tag
	FLinearColor VerbColor = FTSUIStyle::Get().Accent;
	bool bEnabled = true;
	FString Odds;                    // shown greyed on the right (debug), usually ""
};

/**
 * Where the dialogue box reads the conversation from. The box knows nothing about the story system: a game fills
 * this from its own (with Loom: SpeakerInfo(), DialogueText, ChoiceViews, Choose(), CloseDialogue()) and calls
 * Refresh() when the line changes.
 */
struct FTSDialogueView
{
	TFunction<bool()> IsOpen;
	TFunction<FString()> SpeakerName;
	TFunction<FLinearColor()> SpeakerColor;
	TFunction<FString()> Text;
	TFunction<TArray<FTSDialogueChoice>()> Choices;
	TFunction<void(int32 Index)> Choose;
	TFunction<void()> Close;
	/** Optional portraits: the other speaker (bHero false) on the left, the hero (mirrored) on the right. */
	TFunction<UTexture2D*(bool bHero)> Portrait;
};

/**
 * The conversation box: speaker, line, numbered replies with [Verb] tags; the world darkens around it
 * (<world>.assets.dialogueFade, a radial fade texture) and optional portraits slide up beside it.
 * Keys: 1-9 pick; wheel, arrows, W/S or d-pad highlight; Enter, Space, E or gamepad A confirm; Esc or B closes.
 * A choice flashes, fades, then happens (FTSChoose). Modal: keeps keyboard focus while open.
 */
class TESSERAUI_API STSDialogueBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSDialogueBox) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
		SLATE_ARGUMENT(FTSDialogueView, View)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	/** Rebuild the replies (a new line, or just opened). */
	void Refresh();
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override;
	virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override;
	virtual void Tick(const FGeometry& G, const double Time, const float Dt) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	/** Where reply I is on screen (desktop pixels), for tests that click it. */
	FVector2D ChoiceScreenCenter(int32 I) const;

private:
	bool IsOpen() const { return View.IsOpen && View.IsOpen(); }
	/** Move the highlighted reply by Step, skipping disabled ones. */
	void MoveHighlight(int32 Step);
	/** Choose a reply (click, 1-9, Enter): it flashes, fades, then the story moves on. */
	void Confirm(int32 Index);
	void SetPortrait(FSlateBrush& Brush, TStrongObjectPtr<UTexture2D>& Keep, bool bHero);

	TWeakObjectPtr<UWorld> World;
	FTSDialogueView View;
	TArray<FTSDialogueChoice> Shown;
	TSharedPtr<SVerticalBox> Choices;
	int32 Highlight = 0;
	FTSChoose Fx;
	FSlateBrush NpcBrush, HeroBrush;
	TStrongObjectPtr<UTexture2D> NpcTex, HeroTex;
	TSharedPtr<SWidget> NpcPortrait, HeroPortrait;
	TSharedPtr<SWidget> DialogBox;     // the darkness behind the conversation is centred on it
	float FadeIn = 0.f;
	FSlateBrush FadeBrush;
	TStrongObjectPtr<UTexture2D> FadeTex;
	float Appear = 0.f;
	bool bWasOpen = false;
};
