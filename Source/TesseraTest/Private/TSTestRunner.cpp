#include "TSTestRunner.h"
#include "Tessera.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

ATSTestRunner::ATSTestRunner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;   // keeps driving dialogue and menus while the game is paused
}

FString ATSTestRunner::ShotPath(const FString& Name)
{
	const FString Folder = TSConfig::Get(TEXT("ShotFolder"), *(TEXT("Screenshots/") + TSCmd::Prefix()));
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / Folder / Name + TEXT(".png"));
}

void ATSTestRunner::Tick(float Dt)
{
	Super::Tick(Dt);
	T += Dt;
	if (bDone || T < Next) return;
	if (bQuitting)
	{
		Report(TEXT("done"));
		FPlatformMisc::RequestExit(false);
		bDone = true;
		return;
	}
	RunStep();
}

void ATSTestRunner::Report(const FString& Line) const
{
	UE_LOG(LogTessera, Display, TEXT("[TEST %s t=%.1f] %s"), *Scenario, T, *Line);
}

void ATSTestRunner::Quit(float After)
{
	Next = T + After;
	bQuitting = true;
}

void ATSTestRunner::Shot(const FString& Name) const
{
	FScreenshotRequest::RequestScreenshot(ShotPath(Name), /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
}

FVector2D ATSTestRunner::WindowCentre() const
{
	const FSlateApplication& App = FSlateApplication::Get();
	const TSharedPtr<SWindow> W = App.GetActiveTopLevelWindow();
	return W.IsValid() ? W->GetPositionInScreen() + W->GetSizeInScreen() * 0.5f : FVector2D(800, 450);
}

void ATSTestRunner::ClickAt(const FVector2D& At) const
{
	FSlateApplication& App = FSlateApplication::Get();
	// Several games side by side (parallel tests) overlap: put this one on top, or the click lands on another.
	if (GEngine && GEngine->GameViewport)
		if (const TSharedPtr<SWindow> Win = GEngine->GameViewport->GetWindow()) Win->HACK_ForceToFront();
	App.ProcessMouseMoveEvent(FPointerEvent(0, 0, At, At - FVector2D(4, 0), TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	TSet<FKey> Held = { EKeys::LeftMouseButton };
	App.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(0, 0, At, At, Held, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	App.ProcessMouseButtonUpEvent(FPointerEvent(0, 0, At, At, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
}

void TSTestSwitches::Run(UWorld* World, TSubclassOf<ATSTestRunner> RunnerClass)
{
	if (!World) return;
	FTimerManager& Timers = World->GetTimerManager();

	FString CamSpec;
	if (TSCmd::Value(TEXT("Cam"), CamSpec, /*bWhole*/ true))
	{
		TArray<FString> P;
		CamSpec.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 5)
		{
			const FVector Loc(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2]));
			const FRotator Rot(FCString::Atof(*P[3]), FCString::Atof(*P[4]), 0);
			ACameraActor* CamActor = World->SpawnActor<ACameraActor>(Loc, Rot);
			CamActor->GetCameraComponent()->SetFieldOfView(70.f);
			CamActor->GetCameraComponent()->bConstrainAspectRatio = false;
			// The controller re-targets its pawn on possession, so take over the view a moment later.
			TWeakObjectPtr<UWorld> W = World;
			FTimerHandle H;
			Timers.SetTimer(H, [W, CamActor]()
			{
				if (APlayerController* PC = W.IsValid() ? W->GetFirstPlayerController() : nullptr)
				{
					PC->bAutoManageActiveCameraTarget = false;
					PC->SetViewTarget(CamActor);
				}
			}, 0.25f, false);
		}
	}

	float ShotAt = 0.f;
	if (TSCmd::Value(TEXT("Shot"), ShotAt))
	{
		FString Name = TEXT("shot");
		TSCmd::Value(TEXT("ShotName"), Name);
		const FString File = ATSTestRunner::ShotPath(Name);
		FTimerHandle H;
		Timers.SetTimer(H, [File]()
		{
			FScreenshotRequest::RequestScreenshot(File, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
			UE_LOG(LogTessera, Display, TEXT("Screenshot requested: %s"), *File);
		}, ShotAt, false);
		FTimerHandle Q;
		Timers.SetTimer(Q, []() { FPlatformMisc::RequestExit(false); }, ShotAt + 2.f, false);
	}

	FString Scenario;
	if (RunnerClass && TSCmd::Value(TEXT("Test"), Scenario))
	{
		ATSTestRunner* Runner = World->SpawnActor<ATSTestRunner>(RunnerClass);
		Runner->Scenario = Scenario;
		UE_LOG(LogTessera, Display, TEXT("Running self-test scenario '%s'"), *Scenario);
	}

	float QuitAfter = 0.f;
	if (TSCmd::Value(TEXT("QuitAfter"), QuitAfter))
	{
		FTimerHandle Q;
		Timers.SetTimer(Q, []() { UE_LOG(LogTessera, Display, TEXT("Self-test run complete.")); FPlatformMisc::RequestExit(false); }, QuitAfter, false);
	}
}
