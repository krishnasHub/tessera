#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSTestRunner.generated.h"

/**
 * Base for a game's scripted self-tests: scenarios that drive the real game (real input handlers, AI, dialogue, UI)
 * and log what happened, so behaviour can be checked without anyone at the keyboard.
 *
 * A game derives from it and writes RunStep(): a small state machine over Step, waiting by setting Next
 * (seconds since start, T). Report() logs "[TEST <scenario> t=<T>] <line>"; a line containing FAIL fails the
 * scenario, and Quit() ends it with "done" (Tools/Tessera.ps1 test reads these lines). It keeps ticking while
 * the game is paused (menus, dialogue).
 */
UCLASS(Abstract)
class TESSERATEST_API ATSTestRunner : public AActor
{
	GENERATED_BODY()

public:
	ATSTestRunner();
	FString Scenario;
	virtual void Tick(float DeltaSeconds) override;

	/** Where screenshots go: Saved/<[Tessera] ShotFolder, default Screenshots/<CommandPrefix>>/<Name>.png */
	static FString ShotPath(const FString& Name);

protected:
	/** One step of the scenario, run once T reaches Next. */
	virtual void RunStep() {}

	void Report(const FString& Line) const;
	/** Finish: "done" after After seconds, then exit. */
	void Quit(float After);
	/** A screenshot (with UI) to ShotPath(Name). */
	void Shot(const FString& Name) const;
	/** A real left click at a desktop position, through Slate (as if the player clicked there). Brings this game's
	 *  window to the front first (parallel test windows overlap). */
	void ClickAt(const FVector2D& ScreenPos) const;
	/** The middle of the game window (desktop pixels). */
	FVector2D WindowCentre() const;

	float T = 0.f;        // seconds since the scenario started (real ticks, also while paused)
	int32 Step = 0;
	float Next = 0.f;     // RunStep waits until T reaches this

private:
	bool bQuitting = false, bDone = false;
};

/**
 * The automation switches, read from the command line with the game's prefix ([Tessera] CommandPrefix):
 *
 *   -<P>Test=<scenario>       spawn the game's runner class for that scenario
 *   -<P>Shot=<sec>            screenshot to ShotPath(<P>ShotName, default "shot") at <sec>, then quit
 *   -<P>Cam=X,Y,Z,Pitch,Yaw   view from a fixed camera instead of the player's
 *   -<P>QuitAfter=<sec>       exit after <sec> (logic-only runs with -nullrhi)
 *   -<P>NoInput               (read by the game) ignore the real keyboard and mouse
 */
namespace TSTestSwitches
{
	/** Call once the world and the player exist (e.g. the game mode's StartPlay). */
	TESSERATEST_API void Run(UWorld* World, TSubclassOf<ATSTestRunner> RunnerClass);
}
