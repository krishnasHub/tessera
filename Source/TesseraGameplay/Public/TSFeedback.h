#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TSFeedback.generated.h"

struct FTSFloater { FVector World; FString Text; FLinearColor Color; float Size = 1.f; float Age = 0.f; float Life = 1.f; };
struct FTSToast { FString Text; FLinearColor Color; float Age = 0.f; };

/**
 * Feedback the HUD draws: floating text at a world point (damage numbers, "BLOCK"), toasts (short messages
 * stacked on screen) and camera shake. Gameplay pushes into it; the game's HUD reads it.
 */
UCLASS()
class TESSERAGAMEPLAY_API UTSFeedback : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTSFeedback* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTSFeedback, STATGROUP_Tickables); }
	virtual bool IsTickableWhenPaused() const override { return true; }

	void Float(const FVector& At, const FString& Text, const FLinearColor& Color, float Size = 1.f);
	void Toast(const FString& Text, const FLinearColor& Color = FLinearColor::White);
	void Shake(float Amount) { ShakeAmount = FMath::Max(ShakeAmount, Amount); }

	TArray<FTSFloater> Floaters;
	TArray<FTSToast> Toasts;
	float ShakeAmount = 0.f;
	int32 MaxToasts = 5;
	float ToastLife = 3.4f;
};
