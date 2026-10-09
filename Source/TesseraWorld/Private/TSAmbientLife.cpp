#include "TSAmbientLife.h"
#include "TSSky.h"
#include "TSData.h"
#include "TSLook.h"
#include "TSAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

ATSAmbientLife::ATSAmbientLife()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ATSAmbientLife* ATSAmbientLife::Spawn(UWorld* World)
{
	if (!World) return nullptr;
	ATSAmbientLife* A = World->SpawnActor<ATSAmbientLife>();
	if (A) { A->Rand.Initialize(7331); A->LoadKinds(); }
	return A;
}

void ATSAmbientLife::LoadKinds()
{
	const TSJson::FObj All = TSJson::Obj(TSJson::Obj(UTSData::Get(this).World(), TEXT("ambientLife")), TEXT("kinds"));
	if (!All) return;
	for (const auto& KV : All->Values)
	{
		const TSJson::FObj O = TSJson::Obj(All, FString(*KV.Key));
		if (!O) continue;   // ("_doc" strings)
		FKind K;
		K.Id = FName(FString(*KV.Key));
		K.Sheet = TSJson::Str(O, TEXT("sheet"));
		K.Frames = FMath::Max(1, int32(TSJson::Num(O, TEXT("frames"), 4)));
		K.Size = float(TSJson::Num(O, TEXT("size"), 100));
		K.Aspect = float(TSJson::Num(O, TEXT("aspect"), 1));
		K.Fps = float(TSJson::Num(O, TEXT("fps"), 8));
		K.Speed = float(TSJson::Num(O, TEXT("speed"), 120));
		K.Height = float(TSJson::Num(O, TEXT("height"), 0));
		K.Flee = float(TSJson::Num(O, TEXT("flee"), 500));
		K.FleeSpeed = float(TSJson::Num(O, TEXT("fleeSpeed"), 420));
		K.Move = FName(TSJson::Str(O, TEXT("move"), TEXT("walk")));
		K.When = FName(TSJson::Str(O, TEXT("when"), TEXT("any")));
		K.Area = FName(TSJson::Str(O, TEXT("area"), TEXT("grass")));
		K.bVanish = TSJson::Bool(O, TEXT("vanish"), false);
		K.BaseCount = int32(TSJson::Num(O, TEXT("count"), 0));
		Kinds.Add(K);
	}
}

void ATSAmbientLife::SetArea(FName Area, const TArray<FVector>& Points) { Areas.Add(Area, Points); }

void ATSAmbientLife::SetWeight(FName Kind, float Weight)
{
	for (int32 I = 0; I < Kinds.Num(); ++I)
	{
		FKind& K = Kinds[I];
		if (K.Id != Kind) continue;
		K.Weight = FMath::Max(0.f, Weight);
		// Fewer wanted now: the extras are on their way out (they slip away once out of the hero's sight).
		int32 Have = Count(Kind);
		for (FCritter& C : Critters) if (C.Kind == I && !C.bLeaving && Have > WantedCount(K)) { C.bLeaving = true; --Have; }
	}
}

float ATSAmbientLife::GetWeight(FName Kind) const
{
	for (const FKind& K : Kinds) if (K.Id == Kind) return K.Weight;
	return 0.f;
}

int32 ATSAmbientLife::Count(FName Kind) const
{
	int32 N = 0;
	for (const FCritter& C : Critters) if (Kinds[C.Kind].Id == Kind && !C.bLeaving) ++N;
	return N;
}

TArray<FVector> ATSAmbientLife::Positions(FName Kind) const
{
	TArray<FVector> Out;
	for (const FCritter& C : Critters) if (Kinds[C.Kind].Id == Kind && !C.bLeaving) Out.Add(C.Pos);
	return Out;
}

bool ATSAmbientLife::Wanted(const FKind& K) const
{
	const float Night = ATSSky::Night();
	return K.When == TEXT("day") ? Night < 0.5f : K.When == TEXT("night") ? Night >= 0.5f : true;
}

int32 ATSAmbientLife::WantedCount(const FKind& K) const
{
	return Wanted(K) ? FMath::RoundToInt(K.BaseCount * K.Weight) : 0;
}

float ATSAmbientLife::GroundZ(const FVector& P) const
{
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(TSAmbientGround), false);
	const FVector Top(P.X, P.Y, P.Z + 1500.f);
	return GetWorld()->LineTraceSingleByChannel(H, Top, Top - FVector(0, 0, 4000.f), ECC_Visibility, Q) ? H.ImpactPoint.Z : P.Z;
}

bool ATSAmbientLife::SpawnOne(int32 KindIndex, const FVector& Hero, bool bAnywhere)
{
	const FKind& K = Kinds[KindIndex];
	const TArray<FVector>* Pts = Areas.Find(K.Area);
	if (!Pts || Pts->IsEmpty()) return false;
	FVector At = FVector::ZeroVector;
	bool bFound = false;
	for (int32 Try = 0; Try < 24 && !bFound; ++Try)
	{
		At = (*Pts)[Rand.RandRange(0, Pts->Num() - 1)] + FVector(Rand.FRandRange(-120.f, 120.f), Rand.FRandRange(-120.f, 120.f), 0.f);
		bFound = (bAnywhere || FVector::Dist2D(At, Hero) >= SpawnMinDistance) && Standable(K, At);   // out of the hero's sight, on firm ground
	}
	if (!bFound) return false;
	UMaterialInstanceDynamic* Mat = TSLook::SpriteMaterial(this, K.Sheet, K.Frames, 1);
	if (!Mat) return false;
	FCritter C;
	C.Kind = KindIndex;
	C.Mesh = NewObject<UStaticMeshComponent>(this);
	C.Mesh->SetStaticMesh(TSAssets::Shape(TEXT("Plane")));
	C.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C.Mesh->SetCastShadow(K.Height <= 0.f);
	C.Mesh->SetUsingAbsoluteLocation(true);
	C.Mesh->SetUsingAbsoluteRotation(true);
	C.Mesh->SetUsingAbsoluteScale(true);
	C.Mesh->SetMaterial(0, Mat);
	C.Mesh->SetupAttachment(RootComponent);
	C.Mesh->RegisterComponent();
	C.Mat = Mat;
	C.Home = C.Pos = C.Target = FVector(At.X, At.Y, GroundZ(At));
	C.Phase = Rand.FRandRange(0.f, UE_TWO_PI);
	C.Timer = Rand.FRandRange(0.5f, 2.f);
	C.State = FCritter::EState::Pause;
	Critters.Add(C);
	return true;
}

void ATSAmbientLife::Remove(int32 Index)
{
	if (Critters[Index].Mesh) Critters[Index].Mesh->DestroyComponent();
	Critters.RemoveAtSwap(Index);
}

void ATSAmbientLife::StartVanish(FCritter& C)
{
	C.State = FCritter::EState::Vanish;
	C.Timer = 0.4f;
	if (C.bSeeThrough) return;
	// Fade out where it stands (the see-through sprite material); without that material it shrinks away instead.
	UMaterialInterface* Base = TSAssets::Material(this, TEXT("spriteSeeThrough"));
	UTexture* Tex = nullptr;
	if (!Base || !C.Mat || !C.Mat->GetTextureParameterValue(TEXT("Tex"), Tex) || !Tex) return;
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetTextureParameterValue(TEXT("Tex"), Tex);
	for (const TCHAR* P : { TEXT("Cols"), TEXT("Rows"), TEXT("Col"), TEXT("Row"), TEXT("Flip") })
	{
		float V = 0.f;
		if (C.Mat->GetScalarParameterValue(FName(P), V)) M->SetScalarParameterValue(FName(P), V);
	}
	M->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	C.Mesh->SetMaterial(0, M);
	C.Mat = M;
	C.bSeeThrough = true;
}

void ATSAmbientLife::FillNow()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Hero = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
	for (int32 I = 0; I < Kinds.Num(); ++I)
		for (int32 Guard = 0; Count(Kinds[I].Id) < WantedCount(Kinds[I]) && Guard < 64; ++Guard)
			if (!SpawnOne(I, Hero, true)) break;
}

void ATSAmbientLife::Place(FCritter& C, float Dt)
{
	const FKind& K = Kinds[C.Kind];
	const FRotator R = TSLook::CardRotation();
	const FVector Up = -FRotationMatrix(R).GetUnitAxis(EAxis::Y);
	float Lift = 0.f;
	if (K.Move == TEXT("fly")) Lift = K.Height + FMath::Sin(C.Anim * 3.1f + C.Phase) * K.Height * 0.25f;   // bobbing flight
	const float S = K.Size * (C.bSeeThrough ? 1.f : C.Fade);
	C.Mesh->SetWorldLocationAndRotation(FVector(C.Pos.X, C.Pos.Y, C.Pos.Z + Lift) + Up * (S * 0.5f - S * 0.06f), R);
	C.Mesh->SetWorldScale3D(FVector(S * K.Aspect / 100.f, S / 100.f, 1.f));
	const bool bMoving = C.State != FCritter::EState::Pause || K.Move == TEXT("fly");
	C.Mat->SetScalarParameterValue(TEXT("Col"), bMoving ? float(int32(C.Anim * K.Fps) % K.Frames) : 0.f);
	C.Mat->SetScalarParameterValue(TEXT("Row"), 0.f);
	if (FMath::Abs(C.Vel.X) > 3.f) C.Mat->SetScalarParameterValue(TEXT("Flip"), C.Vel.X < 0.f ? 1.f : 0.f);   // sheets face right
	C.Mesh->SetVisibility(C.Fade > 0.02f);
}

void ATSAmbientLife::Tick(float Dt)
{
	Super::Tick(Dt);
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P) return;
	const FVector Hero = P->GetActorLocation();

	// Come and go: top up kinds below their wanted number (out of sight), retire the ones no longer wanted.
	SpawnTimer -= Dt;
	if (SpawnTimer <= 0.f)
	{
		SpawnTimer = 0.6f;
		for (int32 I = 0; I < Kinds.Num(); ++I)
		{
			const int32 Want = WantedCount(Kinds[I]);
			int32 Have = Count(Kinds[I].Id);
			if (Have < Want) SpawnOne(I, Hero, false);
			for (FCritter& C : Critters) if (C.Kind == I && !C.bLeaving && Have > Want) { C.bLeaving = true; --Have; }
		}
	}

	for (int32 I = Critters.Num() - 1; I >= 0; --I)
	{
		FCritter& C = Critters[I];
		const FKind& K = Kinds[C.Kind];
		C.Anim += Dt;
		C.Timer -= Dt;
		const float ToHero = FVector::Dist2D(C.Pos, Hero);

		// Unwanted (its hour passed, or the world changed): slip away once the hero can't see.
		if (C.bLeaving && ToHero > SpawnMinDistance) { Remove(I); continue; }

		if ((C.State == FCritter::EState::Wander || C.State == FCritter::EState::Pause) && ToHero < K.Flee)
		{
			Fled.Add(K.Id);
			C.Vel = (C.Pos - Hero).GetSafeNormal2D() * K.FleeSpeed;
			if (K.bVanish) StartVanish(C);
			else { C.State = FCritter::EState::Flee; C.Timer = 1.6f; }
		}
		switch (C.State)
		{
		case FCritter::EState::Pause:
			C.Vel = FVector::ZeroVector;
			if (C.Timer <= 0.f)
			{
				C.Target = C.Home + FVector(Rand.FRandRange(-350.f, 350.f), Rand.FRandRange(-350.f, 350.f), 0.f);
				for (int32 Try = 0; Try < 8 && !Standable(K, C.Target); ++Try) C.Target = C.Home + FVector(Rand.FRandRange(-350.f, 350.f), Rand.FRandRange(-350.f, 350.f), 0.f);
				if (!Standable(K, C.Target)) C.Target = C.Pos;
				C.State = FCritter::EState::Wander;
				C.Timer = 6.f;
			}
			break;
		case FCritter::EState::Wander:
		{
			FVector To = C.Target - C.Pos; To.Z = 0.f;
			if (To.Size() < 25.f || C.Timer <= 0.f) { C.State = FCritter::EState::Pause; C.Timer = Rand.FRandRange(0.8f, 3.f); break; }
			C.Vel = To.GetSafeNormal() * K.Speed;
			if (K.Move == TEXT("fly")) C.Vel += FVector(FMath::Sin(C.Anim * 5.f + C.Phase), FMath::Cos(C.Anim * 4.f + C.Phase), 0.f) * K.Speed * 0.6f;   // flutter
			if (K.Move == TEXT("slither")) C.Vel += FVector(-To.Y, To.X, 0.f).GetSafeNormal() * FMath::Sin(C.Anim * 6.f) * K.Speed * 0.5f;
			break;
		}
		case FCritter::EState::Flee:
			if (C.Timer <= 0.f) StartVanish(C);   // out of breath: gone, to come back elsewhere
			break;
		case FCritter::EState::Vanish:
			C.Fade = FMath::Max(0.f, C.Fade - Dt / 0.35f);
			if (C.Fade <= 0.f) { Remove(I); continue; }
			if (C.bSeeThrough) C.Mat->SetScalarParameterValue(TEXT("Opacity"), C.Fade);
			break;
		}
		// Never a step into water or a wall: wanderers stop and think again; runaways just slip away instead.
		const FVector NextPos = C.Pos + C.Vel * Dt;
		if (!Standable(K, NextPos + C.Vel.GetSafeNormal2D() * 40.f))
		{
			if (C.State == FCritter::EState::Flee) StartVanish(C);
			else if (C.State == FCritter::EState::Wander) { C.State = FCritter::EState::Pause; C.Timer = Rand.FRandRange(0.5f, 1.5f); }
			C.Vel = FVector::ZeroVector;
		}
		C.Pos += C.Vel * Dt;
		if (K.Move != TEXT("fly")) C.Pos.Z = GroundZ(C.Pos);
		Place(C, Dt);
	}
}
