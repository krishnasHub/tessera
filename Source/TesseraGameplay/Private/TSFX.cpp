#include "TSFX.h"
#include "TSAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* GlowMat = TEXT("glow");               // <world>.assets keys
	const TCHAR* SeeThroughMat = TEXT("telegraph");
}

ATSFX::ATSFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ATSFX* ATSFX::Make(UWorld* W, const FVector& At, EKind Kind, float Life)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATSFX* F = W->SpawnActor<ATSFX>(At, FRotator::ZeroRotator, P);
	F->Kind = Kind;
	F->Life = Life;
	return F;
}

UStaticMeshComponent* ATSFX::AddPart(const TCHAR* Shape, const TCHAR* MaterialKey, const FLinearColor& Color, float Intensity)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(TSAssets::Shape(Shape));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(TSAssets::Material(this, MaterialKey), this);
	M->SetVectorParameterValue(TEXT("Color"), Color);
	M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	C->SetMaterial(0, M);
	C->SetupAttachment(RootComponent);
	C->RegisterComponent();
	Parts.Add(C);
	Mats.Add(M);
	return C;
}

void ATSFX::Ring(UWorld* W, const FVector& At, float InRadius, const FLinearColor& Color, float InLife)
{
	ATSFX* F = Make(W, At, EKind::Ring, InLife);
	F->Radius = InRadius;
	UStaticMeshComponent* Disc = F->AddPart(TEXT("Cylinder"), SeeThroughMat, Color, 4.f);
	Disc->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.02f));
}

void ATSFX::Bolt(UWorld* W, const TArray<FVector>& Points, const FLinearColor& Color, float InLife)
{
	if (Points.Num() < 2) return;
	ATSFX* F = Make(W, Points[0], EKind::Bolt, InLife);
	for (int32 I = 1; I < Points.Num(); ++I)
	{
		// A few jagged sub-segments per hop.
		FVector A = Points[I - 1];
		for (int32 S = 1; S <= 4; ++S)
		{
			FVector B = FMath::Lerp(Points[I - 1], Points[I], S / 4.f);
			if (S < 4) B += FVector(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-20.f, 20.f));
			UStaticMeshComponent* Seg = F->AddPart(TEXT("Cylinder"), GlowMat, Color, 25.f);
			const FVector Mid = (A + B) * 0.5f, Dir = B - A;
			Seg->SetWorldLocationAndRotation(Mid, FRotationMatrix::MakeFromZ(Dir.GetSafeNormal()).Rotator());
			Seg->SetWorldScale3D(FVector(0.035f, 0.035f, Dir.Size() / 100.f));
			A = B;
		}
	}
	F->Light = NewObject<UPointLightComponent>(F);
	F->Light->SetupAttachment(F->RootComponent);
	F->Light->SetWorldLocation(Points.Last());
	F->Light->SetIntensityUnits(ELightUnits::Candelas);
	F->Light->SetLightColor(Color);
	F->Light->SetIntensity(F->LightBase = 200.f);
	F->Light->SetAttenuationRadius(700.f);
	F->Light->SetCastShadows(false);
	F->Light->RegisterComponent();
}

void ATSFX::Burst(UWorld* W, const FVector& At, float InRadius, const FLinearColor& Color, float InLife)
{
	ATSFX* F = Make(W, At, EKind::Burst, InLife);
	F->Radius = InRadius;
	UStaticMeshComponent* S = F->AddPart(TEXT("Sphere"), SeeThroughMat, Color, 6.f);
	S->SetRelativeScale3D(FVector(InRadius / 50.f));
	F->Light = NewObject<UPointLightComponent>(F);
	F->Light->SetupAttachment(F->RootComponent);
	F->Light->SetIntensityUnits(ELightUnits::Candelas);
	F->Light->SetLightColor(Color);
	F->Light->SetIntensity(F->LightBase = 120.f);
	F->Light->SetAttenuationRadius(InRadius * 6.f);
	F->Light->SetCastShadows(false);
	F->Light->RegisterComponent();
}

void ATSFX::Sphere(UWorld* W, const FVector& At, float InRadius, const FLinearColor& Color, float InLife)
{
	ATSFX* F = Make(W, At, EKind::Sphere, InLife);
	F->Radius = InRadius;
	UStaticMeshComponent* S = F->AddPart(TEXT("Sphere"), SeeThroughMat, Color, 2.5f);
	S->SetRelativeScale3D(FVector(InRadius / 50.f * 0.15f));
	F->Light = NewObject<UPointLightComponent>(F);
	F->Light->SetupAttachment(F->RootComponent);
	F->Light->SetRelativeLocation(FVector(0, 0, InRadius * 0.3f));
	F->Light->SetIntensityUnits(ELightUnits::Candelas);
	F->Light->SetLightColor(Color);
	F->Light->SetIntensity(F->LightBase = 150.f);
	F->Light->SetAttenuationRadius(InRadius * 2.5f);
	F->Light->SetCastShadows(false);
	F->Light->RegisterComponent();
}

void ATSFX::Stain(UWorld* W, const FVector& At, float InRadius, const FLinearColor& Color, float InLife)
{
	ATSFX* F = Make(W, At + FVector(0, 0, 3.f), EKind::Stain, InLife);
	F->Radius = InRadius;
	UStaticMeshComponent* Disc = F->AddPart(TEXT("Cylinder"), SeeThroughMat, Color, 1.6f);
	Disc->SetRelativeScale3D(FVector(InRadius / 50.f, InRadius / 50.f, 0.01f));
}

void ATSFX::Smoke(UWorld* W, const FVector& At, float InRadius, float Duration)
{
	ATSFX* F = Make(W, At, EKind::Smoke, Duration + 2.f);   // + SmokeFade
	if (UParticleSystem* PS = TSAssets::Get<UParticleSystem>(W, TEXT("smoke")))
	{
		for (int32 I = 0; I < 4; ++I)
		{
			const FVector Off(FMath::FRandRange(-InRadius, InRadius) * 0.4f, FMath::FRandRange(-InRadius, InRadius) * 0.4f, 0);
			if (UParticleSystemComponent* E = UGameplayStatics::SpawnEmitterAttached(PS, F->RootComponent, NAME_None, Off, FRotator::ZeroRotator,
				FVector(InRadius / 150.f), EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ false))
				F->Emitters.Add(E);
		}
	}
	Ring(W, At, InRadius, FLinearColor(0.6f, 0.62f, 0.66f), 0.8f);
}

void ATSFX::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	const float K = FMath::Clamp(Age / Life, 0.f, 1.f);
	if (Kind == EKind::Ring && Parts.Num())
	{
		const float R = Radius * (0.3f + 0.7f * K);
		Parts[0]->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, 0.02f));
		Mats[0]->SetScalarParameterValue(TEXT("Opacity"), 0.55f * (1.f - K));
	}
	else if (Kind == EKind::Burst && Parts.Num())
	{
		Parts[0]->SetRelativeScale3D(FVector(Radius / 50.f * (1.f + K * 0.6f)));
		Mats[0]->SetScalarParameterValue(TEXT("Opacity"), 0.5f * (1.f - K));
	}
	else if (Kind == EKind::Sphere && Parts.Num())
	{
		// Swell quickly (ease out) to the full radius in the first 45%, then fade while it hangs there.
		const float Grow = FMath::Clamp(K / 0.45f, 0.f, 1.f);
		const float R = Radius * (0.15f + 0.85f * (1.f - FMath::Square(1.f - Grow)));
		Parts[0]->SetRelativeScale3D(FVector(R / 50.f));
		Mats[0]->SetScalarParameterValue(TEXT("Opacity"), K < 0.45f ? 0.4f : 0.4f * (1.f - (K - 0.45f) / 0.55f));
	}
	else if (Kind == EKind::Stain && Parts.Num())
	{
		// In fast, hold, out over the last 30%.
		const float In = FMath::Clamp(Age / 0.2f, 0.f, 1.f), Out = FMath::Clamp((1.f - K) / 0.3f, 0.f, 1.f);
		Mats[0]->SetScalarParameterValue(TEXT("Opacity"), 0.42f * FMath::Min(In, Out));
	}
	else if (Kind == EKind::Bolt)
	{
		for (UMaterialInstanceDynamic* M : Mats) M->SetScalarParameterValue(TEXT("Intensity"), 25.f * (1.f - K));
	}
	else if (Kind == EKind::Smoke && !bSmokeStopped && Age >= Life - SmokeFade)
	{
		// The cloud is done: stop making smoke and let what's in the air drift off before the actor goes.
		bSmokeStopped = true;
		for (UParticleSystemComponent* E : Emitters) if (E) E->Deactivate();
	}
	if (Light) Light->SetIntensity(LightBase * (1.f - K));
	if (Age >= Life) Destroy();
}
