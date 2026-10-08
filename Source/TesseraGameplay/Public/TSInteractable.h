#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSJson.h"
#include "TSInteractable.generated.h"

class UStaticMeshComponent;

/**
 * Something in the world the hero can walk up to and use, that isn't a character: a grave, a lost ring, a sign, a cave
 * mouth. It is a pixel-art card standing (or lying flat) on the ground with a name and a conversation (the game opens
 * it with its story system, as it would a character's). Clicking it (or E nearby) walks up and uses it: see
 * UTSHeroControl's Use hooks. Can't be hurt, pushed or frozen.
 *
 * Data (the game reads its own list and calls Spawn): {
 *     "sheet": "PR_GraveMound", "size": 130 (uu tall; flat: uu long), "aspect": 1.6 (width / height),
 *     "flat": false (lie on the ground), "name": "An unmarked grave", "color": "#c8c0a8",
 *     "dialogue": "grave_root", "talkKey": "grave" (defaults to the id), "radius": 60, "usable": true,
 *     "door": "cave_exit" (a two-way door: using it takes you to that interactable), "exit": [0, 1] (where you come
 *     out when arriving through this one, in tiles from it; default one tile south),
 *     "use": "lock" (the game's own action when it's used: a locked door, a sealed gate...)
 * }
 * No "sheet": nothing is drawn (a spot on something the world builder made, like a house door); it is still named
 * on hover and usable.
 * Visible / usable can be switched by the game (story flags...): SetShown.
 */
UCLASS()
class TESSERAGAMEPLAY_API ATSInteractable : public AActor
{
	GENERATED_BODY()

public:
	ATSInteractable();
	static ATSInteractable* Spawn(UWorld* World, const FString& Id, const TSJson::FObj& Def, const FVector& At);

	FString Id;
	FString DisplayName;
	FLinearColor NameColor = FLinearColor(0.85f, 0.82f, 0.72f);
	FString TalkKey;
	FString DialogueRoot;
	TSJson::FObj Def;
	float Radius = 60.f;
	/** Can the hero use it (a decoration-only prop says no)? */
	bool bUsable = true;
	/** A door: the interactable it leads to ("" = not a door). */
	FString DoorTo;
	FVector2D ExitTiles = FVector2D(0, 1);
	/** The game's action when it's used ("" = talk or a door). */
	FString UseAction;

	/** The other side of this door (null if not a door, or it's missing). */
	ATSInteractable* DoorTarget() const;
	/** Where someone coming through to this one appears (on the ground). */
	FVector ExitPoint(float TileSize) const;

	/** Show or hide it (hidden: not drawn, not usable). */
	void SetShown(bool bShow);
	bool IsShown() const { return bShown; }
	bool CanUse() const { return bShown && bUsable && (!DialogueRoot.IsEmpty() || !DoorTo.IsEmpty() || !UseAction.IsEmpty()); }
	/** Where a name or a float sits (the top of the card). */
	FVector Top() const;
	/** How far the cursor ray passes from it (minus its radius): <= 0 is on it. */
	float CursorMiss(const FVector& RayOrigin, const FVector& RayDir) const;

	/** The usable one nearest Point within Range (from its edge), or null. */
	static ATSInteractable* Nearest(const UWorld* World, const FVector& Point, float Range);
	static ATSInteractable* Find(const UWorld* World, const FString& Id);

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Card;
	float Size = 100.f, Aspect = 1.f;
	bool bFlat = false;
	bool bShown = true;
	void Place();
};
