#include "TSFX.h"
#include "TSAssets.h"
#include "TSData.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"

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

namespace
{
	/** Builds a scar's triangles on the ground around a point (each vertex dropped onto the ground under it). */
	struct FTSScarBuilder
	{
		UWorld* W = nullptr;
		FVector At;
		float Radius = 100.f;
		TArray<FVector> V;
		TArray<int32> Tri;

		FVector Ground(const FVector2D& P) const
		{
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(TSScar), false);
			const FVector Top(At.X + P.X, At.Y + P.Y, At.Z + 300.f);
			const float Z = W->LineTraceSingleByChannel(H, Top, Top - FVector(0, 0, 900.f), ECC_Visibility, Q) && H.ImpactNormal.Z > 0.6f ? H.ImpactPoint.Z : At.Z;
			return FVector(P.X, P.Y, Z - At.Z + 2.f);   // relative to the actor, a hair above the ground
		}
		static FVector2D Dir(float Deg) { return FVector2D(FMath::Cos(FMath::DegreesToRadians(Deg)), FMath::Sin(FMath::DegreesToRadians(Deg))); }

		void Quad(const FVector2D& A, const FVector2D& B, float WA, float WB)
		{
			const FVector2D D = (B - A).GetSafeNormal(), N(-D.Y, D.X);
			const int32 I = V.Num();
			V.Add(Ground(A + N * WA)); V.Add(Ground(A - N * WA)); V.Add(Ground(B + N * WB)); V.Add(Ground(B - N * WB));
			Tri.Append({ I, I + 2, I + 1, I + 1, I + 2, I + 3 });
		}
		/** A ragged disc: radius R, each rim point R x (1 - Rough .. 1). */
		void Blotch(const FVector2D& C, float R, float Rough)
		{
			const int32 N = FMath::RandRange(12, 18);
			const int32 I = V.Num();
			V.Add(Ground(C));
			for (int32 K = 0; K < N; ++K) V.Add(Ground(C + Dir(360.f * K / N + FMath::FRandRange(-8.f, 8.f)) * R * FMath::FRandRange(1.f - Rough, 1.f)));
			for (int32 K = 0; K < N; ++K) Tri.Append({ I, I + 1 + (K + 1) % N, I + 1 + K });
		}
		/** A wandering line from P, thinning to a point, branching now and then. */
		void Crack(FVector2D P, float Angle, float Length, float Width, int32 Depth, float Jag, float Branch)
		{
			const int32 Steps = FMath::RandRange(4, 7);
			const float Step = Length / Steps;
			for (int32 S = 0; S < Steps; ++S)
			{
				Angle += FMath::FRandRange(-Jag, Jag);
				const FVector2D Q = P + Dir(Angle) * Step * FMath::FRandRange(0.7f, 1.3f);
				if (Q.Size() > Radius) break;
				const float W0 = Width * (1.f - float(S) / Steps), W1 = Width * (1.f - float(S + 1) / Steps) + 0.6f;
				Quad(P, Q, W0, W1);
				if (Depth < 2 && FMath::FRand() < Branch)
					Crack(Q, Angle + (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(30.f, 65.f), Length * FMath::FRandRange(0.25f, 0.45f), W0 * 0.6f, Depth + 1, Jag, Branch);
				P = Q;
			}
		}
		/** Lines bursting out from around the centre: Count of them, Length x radius, Width at the root. */
		void Burst(int32 Count, float MinLen, float MaxLen, float Width, float Jag, float Branch, float StartAt)
		{
			const float Start = FMath::FRandRange(0.f, 360.f);
			for (int32 I = 0; I < Count; ++I)
			{
				const float A = Start + 360.f * I / Count + FMath::FRandRange(-18.f, 18.f);
				Crack(Dir(A) * Radius * FMath::FRandRange(0.f, StartAt), A, Radius * FMath::FRandRange(MinLen, MaxLen), Width * FMath::FRandRange(0.7f, 1.2f), 0, Jag, Branch);
			}
		}
		void Clear() { V.Reset(); Tri.Reset(); }
	};
}

FVector ATSFX::GroundBelow(UWorld* W, const FVector& P)
{
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(TSGround), false);
	return W && W->LineTraceSingleByChannel(H, P + FVector(0, 0, 60.f), P - FVector(0, 0, 1500.f), ECC_Visibility, Q) ? H.ImpactPoint : P;
}

void ATSFX::Scar(UWorld* W, const FVector& At, const TSJson::FObj& Spec, float DefaultRadius)
{
	if (!W || !Spec || FMath::FRand() >= TSJson::Num(Spec, TEXT("chance"), 1)) return;   // sometimes the ground takes no mark
	const FString Style = TSJson::Str(Spec, TEXT("style"), TEXT("cracks"));
	const UTSData& D = UTSData::Get(W);
	const float R = TSJson::Has(Spec, TEXT("radius")) ? D.Px(TSJson::Num(Spec, TEXT("radius"))) : DefaultRadius;
	const TArray<TSharedPtr<FJsonValue>> LifeRange = TSJson::Arr(Spec, TEXT("life"));
	const float Life = LifeRange.Num() == 2 ? FMath::FRandRange(float(LifeRange[0]->AsNumber()), float(LifeRange[1]->AsNumber())) : float(TSJson::Num(Spec, TEXT("life"), 20));
	const float InDelay = float(TSJson::Num(Spec, TEXT("delay"), 0));

	ATSFX* F = Make(W, At, EKind::Scar, InDelay + Life);
	F->Radius = R;
	F->Delay = InDelay;
	F->MaxOpacity = float(TSJson::Num(Spec, TEXT("opacity"), 0.75));
	F->GlowTime = TSJson::Has(Spec, TEXT("glow")) ? float(TSJson::Num(Spec, TEXT("glowTime"), 2)) : 0.f;

	FTSScarBuilder B;
	B.W = W; B.At = At; B.Radius = R;
	FTSScarBuilder Glow = B;   // embers / the flash, drawn over the scar
	if (Style == TEXT("scorch"))
	{
		B.Blotch(FVector2D::ZeroVector, R * 0.55f, 0.45f);                       // the burnt patch
		for (int32 I = FMath::RandRange(1, 3); I > 0; --I)                         // a few scattered burns around it
			B.Blotch(FVector2D(FMath::FRandRange(-0.5f, 0.5f), FMath::FRandRange(-0.5f, 0.5f)) * R, R * FMath::FRandRange(0.12f, 0.25f), 0.5f);
		B.Burst(FMath::RandRange(7, 12), 0.6f, 1.f, R * 0.06f, 12.f, 0.15f, 0.3f);   // streaks blasted outward
		for (int32 I = FMath::RandRange(4, 8); I > 0; --I)                         // embers
			Glow.Blotch(FVector2D(FMath::FRandRange(-0.45f, 0.45f), FMath::FRandRange(-0.45f, 0.45f)) * R, R * FMath::FRandRange(0.03f, 0.07f), 0.4f);
	}
	else if (Style == TEXT("forks"))
	{
		B.Blotch(FVector2D::ZeroVector, R * 0.18f, 0.5f);                        // where it struck
		B.Burst(FMath::RandRange(3, 6), 0.5f, 1.f, R * 0.035f, 38.f, 0.45f, 0.05f);   // jagged burn lines
		Glow.Blotch(FVector2D::ZeroVector, R * 0.22f, 0.3f);                     // the flash
	}
	else
	{
		B.Burst(FMath::RandRange(5, 9), 0.55f, 0.95f, FMath::FRandRange(4.f, 7.f), 28.f, 0.3f, 0.12f);   // cracks
	}

	auto Section = [&](FTSScarBuilder& S, int32 Index, const FLinearColor& Color, float Intensity)
	{
		if (S.V.IsEmpty()) return;
		if (!F->Crack)
		{
			F->Crack = NewObject<UProceduralMeshComponent>(F);
			F->Crack->SetupAttachment(F->RootComponent);
			F->Crack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F->Crack->SetCastShadow(false);
			F->Crack->RegisterComponent();
		}
		TArray<FVector> Normals; Normals.Init(FVector::UpVector, S.V.Num());
		F->Crack->CreateMeshSection(Index, S.V, S.Tri, Normals, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(TSAssets::Material(F, SeeThroughMat), F);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		M->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		M->SetScalarParameterValue(TEXT("Opacity"), 0.f);
		F->Crack->SetMaterial(Index, M);
		F->Mats.Add(M);   // [0] the scar, [1] the glow
	};
	Section(B, 0, TSJson::Color(TSJson::Str(Spec, TEXT("color"), TEXT("#2a323c"))), 1.f);
	if (F->GlowTime > 0.f) Section(Glow, 1, TSJson::Color(TSJson::Str(Spec, TEXT("glow"))), 6.f);
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
	else if (Kind == EKind::Scar && Mats.Num())
	{
		// Hidden until Delay, in quickly, then out over the last 3 s; the glow flares at Delay and dies over GlowTime.
		const float In = FMath::Clamp((Age - Delay) / 0.5f, 0.f, 1.f), Out = FMath::Clamp((Life - Age) / 3.f, 0.f, 1.f);
		Mats[0]->SetScalarParameterValue(TEXT("Opacity"), MaxOpacity * FMath::Min(In, Out));
		if (Mats.Num() > 1)
		{
			const float G = Age < Delay ? 0.f : FMath::Clamp(1.f - (Age - Delay) / FMath::Max(0.05f, GlowTime), 0.f, 1.f);
			Mats[1]->SetScalarParameterValue(TEXT("Opacity"), 0.9f * G * G);
		}
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
