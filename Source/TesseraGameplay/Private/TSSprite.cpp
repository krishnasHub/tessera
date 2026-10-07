#include "TSSprite.h"
#include "TSCharacter.h"
#include "TSLook.h"
#include "TSAssets.h"

#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	enum EAct { Idle = 0, Walk = 1, Attack = 2, Hurt = 3 };
}

UTSSpriteComponent::UTSSpriteComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;   // after movement, so the card never lags its owner
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	CastShadow = true;
	// A flat card has paper-thin bounds; the occlusion culler can then decide a character standing on uneven ground
	// is buried in it and stop drawing them (seen in the ruins). Roomier bounds keep the test honest.
	BoundsScale = 2.f;
}

void UTSSpriteComponent::Setup(const FString& Sheet)
{
	if (Sheet == SheetName && Mat) return;
	SheetName = Sheet;
	SetStaticMesh(TSAssets::Shape(TEXT("Plane")));
	Mat = TSLook::SpriteMaterial(this, TEXT("SPR_") + Sheet, TSSpriteSheet::Cols, TSSpriteSheet::Rows);
	if (Mat) SetMaterial(0, Mat);
	HideCheck = 0.f;
	if (TSLook::Mode() == TSLook::EMode::Flat2D && !Shadow)
	{
		SetCastShadow(false);
		Shadow = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("SpriteShadow"));
		Shadow->SetStaticMesh(TSAssets::Shape(TEXT("Plane")));
		Shadow->SetMaterial(0, TSLook::PropMaterial(TEXT("PR_Shadow")));
		Shadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shadow->SetCastShadow(false);
		Shadow->SetUsingAbsoluteLocation(true);
		Shadow->SetUsingAbsoluteRotation(true);
		Shadow->SetUsingAbsoluteScale(true);
		Shadow->SetupAttachment(GetOwner()->GetRootComponent());
		Shadow->RegisterComponent();
	}
}

void UTSSpriteComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	ATSCharacter* C = Cast<ATSCharacter>(GetOwner());
	if (!C || !Mat) return;
	const bool bFrozen = C->IsFrozen();
	if (!bFrozen) Clock += Dt;
	const FLinearColor Tint = C->StatusTint();   // e.g. whitish blue while frozen (<world>.statusTints)
	if (!Tint.Equals(LastTint)) { Mat->SetVectorParameterValue(TEXT("Tint"), Tint); LastTint = Tint; }

	// Keep the 3D body hidden (weapon kits are rebuilt on style swaps). It still animates underneath.
	HideCheck -= Dt;
	if (HideCheck <= 0.f) { HideCheck = 0.5f; C->HideBody(); }

	// Took damage since last frame?
	const float HP = C->Stats->Health();
	if (LastHP >= 0.f && HP < LastHP - 0.01f) HurtT = 0.2f;
	LastHP = HP;
	HurtT -= Dt;

	// Direction relative to the fixed camera: 0 = facing the camera, 1 = away, 2 = side (mirrored for left).
	const float CamYaw = TSLook::CameraRotation().Yaw;
	const FVector Fwd = FRotator(0, CamYaw, 0).Vector(), Right = FRotator(0, CamYaw + 90.f, 0).Vector();
	const FVector F = C->Facing();
	const float DF = FVector::DotProduct(F, Fwd), DR = FVector::DotProduct(F, Right);
	const int32 Dir = DF > 0.6f ? 1 : DF < -0.6f ? 0 : 2;
	const bool bFlip = Dir == 2 && DR < 0.f;

	// Action and frame.
	int32 Row = 0, Col = 0;
	const float SinceAttack = GetWorld()->GetTimeSeconds() - C->SpriteAttackAt;
	const bool bWindup = C->IsWindingUp();
	if (bFrozen) { Row = HeldRow; Col = HeldCol; }   // frozen mid-step: hold the frame
	else if (C->IsDead()) { Row = 12; Col = 0; }
	else
	{
		int32 Act = Idle;
		if (HurtT > 0.f) { Act = Hurt; Col = 0; }
		else if (SinceAttack < 0.3f) { Act = Attack; Col = 1 + FMath::Min(2, int32(SinceAttack / 0.1f)); }   // swing, strike, recover
		else if (bWindup) { Act = Attack; Col = 0; }
		else if (C->GuardStyle().IsValid()) { Act = -1; Row = 12; Col = 1 + Dir; }   // shield raised (row 12, cols 1-3)
		else if (C->GetVelocity().Size2D() > 25.f) { Act = Walk; Col = int32(Clock * 9.f) % 4; }
		else { Act = Idle; Col = int32(Clock * 2.2f) % 2; }
		if (Act >= 0) Row = Dir * 4 + Act;
	}
	HeldRow = Row; HeldCol = Col;
	Mat->SetScalarParameterValue(TEXT("Row"), float(Row));
	Mat->SetScalarParameterValue(TEXT("Col"), float(Col));
	Mat->SetScalarParameterValue(TEXT("Flip"), bFlip ? 1.f : 0.f);
	Mat->SetScalarParameterValue(TEXT("Flash"), HurtT > 0.f && !bFrozen ? 0.75f : 0.f);

	// Placement: the card's bottom edge at the feet (the art leaves ~2 px under the boots).
	const float Units = TSLook::SpriteUnits() * C->GetActorScale3D().Z;
	const float Size = TSSpriteSheet::Frame * Units;
	// Standing upright (stretched to look the same): a tall character never leans into a wall behind them.
	const FRotator R = TSLook::StandingRotation();
	const FVector CardUp = TSLook::StandingUp();
	const float Stretch = TSLook::StandingStretch();
	FVector Feet = C->GetActorLocation() - FVector(0, 0, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FVector Center = Feet + CardUp * (Size * 0.5f - 2.5f * Units) * Stretch;
	if (TSLook::Mode() == TSLook::EMode::Flat2D) Center.Z = TSLook::FlatSortZ(Feet.Y);
	SetWorldLocationAndRotation(Center, R);
	SetWorldScale3D(FVector(Size / 100.f, Size * Stretch / 100.f, 1.f));
	if (Shadow)
	{
		Shadow->SetVisibility(!C->IsDead());
		Shadow->SetWorldLocationAndRotation(FVector(Feet.X, Feet.Y, 2.f) + CardUp * Units, R);
		Shadow->SetWorldScale3D(FVector(20.f * Units / 100.f, 7.f * Units / 100.f, 1.f));
	}
}
