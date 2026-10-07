#include "TSCharacter.h"
#include "TSSprite.h"
#include "TSLook.h"
#include "TSData.h"
#include "TSAssets.h"
#include "TSCombat.h"
#include "TSAreaEvents.h"
#include "TSCharacterEvents.h"
#include "EngineUtils.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Materials/MaterialInstanceDynamic.h"

ATSCharacter::ATSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(36.f, 90.f);

	// A UE5-mannequin-style mesh: root at the feet, facing +Y; the capsule's origin is its centre.
	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -90.f), FRotator(0, -90.f, 0));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0, 720.f, 0);
	Move->MaxAcceleration = 2600.f;
	Move->BrakingDecelerationWalking = 2400.f;
	Move->GroundFriction = 8.f;
	Move->JumpZVelocity = 520.f;
	Move->AirControl = 0.35f;
	bUseControllerRotationYaw = false;

	Stats = CreateDefaultSubobject<UTSStatsComponent>(TEXT("Stats"));
}

float ATSCharacter::Radius() const { return GetCapsuleComponent()->GetScaledCapsuleRadius(); }

UAnimInstance* ATSCharacter::Anim() const { return GetMesh() ? GetMesh()->GetAnimInstance() : nullptr; }

float ATSCharacter::PlayMontage(UAnimMontage* Montage, float Rate, FName Section)
{
	UAnimInstance* A = Anim();
	if (!A || !Montage) return 0.f;
	const float Len = A->Montage_Play(Montage, Rate);
	if (Section != NAME_None) A->Montage_JumpToSection(Section, Montage);
	return Len;
}

void ATSCharacter::SetWeaponKits(const TArray<FString>& KitIds)
{
	for (USceneComponent* C : WeaponParts) if (C) C->DestroyComponent();
	WeaponParts.Reset();
	KitMounts.Reset();
	KitGlows.Reset();
	KitParts.Reset();
	if (!CanHoldKits()) return;

	const TSJson::FObj W3 = UTSData::Get(this).World();
	auto Vec = [](const TSJson::FObj& O, const TCHAR* K, const FVector& Def)
	{
		const TArray<TSharedPtr<FJsonValue>> A = TSJson::Arr(O, K);
		return A.Num() == 3 ? FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) : Def;
	};
	UMaterialInterface* Glow = TSAssets::Material(this, TEXT("glow"));

	for (const FString& KitId : KitIds)
	{
		const TSJson::FObj Kit = TSJson::Obj(TSJson::Obj(W3, TEXT("kits")), KitId);
		if (!Kit) continue;
		const FString Bone = TSJson::Str(Kit, TEXT("bone"), TEXT("hand_r"));
		const TSJson::FObj Mount = TSJson::Obj(TSJson::Obj(W3, TEXT("mounts")), Bone);

		// A mount per kit: positions the kit's "weapon space" (+Z out of the fist) on the bone.
		USceneComponent* Root = NewObject<USceneComponent>(this);
		Root->SetupAttachment(BodyMesh(), FName(Bone));
		// A kit can override its bone's mount (e.g. a staff stands upright instead of pointing forward).
		const FVector R = Vec(Kit, TEXT("mountRot"), Vec(Mount, TEXT("rot"), FVector::ZeroVector));
		Root->SetRelativeLocationAndRotation(Vec(Kit, TEXT("mountLoc"), Vec(Mount, TEXT("loc"), FVector::ZeroVector)), FRotator(R.X, R.Y, R.Z));
		Root->RegisterComponent();
		WeaponParts.Add(Root);
		FKitMount& KM = KitMounts.Add(KitId, { Root, FName(Bone), Root->GetRelativeTransform(), false });
		if (const TSJson::FObj H = TSJson::Obj(Kit, TEXT("holster")))
		{
			// Holster given as the kit's X and Z axes in the holster bone's space (easier to reason about than angles).
			KM.bHasHolster = true;
			KM.HolsterBone = FName(TSJson::Str(H, TEXT("bone"), TEXT("pelvis")));
			KM.Holster = FTransform(FRotationMatrix::MakeFromXZ(Vec(H, TEXT("x"), FVector::ForwardVector), Vec(H, TEXT("z"), FVector::UpVector)).ToQuat(), Vec(H, TEXT("loc"), FVector::ZeroVector));
		}

		for (const TSharedPtr<FJsonValue>& V : TSJson::Arr(Kit, TEXT("parts")))
		{
			const TSJson::FObj Part = V->AsObject();
			UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
			M->SetStaticMesh(TSAssets::Shape(TSJson::Str(Part, TEXT("shape"), TEXT("Cube"))));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (TSJson::Has(Part, TEXT("glow")))
			{
				UMaterialInstanceDynamic* G = UMaterialInstanceDynamic::Create(Glow, this);
				G->SetVectorParameterValue(TEXT("Color"), TSJson::Color(TSJson::Str(Part, TEXT("glow"))));
				G->SetScalarParameterValue(TEXT("Intensity"), 10.f);
				M->SetMaterial(0, G);
				UPointLightComponent* L = NewObject<UPointLightComponent>(this);
				L->SetupAttachment(M);
				L->SetIntensityUnits(ELightUnits::Candelas);
				L->SetIntensity(4.f);
				L->SetAttenuationRadius(250.f);
				L->SetLightColor(TSJson::Color(TSJson::Str(Part, TEXT("glow"))));
				L->SetCastShadows(false);
				L->RegisterComponent();
				WeaponParts.Add(L);
				KitGlows.Add(KitId, { M, G, L, Vec(Part, TEXT("size"), FVector(10)) / 100.f });
			}
			else M->SetMaterial(0, TSAssets::ResolveMaterial(TSJson::Str(Part, TEXT("mat"), TSAssets::Path(this, TEXT("kitPart")))));   // default: <world>.assets.kitPart
			M->SetupAttachment(Root);
			const FVector PR = Vec(Part, TEXT("rot"), FVector::ZeroVector);
			M->SetRelativeLocationAndRotation(Vec(Part, TEXT("loc"), FVector::ZeroVector), FRotator(PR.X, PR.Y, PR.Z));
			M->SetRelativeScale3D(Vec(Part, TEXT("size"), FVector(10)) / 100.f);
			M->RegisterComponent();
			WeaponParts.Add(M);
			if (TSJson::Has(Part, TEXT("id"))) KitParts.Add(KitId + TEXT("/") + TSJson::Str(Part, TEXT("id")), M);
		}
	}
}

void ATSCharacter::PoseKit(const FString& KitId, bool bOffBone, const FTransform& BodyRelative)
{
	FKitMount* K = KitMounts.Find(KitId);
	if (!K || !K->Root || K->bOffBone == bOffBone) return;
	K->bOffBone = bOffBone;
	if (bOffBone)
	{
		K->Root->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		K->Root->SetRelativeTransform(BodyRelative);
	}
	else
	{
		K->Root->AttachToComponent(BodyMesh(), FAttachmentTransformRules::KeepRelativeTransform, K->Bone);
		K->Root->SetRelativeTransform(K->OnBone);
	}
}

void ATSCharacter::SetKitHolstered(const FString& KitId, bool bHolstered)
{
	FKitMount* K = KitMounts.Find(KitId);
	if (!K || !K->Root || !K->bHasHolster || K->bHolstered == bHolstered) return;
	K->bHolstered = bHolstered;
	K->Root->AttachToComponent(BodyMesh(), FAttachmentTransformRules::KeepRelativeTransform, bHolstered ? K->HolsterBone : K->Bone);
	K->Root->SetRelativeTransform(bHolstered ? K->Holster : K->OnBone);
}

UStaticMeshComponent* ATSCharacter::KitGlow(const FString& KitId) const
{
	const FKitGlow* G = KitGlows.Find(KitId);
	return G ? G->Mesh.Get() : nullptr;
}

void ATSCharacter::SetKitGlow(const FString& KitId, float InFlash)
{
	if (FKitGlow* G = KitGlows.Find(KitId))
	{
		G->Mat->SetScalarParameterValue(TEXT("Intensity"), 10.f + 30.f * InFlash);
		G->Light->SetIntensity(4.f + 260.f * InFlash);
		G->Mesh->SetRelativeScale3D(G->BaseScale * (1.f + 0.4f * InFlash));
	}
}

void ATSCharacter::Knock(const FVector& Velocity) { KnockVelocity += FVector(Velocity.X, Velocity.Y, 0.f); }

UMaterialInterface* ATSCharacter::FlashMaterial() const
{
	// Looked up each time, not cached in a static: a New Game tears the world down in between.
	return TSAssets::Material(this, TEXT("flash"));
}

void ATSCharacter::Flash()
{
	FlashTime = 0.1f;
	if (USkinnedMeshComponent* Body = BodyMesh()) Body->SetOverlayMaterial(FlashMaterial());
}

void ATSCharacter::ClearFlash()
{
	if (USkinnedMeshComponent* Body = BodyMesh()) Body->SetOverlayMaterial(nullptr);
}

void ATSCharacter::Die(AActor* Killer)
{
	if (bDead) return;
	bDead = true;
	DeathTime = 0.f;
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UTSCharacterEvents* Events = UTSCharacterEvents::Get(this)) Events->OnDied.Broadcast(this, Killer);   // tell the game
}

void ATSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashTime > 0.f)
	{
		FlashTime -= DeltaSeconds;
		if (FlashTime <= 0.f) ClearFlash();
	}

	if (bDead) { DeathTime += DeltaSeconds; return; }

	Tags.Tick(DeltaSeconds);

	// Frozen: stand still, the 3D body's animation stops (a sprite holds its frame), no knockback.
	const bool bFrozen = IsFrozen();
	if (USkeletalMeshComponent* Body = GetMesh()) Body->bPauseAnims = bFrozen;
	if (bFrozen)
	{
		GetCharacterMovement()->StopMovementImmediately();
		KnockVelocity = FVector::ZeroVector;
	}

	// A barrier guard (keepOut): hold opponents at its edge, after everyone has moved this frame (UTSKeepOut).
	if (!KeepOut && GuardStyle() && TSJson::Num(GuardStyle(), TEXT("keepOut"), 0) > 0)
	{
		KeepOut = NewObject<UTSKeepOut>(this, TEXT("KeepOut"));
		KeepOut->RegisterComponent();
	}

	// Knockback decays fast (x0.002 per second).
	if (KnockVelocity.SizeSquared() > 25.f)
	{
		AddMovementInput(FVector::ZeroVector);
		GetCharacterMovement()->Velocity.X = KnockVelocity.X;
		GetCharacterMovement()->Velocity.Y = KnockVelocity.Y;
		KnockVelocity *= FMath::Pow(0.002f, DeltaSeconds);
	}
	else KnockVelocity = FVector::ZeroVector;

	PoiseTimer -= DeltaSeconds;
	if (PoiseTimer <= 0.f) Poise = MaxPoise;
}

void ATSCharacter::HoldOutOpponents()
{
	const TSJson::FObj Guard = bDead ? nullptr : GuardStyle();
	const double Px = Guard ? TSJson::Num(Guard, TEXT("keepOut"), 0) : 0;
	if (Px <= 0) { BarrierUpFor = 0.f; return; }
	const bool bJustRaised = BarrierUpFor == 0.f;
	BarrierUpFor += GetWorld()->GetDeltaSeconds();
	const float R = UTSData::Get(this).Px(Px);
	const FVector Me = GetActorLocation();
	if (bJustRaised)   // non-characters inside (animals...) are the game's to throw
		if (UTSAreaEvents* Events = UTSAreaEvents::Get(this)) Events->OnPush.Broadcast(Me, R);
	for (TActorIterator<ATSCharacter> It(GetWorld()); It; ++It)
	{
		ATSCharacter* E = *It;
		if (E == this || E->IsDead() || E->IsLeaving()) continue;
		FVector To = E->GetActorLocation() - Me;
		To.Z = 0.f;
		const float Min = R + E->Radius();
		const float Dist = To.Size();
		if (Dist >= Min) continue;
		const FVector Out = To.IsNearlyZero() ? -Facing() : To / Dist;
		if (bJustRaised)
		{
			// Caught inside as it goes up: thrown clear. Knockback slides ~1/6.2 of its speed before it stops
			// (it decays x0.002 per second), so this lands it a little past the edge.
			E->KnockVelocity = Out * (Min - Dist + 60.f) * 6.2f;
			E->Stagger(0.4f);
			continue;
		}
		if (E->KnockVelocity.SizeSquared() > 25.f && FVector::DotProduct(E->KnockVelocity, Out) > 0.f) continue;   // still flying out
		E->SetActorLocation(FVector(Me.X, Me.Y, E->GetActorLocation().Z) + Out * Min);
		// Drop whatever carried it inward (walking, a lunge).
		FVector& V = E->GetCharacterMovement()->Velocity;
		const float In = FVector::DotProduct(V, -Out);
		if (In > 0.f) V += Out * In;
		E->KnockVelocity = FVector::ZeroVector;
	}
}

UTSKeepOut::UTSKeepOut()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;   // after every character's movement this frame
}

void UTSKeepOut::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	if (ATSCharacter* C = Cast<ATSCharacter>(GetOwner())) C->HoldOutOpponents();
}

bool ATSCharacter::TintForTag(const UObject* WorldContext, FName Tag, FLinearColor& Out)
{
	const TSJson::FObj Tints = TSJson::Obj(UTSData::Get(WorldContext).World(), TEXT("statusTints"));
	const FString Hex = TSJson::Str(Tints, Tag.ToString());
	if (Hex.IsEmpty()) return false;
	Out = TSJson::Color(Hex);
	return true;
}

FLinearColor ATSCharacter::StatusTint() const
{
	FLinearColor C;
	for (const auto& KV : Tags.Map) if (TintForTag(this, KV.Key, C)) return C;
	return FLinearColor::White;
}

// ---------------------------------------------------------------------------------------------
// 2D looks
// ---------------------------------------------------------------------------------------------

void ATSCharacter::UseSprite(const FString& Sheet)
{
	if (!TSLook::IsSprite() || Sheet.IsEmpty()) return;
	if (!Sprite)
	{
		Sprite = NewObject<UTSSpriteComponent>(this, TEXT("Sprite"));
		Sprite->SetupAttachment(RootComponent);
		Sprite->RegisterComponent();
	}
	Sprite->Setup(Sheet);
	HeadZ = 100.f;
	HideBody();
}

void ATSCharacter::HideBody()
{
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetVisibility(false, true);
	if (BodyMesh() && BodyMesh() != GetMesh()) BodyMesh()->SetVisibility(false, true);
}
