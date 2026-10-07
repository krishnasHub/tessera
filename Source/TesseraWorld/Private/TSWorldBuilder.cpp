#include "TSWorldBuilder.h"
#include "Tessera.h"
#include "TSAssets.h"
#include "TSSky.h"

#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Camera/PlayerCameraManager.h"
#include "NavigationSystem.h"
#include "NavAreas/NavArea_Null.h"
#include "NavMesh/NavMeshBoundsVolume.h"

ATSWorldBuilder::ATSWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	RootComponent = Terrain;
}

void ATSWorldBuilder::BeginBuild()
{
	ATSSky::BeginWorld(this);
	Cutaways.Reset();
	InstanceCutaways.Reset();
	Flickers.Reset();
}

void ATSWorldBuilder::FinishBuild(const FBox& PlayableArea)
{
	Sky = ATSSky::Spawn(GetWorld());
	BuildNavigation(PlayableArea);
}

void ATSWorldBuilder::AddFlicker(UPointLightComponent* Light, float Base)
{
	if (Light) Flickers.Add({ Light, Base, FMath::FRand() * 10.f });
}

void ATSWorldBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld()->GetTimeSeconds();
	for (FFlicker& F : Flickers)
	{
		const float N = FMath::Sin(T * 13.f + F.Phase) * 0.08f + FMath::Sin(T * 7.3f + F.Phase * 2.f) * 0.1f + FMath::PerlinNoise1D(T * 4.f + F.Phase) * 0.15f;
		F.Light->SetIntensity(F.Base * (1.f + N));
	}
	UpdateCutaways(DeltaSeconds);
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

UHierarchicalInstancedStaticMeshComponent* ATSWorldBuilder::MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollide, int32 CullDistance)
{
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	H->SetStaticMesh(Mesh);
	if (Material) H->SetMaterial(0, Material);
	H->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	if (CullDistance > 0) H->SetCullDistances(CullDistance * 3 / 4, CullDistance);
	H->SetupAttachment(RootComponent);
	H->RegisterComponent();
	return H;
}

UStaticMeshComponent* ATSWorldBuilder::AddMesh(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& T, bool bCollide)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	if (Material) C->SetMaterial(0, Material);
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	C->SetupAttachment(RootComponent);
	C->SetWorldTransform(T);
	C->RegisterComponent();
	if (Collect) Collect->Add(C);
	return C;
}

UStaticMeshComponent* ATSWorldBuilder::AddBox(const FVector& Center, const FVector& Size, UMaterialInterface* Material, bool bCollide, const FRotator& Rot)
{
	// The engine cube is 100uu and centred on its pivot. (Looked up each time, not cached in a static: a New Game
	// tears the world down and the engine may unload the mesh in between.)
	return AddMesh(TSAssets::Shape(TEXT("Cube")), Material, FTransform(Rot, Center, Size / 100.f), bCollide);
}

void ATSWorldBuilder::AddBlocker(const FVector& Center, const FVector& HalfExtent)
{
	UBoxComponent* B = NewObject<UBoxComponent>(this);
	B->SetBoxExtent(HalfExtent);
	B->SetCollisionProfileName(TEXT("InvisibleWall"));   // blocks pawns, not sight
	// ...and carves a hole in the navmesh, so click-to-move paths go around (over the bridge).
	B->SetCanEverAffectNavigation(true);
	B->bDynamicObstacle = true;
	B->SetAreaClassOverride(UNavArea_Null::StaticClass());
	B->SetupAttachment(RootComponent);
	B->SetWorldLocation(Center);
	B->RegisterComponent();
}

float ATSWorldBuilder::GroundZ(float X, float Y) const
{
	if (Heights.IsEmpty()) return 0.f;
	const float FI = (X - OriginX) / Step, FJ = (Y - OriginY) / Step;
	const int32 I = FMath::Clamp(FMath::FloorToInt(FI), 0, GridW - 2), J = FMath::Clamp(FMath::FloorToInt(FJ), 0, GridH - 2);
	const float U = FMath::Clamp(FI - I, 0.f, 1.f), V = FMath::Clamp(FJ - J, 0.f, 1.f);
	const float H00 = Heights[J * GridW + I], H10 = Heights[J * GridW + I + 1];
	const float H01 = Heights[(J + 1) * GridW + I], H11 = Heights[(J + 1) * GridW + I + 1];
	return FMath::Lerp(FMath::Lerp(H00, H10, U), FMath::Lerp(H01, H11, U), V);
}

// ---------------------------------------------------------------------------------------------
// Cutaways
// ---------------------------------------------------------------------------------------------

void ATSWorldBuilder::UpdateCutaways(float Dt)
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P || !PC->PlayerCameraManager) return;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation(), At = P->GetActorLocation();
	for (FTSCutaway& H : Cutaways)
	{
		// Sight lines from the camera to the hero's feet, middle and head.
		bool bBlocks = false;
		for (const float Z : { -80.f, 0.f, 90.f })
		{
			const FVector End = At + FVector(0, 0, Z);
			if (FMath::LineBoxIntersection(H.Bounds, Cam, End, End - Cam)) { bBlocks = true; break; }
		}
		H.Hold = bBlocks ? CutawayHold : H.Hold - Dt;   // linger a moment so it doesn't flicker at the edge
		const bool bCut = H.Hold > 0.f;
		if (bCut == H.bCut) continue;
		H.bCut = bCut;
		for (UStaticMeshComponent* C : H.Full) C->SetVisibility(!bCut);
		for (UStaticMeshComponent* C : H.Cut) C->SetVisibility(bCut);
	}

	// Instanced pieces between the camera and the hero (tree crowns) shrink away.
	TSet<UHierarchicalInstancedStaticMeshComponent*> Dirty;
	for (FTSInstanceCutaway& Tr : InstanceCutaways)
	{
		UHierarchicalInstancedStaticMeshComponent* Set = Tr.Set.Get();
		if (!Set || (!Tr.bCut && FVector::DistSquared2D(Tr.Bounds.GetCenter(), At) > FMath::Square(InstanceCutawayRange))) continue;
		bool bBlocks = false;
		for (const float Z : { -80.f, 0.f, 90.f })
		{
			const FVector End = At + FVector(0, 0, Z);
			if (FMath::LineBoxIntersection(Tr.Bounds, Cam, End, End - Cam)) { bBlocks = true; break; }
		}
		Tr.Hold = bBlocks ? CutawayHold : Tr.Hold - Dt;
		const bool bCut = Tr.Hold > 0.f;
		if (bCut == Tr.bCut) continue;
		Tr.bCut = bCut;
		for (int32 I = 0; I < Tr.Instances.Num(); ++I)
		{
			FTransform T = Tr.Transforms[I];
			if (bCut) T.SetScale3D(FVector(0.001f));
			Set->UpdateInstanceTransform(Tr.Instances[I], T, true, false, true);
		}
		Dirty.Add(Set);
	}
	for (UHierarchicalInstancedStaticMeshComponent* Set : Dirty) Set->MarkRenderStateDirty();
}

// ---------------------------------------------------------------------------------------------
// Navigation: a navmesh over the playable area, generated at runtime. There is no level file to hold a
// NavMeshBoundsVolume, so one is spawned here; a volume's bounds are its components' bounding box, so an
// area-sized box component on it sets the extent.
// ---------------------------------------------------------------------------------------------

void ATSWorldBuilder::BuildNavigation(const FBox& Area)
{
	UWorld* W = GetWorld();
	if (!FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
	{
		if (AWorldSettings* WS = W->GetWorldSettings(); WS && !WS->GetNavigationSystemConfig())
			WS->SetNavigationSystemConfigOverride(NewObject<UNavigationSystemModuleConfig>(WS));
		FNavigationSystem::AddNavigationSystemToWorld(*W, FNavigationSystemRunMode::GameMode);
	}
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	if (!Nav) { UE_LOG(LogTessera, Warning, TEXT("No navigation system: paths are straight lines.")); return; }

	ANavMeshBoundsVolume* Vol = W->SpawnActor<ANavMeshBoundsVolume>(Area.GetCenter(), FRotator::ZeroRotator);
	if (!Vol) return;
	UBoxComponent* Box = NewObject<UBoxComponent>(Vol);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCanEverAffectNavigation(false);
	Box->SetBoxExtent(Area.GetExtent());
	Box->SetupAttachment(Vol->GetRootComponent());
	Box->RegisterComponent();
	Nav->OnNavigationBoundsUpdated(Vol);
	// The terrain joined the navigation octree when it was created, before it had any geometry (and was
	// skipped for empty bounds); now that it's built, register it again so the navmesh has ground to stand on.
	if (Terrain) Nav->UpdateComponentInNavOctree(*Terrain);
	UE_LOG(LogTessera, Display, TEXT("Navmesh bounds: %s"), *Vol->GetComponentsBoundingBox(true).ToString());
}
