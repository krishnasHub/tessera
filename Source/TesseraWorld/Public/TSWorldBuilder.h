#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSWorldBuilder.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UPointLightComponent;
class ATSSky;

/**
 * Base for a game's world builder: the world is generated at runtime from data, so a game ships no .umap.
 * A game subclass overrides Build():
 *
 *   BeginBuild();                 // night lights reset, ready to register fires
 *   ... terrain (fill the height field), props, buildings, using the helpers below ...
 *   FinishBuild(PlayableArea);    // sky + day/night (ATSSky) and the runtime navmesh over the area
 *
 * Provides: mesh / box / instanced-set / invisible-blocker helpers, a height field for GroundZ, flickering lights,
 * and cutaways: groups of meshes that are swapped for a cut-down version while they hide the hero from the camera
 * (buildings), or instanced pieces that shrink away (tree crowns).
 *
 * A building's cutaway can also be peeked into (cut whenever the hero is within CutawayPeekRange, e.g. at night to
 * show who sleeps inside) and opened (its full shell stops colliding and its cut walls start: walk in by the door gap
 * the game left in them). The game finds a building with CutawayAt.
 */
UCLASS(Abstract)
class TESSERAWORLD_API ATSWorldBuilder : public AActor
{
	GENERATED_BODY()

public:
	ATSWorldBuilder();

	virtual void Build() {}
	virtual void Tick(float DeltaSeconds) override;

	/** Ground height at a world XY (the height field; 0 without one). */
	float GroundZ(float X, float Y) const;

	UPROPERTY() TObjectPtr<ATSSky> Sky;

	// ---- buildings (cutaways) ----
	/** The building whose cutaway bounds hold Point (flat), or INDEX_NONE. */
	int32 CutawayAt(const FVector& Point) const;
	/** Cut it away while the hero is within CutawayPeekRange of it (not only while it hides them). */
	void SetCutawayPeek(int32 Index, bool bPeek);
	/** Open it to walk in: the full shell stops colliding, the cut-down walls (with their door gap) collide instead. */
	void SetCutawayOpen(int32 Index, bool bOpen);
	bool IsCutawayOpen(int32 Index) const { return Cutaways.IsValidIndex(Index) && Cutaways[Index].bOpen; }
	bool IsCutawayCut(int32 Index) const { return Cutaways.IsValidIndex(Index) && Cutaways[Index].bCut; }
	float CutawayPeekRange = 260.f;

protected:
	void BeginBuild();
	void FinishBuild(const FBox& PlayableArea);
	/** More walkable space for the navmesh (separate areas off the main map); call before FinishBuild. */
	void AddNavArea(const FBox& Area) { ExtraNavAreas.Add(Area); }
	TArray<FBox> ExtraNavAreas;
	/** Boxes where the ground is flat at a given height (areas off the height field): X/Y extent, Z = height. */
	TArray<FBox> FlatGround;

	/** The ground mesh (root). Re-registered with navigation once built. */
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;

	// ---- helpers ----
	UHierarchicalInstancedStaticMeshComponent* MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollide, int32 CullDistance = 0);
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& T, bool bCollide);
	/** A box built from the 100uu engine cube: Center/Size in world units, rotation optional. */
	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollide = true, const FRotator& Rot = FRotator::ZeroRotator);
	/** An invisible wall that blocks pawns but not sight or shots, and carves the navmesh (paths go around). */
	void AddBlocker(const FVector& Center, const FVector& HalfExtent);
	/** Make a point light flicker like a flame around its Base intensity. */
	void AddFlicker(UPointLightComponent* Light, float Base);

	// ---- height field (filled by the game's terrain) ----
	TArray<float> Heights;
	int32 GridW = 0, GridH = 0;
	float Step = 100.f, OriginX = 0.f, OriginY = 0.f;

	// ---- cutaways ----
	/** Meshes added while Collect points at Full are hidden (and those in Cut shown) while Bounds blocks the view. */
	struct FTSCutaway { FBox Bounds; TArray<TObjectPtr<UStaticMeshComponent>> Full, Cut; float Hold = 0; bool bCut = false, bPeek = false, bOpen = false; };
	TArray<FTSCutaway> Cutaways;
	/** Instances of Set that shrink away while Bounds blocks the view. */
	struct FTSInstanceCutaway { FBox Bounds; TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Set; TArray<int32> Instances; TArray<FTransform> Transforms; float Hold = 0; bool bCut = false; };
	TArray<FTSInstanceCutaway> InstanceCutaways;
	/** While set, AddMesh / AddBox also append what they create here (to group a building's parts). */
	TArray<TObjectPtr<UStaticMeshComponent>>* Collect = nullptr;
	/** Seconds a cutaway lingers after the view clears (no flicker at the edge); instance cutaways farther than
	 *  InstanceCutawayRange from the hero aren't tested. */
	float CutawayHold = 0.35f, InstanceCutawayRange = 2500.f;

private:
	void UpdateCutaways(float Dt);
	void BuildNavigation(const FBox& Area);

	struct FFlicker { TObjectPtr<UPointLightComponent> Light; float Base = 0; float Phase = 0; };
	TArray<FFlicker> Flickers;
};
