#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSAmbientLife.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Life that isn't part of the game: harmless critters and passers-by that wander their areas, by day or by night,
 * and run off or vanish when the hero comes near. None of it can be fought or touched; it's there to make the world
 * feel lived in, and to show its state (a game re-weights the kinds: more children when things go well, wolves at
 * night when they don't).
 *
 * Data: <world>.ambientLife.kinds { "<id>": {
 *     "sheet": "SPR_Wolf", "frames": 4, "size": 140 (uu tall card), "aspect": 1.33 (frame width / height), "fps": 8,
 *     "move": "walk" | "fly" | "slither", "speed": 120, "height": 0 (fliers: uu above the ground),
 *     "flee": 500 (uu: the hero this close and it flees), "fleeSpeed": 420, "vanish": false (fade away instead of running),
 *     "when": "day" | "night" | "any", "area": "grass", "count": 3 (at weight 1)
 * } }
 * The game names the areas (SetArea: points the critters live around) and scales each kind (SetWeight). Sheets face
 * right and are one row of frames (frame 0 = standing still); <world>.looks2d.textureFolder holds them.
 */
UCLASS()
class TESSERAWORLD_API ATSAmbientLife : public AActor
{
	GENERATED_BODY()

public:
	ATSAmbientLife();
	static ATSAmbientLife* Spawn(UWorld* World);
	virtual void Tick(float DeltaSeconds) override;

	/** Points a kind's "area" names (where its critters spawn and wander). */
	void SetArea(FName Area, const TArray<FVector>& Points);
	/** How many of a kind (x its "count"); 0 = none. Changes take effect as critters come and go. */
	void SetWeight(FName Kind, float Weight);
	float GetWeight(FName Kind) const;
	/** Live critters of a kind (tests). */
	int32 Count(FName Kind) const;
	/** Where the live critters of a kind are (tests). */
	TArray<FVector> Positions(FName Kind) const;
	/** Has any critter of this kind fled from the hero (tests)? */
	bool HasFled(FName Kind) const { return Fled.Contains(Kind); }
	/** Spawn up to the wanted numbers right away, anywhere in their areas (tests, screenshots). */
	void FillNow();
	/** Can a walking (or slithering) critter stand here? The game says (no water, walls, houses...); fliers ignore it. */
	TFunction<bool(const FVector& At)> CanStand;
	/** Distance from the hero at which critters are allowed to appear (so they don't pop in in plain sight). */
	float SpawnMinDistance = 1100.f;

private:
	struct FKind
	{
		FName Id;
		FString Sheet;
		int32 Frames = 4;
		float Size = 100.f, Aspect = 1.f, Fps = 8.f, Speed = 120.f, Height = 0.f, Flee = 500.f, FleeSpeed = 420.f;
		FName Move = TEXT("walk"), When = TEXT("any"), Area = TEXT("grass");
		bool bVanish = false;
		int32 BaseCount = 0;
		float Weight = 1.f;
	};
	struct FCritter
	{
		int32 Kind = 0;
		TObjectPtr<UStaticMeshComponent> Mesh;
		TObjectPtr<UMaterialInstanceDynamic> Mat;
		FVector Home = FVector::ZeroVector, Pos = FVector::ZeroVector, Target = FVector::ZeroVector, Vel = FVector::ZeroVector;
		float Timer = 0.f, Anim = 0.f, Phase = 0.f, Fade = 1.f;
		enum class EState : uint8 { Wander, Pause, Flee, Vanish } State = EState::Wander;
		bool bLeaving = false;   // its kind isn't wanted now: go once out of the hero's sight
		bool bSeeThrough = false;   // vanishing on the see-through material: fades out at full size
	};

	void LoadKinds();
	bool Wanted(const FKind& K) const;
	int32 WantedCount(const FKind& K) const;
	bool SpawnOne(int32 KindIndex, const FVector& Hero, bool bAnywhere);
	void Remove(int32 Index);
	void StartVanish(FCritter& C);
	float GroundZ(const FVector& P) const;
	void Place(FCritter& C, float Dt);
	bool Standable(const FKind& K, const FVector& At) const { return K.Move == TEXT("fly") || !CanStand || CanStand(At); }

	TArray<FKind> Kinds;
	TArray<FCritter> Critters;
	TMap<FName, TArray<FVector>> Areas;
	TSet<FName> Fled;
	FRandomStream Rand;
	float SpawnTimer = 0.f;
};
