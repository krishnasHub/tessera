#include "TSDressing.h"
#include "TSSky.h"
#include "TSData.h"
#include "TSLook.h"
#include "TSAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

ATSDressing::ATSDressing()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;   // only watches the hour
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ATSDressing* ATSDressing::Spawn(UWorld* World)
{
	if (!World) return nullptr;
	ATSDressing* A = World->SpawnActor<ATSDressing>();
	if (A) A->LoadKinds();
	return A;
}

void ATSDressing::LoadKinds()
{
	const TSJson::FObj All = TSJson::Obj(TSJson::Obj(UTSData::Get(this).World(), TEXT("dressing")), TEXT("kinds"));
	if (!All) return;
	for (const auto& KV : All->Values)
	{
		const TSJson::FObj O = TSJson::Obj(All, FString(*KV.Key));
		if (!O) continue;
		FKind K;
		K.Id = FName(FString(*KV.Key));
		K.Sheet = TSJson::Str(O, TEXT("sheet"));
		K.Spots = FName(TSJson::Str(O, TEXT("spots")));
		K.When = FName(TSJson::Str(O, TEXT("when"), TEXT("any")));
		K.Size = float(TSJson::Num(O, TEXT("size"), 100));
		K.Aspect = float(TSJson::Num(O, TEXT("aspect"), 1));
		K.Jitter = float(TSJson::Num(O, TEXT("jitter"), 0));
		K.Opacity = float(TSJson::Num(O, TEXT("opacity"), 1));
		K.Lift = float(TSJson::Num(O, TEXT("lift"), 0));
		K.bFlat = TSJson::Bool(O, TEXT("flat"), false);
		K.bSeeThrough = TSJson::Bool(O, TEXT("seeThrough"), false);
		Kinds.Add(K);
	}
}

void ATSDressing::SetSpots(FName List, const TArray<FVector>& Points)
{
	SpotLists.Add(List, Points);
	for (FKind& K : Kinds) if (K.Spots == List) { K.Order.Reset(); Rebuild(K); }
}

void ATSDressing::SetCount(FName Kind, int32 Count)
{
	for (FKind& K : Kinds) if (K.Id == Kind && K.Count != Count) { K.Count = FMath::Max(0, Count); Rebuild(K); }
}

int32 ATSDressing::Shown(FName Kind) const
{
	for (const FKind& K : Kinds) if (K.Id == Kind) return Fits(K) ? K.Cards.Num() : 0;
	return 0;
}

bool ATSDressing::Fits(const FKind& K) const
{
	const float Night = ATSSky::Night();
	return K.When == TEXT("day") ? Night < 0.5f : K.When == TEXT("night") ? Night >= 0.5f : true;
}

void ATSDressing::Rebuild(FKind& K)
{
	// A stable shuffle of its spots (seeded by the kind), so a higher count only adds to what's shown.
	if (K.Order.IsEmpty())
		if (const TArray<FVector>* Pts = SpotLists.Find(K.Spots))
		{
			K.Order = *Pts;
			FRandomStream R(GetTypeHash(K.Id));
			for (int32 I = K.Order.Num() - 1; I > 0; --I) K.Order.Swap(I, R.RandRange(0, I));
			for (FVector& P : K.Order) P += FVector(R.FRandRange(-K.Jitter, K.Jitter), R.FRandRange(-K.Jitter, K.Jitter), 0.f);
		}
	const int32 Want = FMath::Min(K.Count, K.Order.Num());
	while (K.Cards.Num() > Want) { if (UStaticMeshComponent* C = K.Cards.Pop()) C->DestroyComponent(); }
	if (K.Cards.Num() >= Want) return;

	UMaterialInterface* Mat = nullptr;
	if (K.bSeeThrough)
	{
		// Light through the air: the see-through sprite material, at the kind's opacity.
		UMaterialInterface* Base = TSAssets::Material(this, TEXT("spriteSeeThrough"));
		UTexture2D* Tex = TSAssets::Load<UTexture2D>(TSAssets::ObjPath(TSJson::Str(TSJson::Obj(UTSData::Get(this).World(), TEXT("looks2d")), TEXT("textureFolder")), K.Sheet));
		if (Base && Tex)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
			M->SetTextureParameterValue(TEXT("Tex"), Tex);
			M->SetScalarParameterValue(TEXT("Cols"), 1.f);
			M->SetScalarParameterValue(TEXT("Rows"), 1.f);
			M->SetScalarParameterValue(TEXT("Opacity"), K.Opacity);
			M->SetScalarParameterValue(TEXT("Emissive"), 0.6f);
			Mat = M;
		}
	}
	else Mat = TSLook::PropMaterial(K.Sheet);
	if (!Mat) return;

	const FRotator Card = TSLook::CardRotation();
	const FVector Up = -FRotationMatrix(Card).GetUnitAxis(EAxis::Y);
	while (K.Cards.Num() < Want)
	{
		const FVector At = K.Order[K.Cards.Num()];
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(TSAssets::Shape(TEXT("Plane")));
		C->SetMaterial(0, Mat);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		if (K.bFlat)
		{
			// On whatever the ground really is there (roads can sit above the height field).
			FVector G = At;
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(TSDressingGround), false);
			if (GetWorld()->LineTraceSingleByChannel(H, At + FVector(0, 0, 600.f), At - FVector(0, 0, 600.f), ECC_Visibility, Q)) G.Z = H.ImpactPoint.Z;
			C->SetWorldLocationAndRotation(G + FVector(0, 0, 3.f + K.Lift), FRotator::ZeroRotator);
		}
		else C->SetWorldLocationAndRotation(At + FVector(0, 0, K.Lift) + Up * K.Size * 0.5f, Card);
		C->SetWorldScale3D(FVector(K.Size * K.Aspect / 100.f, K.Size / 100.f, 1.f));
		C->SetVisibility(Fits(K));
		K.Cards.Add(C);
	}
}

void ATSDressing::Tick(float Dt)
{
	Super::Tick(Dt);
	// Day-only / night-only kinds follow the hour.
	const float Night = ATSSky::Night();
	if ((Night >= 0.5f) == (LastNight >= 0.5f) && LastNight >= 0.f) return;
	LastNight = Night;
	for (const FKind& K : Kinds)
		for (UStaticMeshComponent* C : K.Cards) if (C) C->SetVisibility(Fits(K));
}
