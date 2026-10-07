#include "TSInteractable.h"
#include "TSLook.h"
#include "TSAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATSInteractable::ATSInteractable()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Card = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Card"));
	Card->SetupAttachment(RootComponent);
	Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Card->SetUsingAbsoluteRotation(true);
	Card->SetUsingAbsoluteScale(true);
}

ATSInteractable* ATSInteractable::Spawn(UWorld* World, const FString& Id, const TSJson::FObj& Def, const FVector& At)
{
	if (!World || !Def) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATSInteractable* A = World->SpawnActor<ATSInteractable>(At, FRotator::ZeroRotator, P);
	if (!A) return nullptr;
	A->Id = Id;
	A->Def = Def;
	A->DisplayName = TSJson::Str(Def, TEXT("name"), Id);
	if (TSJson::Has(Def, TEXT("color"))) A->NameColor = TSJson::Color(TSJson::Str(Def, TEXT("color")), A->NameColor);
	A->DialogueRoot = TSJson::Str(Def, TEXT("dialogue"));
	A->TalkKey = TSJson::Str(Def, TEXT("talkKey"), Id);
	A->Radius = float(TSJson::Num(Def, TEXT("radius"), 60));
	A->bUsable = TSJson::Bool(Def, TEXT("usable"), true);
	A->DoorTo = TSJson::Str(Def, TEXT("door"));
	const TArray<TSharedPtr<FJsonValue>> Exit = TSJson::Arr(Def, TEXT("exit"));
	if (Exit.Num() == 2) A->ExitTiles = FVector2D(Exit[0]->AsNumber(), Exit[1]->AsNumber());
	A->Size = float(TSJson::Num(Def, TEXT("size"), 100));
	A->Aspect = float(TSJson::Num(Def, TEXT("aspect"), 1));
	A->bFlat = TSJson::Bool(Def, TEXT("flat"), false);
	A->Card->SetStaticMesh(TSAssets::Shape(TEXT("Plane")));
	A->Card->SetMaterial(0, TSLook::PropMaterial(TSJson::Str(Def, TEXT("sheet"))));
	A->Card->SetCastShadow(!A->bFlat);
	A->Place();
	return A;
}

void ATSInteractable::Place()
{
	const FVector At = GetActorLocation();
	if (bFlat)
	{
		// Lying on the ground, long side east-west (a plane faces up by default).
		Card->SetWorldLocationAndRotation(At + FVector(0, 0, 2.f), FRotator::ZeroRotator);
		Card->SetWorldScale3D(FVector(Size * Aspect / 100.f, Size / 100.f, 1.f));
		return;
	}
	const FRotator R = TSLook::CardRotation();
	const FVector Up = -FRotationMatrix(R).GetUnitAxis(EAxis::Y);
	Card->SetWorldLocationAndRotation(At + Up * (Size * 0.5f - Size * 0.04f), R);
	Card->SetWorldScale3D(FVector(Size * Aspect / 100.f, Size / 100.f, 1.f));
}

void ATSInteractable::SetShown(bool bShow)
{
	bShown = bShow;
	SetActorHiddenInGame(!bShow);
}

FVector ATSInteractable::Top() const
{
	return GetActorLocation() + FVector(0, 0, bFlat ? 60.f : Size * 0.9f);
}

float ATSInteractable::CursorMiss(const FVector& O, const FVector& R) const
{
	// Generous, like characters: a few points up the card (or along a flat one).
	const FVector At = GetActorLocation();
	float Miss = FMath::PointDistToLine(At, R, O);
	if (!bFlat)
	{
		const FVector Up = -FRotationMatrix(TSLook::CardRotation()).GetUnitAxis(EAxis::Y);
		for (const float F : { 0.25f, 0.5f, 0.75f }) Miss = FMath::Min(Miss, FMath::PointDistToLine(At + Up * Size * F, R, O));
	}
	return Miss - Radius;
}

ATSInteractable* ATSInteractable::Nearest(const UWorld* World, const FVector& Point, float Range)
{
	ATSInteractable* Best = nullptr;
	float BestD = Range;
	for (TActorIterator<ATSInteractable> It(World); It; ++It)
	{
		if (!It->CanUse()) continue;
		const float D = FVector::Dist2D(It->GetActorLocation(), Point) - It->Radius;
		if (D < BestD) { BestD = D; Best = *It; }
	}
	return Best;
}

ATSInteractable* ATSInteractable::DoorTarget() const
{
	return DoorTo.IsEmpty() ? nullptr : Find(GetWorld(), DoorTo);
}

FVector ATSInteractable::ExitPoint(float TileSize) const
{
	return GetActorLocation() + FVector(ExitTiles.X * TileSize, ExitTiles.Y * TileSize, 0.f);
}

ATSInteractable* ATSInteractable::Find(const UWorld* World, const FString& Id)
{
	for (TActorIterator<ATSInteractable> It(World); It; ++It) if (It->Id == Id) return *It;
	return nullptr;
}
