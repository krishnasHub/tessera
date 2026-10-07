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

/**
 * Title screen: the game's name and tagline over whatever the camera shows (a slow drift over the world),
 * a soft dark band behind the text (<world>.assets.titleFade), Start New Game / Quit.
 * Mouse, arrows / W S / wheel + Enter, d-pad + A. Words: <world>.text { startNewGame, quit, menuHint }.
 */
class TESSERAUI_API STSTitle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSTitle) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
		SLATE_ARGUMENT(FString, Title)
		SLATE_ARGUMENT(FString, Tagline)
		SLATE_ARGUMENT(FString, Footer)          // small print, bottom right
		SLATE_EVENT(FSimpleDelegate, OnStart)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override;
	virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override;
	virtual void Tick(const FGeometry& G, const double Time, const float Dt) override;

private:
	FSimpleDelegate OnStart, OnQuit;
	FTSChoose Fx;
	FSlateBrush FadeBrush;
	TStrongObjectPtr<UTexture2D> FadeTex;
	int32 Highlight = 0;
	void Activate(int32 Index);
};

/**
 * Pause menu: Resume / New Game / Quit Game over a dimmed, paused world. New Game and Quit ask to confirm
 * (Cancel is highlighted, so a slip of Enter loses nothing). Mouse, arrows / W S / wheel + Enter, Esc to go back;
 * d-pad + A / B. Words: <world>.text { paused, pausedNote, resume, newGame, quitGame, confirmNew, confirmQuit,
 * confirmNewYes, confirmQuitYes, cancel, progressLost, pauseHint, confirmHint }.
 */
class TESSERAUI_API STSPauseMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSPauseMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
		SLATE_EVENT(FSimpleDelegate, OnResume)
		SLATE_EVENT(FSimpleDelegate, OnNewGame)
		SLATE_EVENT(FSimpleDelegate, OnQuit)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	/** Show the main page with Resume highlighted. */
	void Open();
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override;
	virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override;
	virtual void Tick(const FGeometry& G, const double Time, const float Dt) override;

private:
	enum class EPage : uint8 { Main, ConfirmNew, ConfirmQuit };
	EPage Page = EPage::Main;
	int32 Highlight = 0;
	FTSChoose Fx;
	void Choose(int32 Index) { Fx.Start(Index, [this, Index]() { Activate(Index); }); }
	FString Word(const TCHAR* Key, const TCHAR* Default) const;
	TWeakObjectPtr<UWorld> World;
	FSimpleDelegate OnResume, OnNewGame, OnQuit;
	TSharedPtr<SVerticalBox> List;
	TArray<FString> Items() const;
	void Rebuild();
	void Activate(int32 Index);
	void Back();
};
