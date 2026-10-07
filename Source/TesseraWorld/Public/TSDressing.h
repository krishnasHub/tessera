#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSDressing.generated.h"

class UStaticMeshComponent;

/**
 * Set dressing a game can turn up or down: small pixel-art props laid over the world at named spots - puddles on
 * unkept roads, cracks and moss on walls, vines on a happy village's houses, shafts of sunlight. Purely cosmetic.
 *
 * Data: <world>.dressing.kinds { "<id>": {
 *     "sheet": "PR_Puddle", "size": 120 (uu tall; flat: uu long), "aspect": 2, "flat": false (lie on the ground),
 *     "spots": "road" (the game's spot list), "jitter": 60 (uu), "seeThrough": false, "opacity": 0.6,
 *     "when": "day" | "night" | "any", "lift": 0 (uu above the spot)
 * } }
 * The game names the spot lists (SetSpots) and says how many of each kind to show (SetCount); the same spots are
 * used each time (a stable order), so turning a kind up adds to what's there and turning it down takes the newest away.
 */
UCLASS()
class TESSERAWORLD_API ATSDressing : public AActor
{
	GENERATED_BODY()

public:
	ATSDressing();
	static ATSDressing* Spawn(UWorld* World);
	virtual void Tick(float DeltaSeconds) override;

	void SetSpots(FName List, const TArray<FVector>& Points);
	/** Show this many of a kind (clamped to its spots). */
	void SetCount(FName Kind, int32 Count);
	/** How many of a kind are shown (tests). */
	int32 Shown(FName Kind) const;

private:
	struct FKind
	{
		FName Id, Spots, When = TEXT("any");
		FString Sheet;
		float Size = 100.f, Aspect = 1.f, Jitter = 0.f, Opacity = 1.f, Lift = 0.f;
		bool bFlat = false, bSeeThrough = false;
		int32 Count = 0;
		TArray<TObjectPtr<UStaticMeshComponent>> Cards;
		TArray<FVector> Order;   // its spots, shuffled once
	};
	void LoadKinds();
	void Rebuild(FKind& K);
	bool Fits(const FKind& K) const;

	TArray<FKind> Kinds;
	TMap<FName, TArray<FVector>> SpotLists;
	float LastNight = -1.f;
};
