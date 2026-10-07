#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TSAnimNotifies.h"
#include "TSStats.h"
#include "TSJson.h"
#include "TSCharacter.generated.h"

class UStaticMeshComponent;
class UAnimMontage;
class UAnimInstance;
class UMaterialInstanceDynamic;
class UTSSpriteComponent;
struct FTSHit;

/** Who fights whom: the player's side, its enemies, and bystanders nobody attacks. */
UENUM()
enum class ETSTeam : uint8 { Neutral, Player, Hostile };

/**
 * Base for every character in a Tessera game: the hero, enemies, villagers.
 *
 *   - stats (UTSStatsComponent), timed tags, poise and stagger, knockback, hit flash, death
 *   - weapon kits built from data (<world>.kits / mounts): simple shapes attached to bones, holsters, glowing parts
 *   - a pixel-art sprite (UTSSpriteComponent) in the 2D looks; the 3D body stays hidden but keeps animating
 *   - who it is in conversation (name, colour, dialogue root, memory key) for the game's story system
 *   - hooks the damage pipeline and abilities call, which a game overrides (factions, duels, aiming...)
 */
UCLASS(Abstract)
class TESSERAGAMEPLAY_API ATSCharacter : public ACharacter, public ITSAttacker
{
	GENERATED_BODY()

public:
	ATSCharacter();

	ETSTeam Team = ETSTeam::Neutral;

	// ---- identity (dialogue / UI) ----
	FString DisplayName;
	FLinearColor NameColor = FLinearColor::White;
	FString DialogueRoot;      // root dialogue node ("" = can't talk)
	FString TalkKey;           // key for disposition / seeded rolls

	UPROPERTY(VisibleAnywhere, Category = "Tessera") TObjectPtr<UTSStatsComponent> Stats;
	FTSTags Tags;

	// ---- damage-pipeline hooks ----
	/** A non-hostile member of a faction: won't fight, can be talked to. */
	virtual bool IsPassive() const { return false; }
	virtual bool IsLeaving() const { return false; }
	virtual FString FactionId() const { return FString(); }
	/** While guarding: the guard's data { arc, reduction, perfectWindow, staminaPerDamage, ... }, else null. */
	virtual TSJson::FObj GuardStyle() const { return nullptr; }
	float GuardTime = 0.f;                 // seconds since the guard went up (perfect-block window)
	/** Struck by Src, before the damage roll (e.g. a bystander's faction turns hostile). */
	virtual void OnStruck(ATSCharacter* Src) {}
	/** Health just went down; return true to end the hit here (e.g. a duel opponent yields instead of dying). */
	virtual bool OnHurt(ATSCharacter* Src) { return false; }
	/** After the hit: provoke, alert. */
	virtual void OnDamaged(ATSCharacter* Src) {}
	/** Health damage over time can't take it below this (e.g. a duel ends in a yield, not a death). */
	virtual float HealthFloor() const { return 0.f; }
	virtual float KnockbackMul() const { return 1.f; }
	/** Seconds of invulnerability after taking a hit (stops stun-locks; usually just the hero). */
	float HitIframes = 0.f;
	float MarkMul = 1.f;                   // damage taken x this while tagged "Marked"

	// ---- ability / aiming hooks (the hero overrides these) ----
	virtual int32 Level() const { return FMath::Max(1, FMath::RoundToInt(Stats->Get(TEXT("level")))); }
	/** Not stunned / mid-dodge: may start an ability. */
	virtual bool CanAct() const { return !bDead && !Tags.Has(TEXT("Staggered")); }
	/** An ability's "requires" (e.g. "shield"); false with Why to refuse. */
	virtual bool MeetsRequirement(const FString& Requirement, FString& Why) const { return true; }
	virtual FVector AimDirection(const FVector& From) const { return Facing(); }
	FVector AimDirection() const { return AimDirection(Chest()); }
	/** Where shots leave from (a staff tip, a bow hand...). */
	virtual FVector Muzzle() const { return Chest() + Facing() * 50.f; }
	/** Where a lobbed shot from From should land, no further than MaxRange. */
	virtual FVector ArrowTarget(const FVector& From, float MaxRange) const { return From + AimDirection(From) * MaxRange; }
	virtual void FaceAim() {}
	/** Invulnerable dash; with a Strike definition it damages everything it passes through. */
	virtual void StartDash(const FVector& Dir, float Speed, float Duration, const TSJson::FObj& Strike) {}
	/** After an ability fires (play a pose, show the bow...). */
	virtual void OnAbilityUsed(const TSJson::FObj& Ability) {}
	/** A talking ability (e.g. a daze) opened a conversation with Target at Node. */
	virtual void OnParley(ATSCharacter* Target, const FString& Node) {}
	/** Can words reach it (a daze, a parley)? And its mid-fight dialogue node. */
	virtual bool IsReasonable() const { return false; }
	virtual FString ParleyNode() const { return FString(); }
	/** Smoke, a feint...: forget the hero if hunting them. */
	virtual void LoseTrack() {}
	/** Showing an attack wind-up (sprites hold the wind-up frame). */
	virtual bool IsWindingUp() const { return bSpriteHold; }

	// ---- body ----
	void Stagger(float Duration) { Tags.Add(TEXT("Staggered"), Duration); OnStaggered(); }
	virtual void OnStaggered() {}
	FVector Chest() const { return GetActorLocation() + FVector(0, 0, 30.f * GetActorScale3D().Z); }
	FVector Head() const { return GetActorLocation() + FVector(0, 0, HeadZ * GetActorScale3D().Z); }
	float HeadZ = 80.f;        // top of the head above the actor centre (a sprite is taller than a mannequin)
	float Radius() const;
	/** Unit vector the character faces (yaw only). */
	FVector Facing() const { return GetActorForwardVector().GetSafeNormal2D(); }
	float MaxPoise = 50.f, Poise = 50.f, PoiseTimer = 0.f;
	/** Push the character (data units / s). */
	void Knock(const FVector& Velocity);
	virtual void Flash();
	virtual void Die(AActor* Killer);
	bool IsDead() const { return bDead; }

	// ---- weapon kits (<world>.kits, mounts) ----
	void SetWeaponKits(const TArray<FString>& KitIds);
	/** Move a kit off its bone to a pose relative to the body, or back. */
	void PoseKit(const FString& KitId, bool bOffBone, const FTransform& BodyRelative = FTransform::Identity);
	/** Move a kit between its hand mount and its holster. */
	void SetKitHolstered(const FString& KitId, bool bHolstered);
	UStaticMeshComponent* KitGlow(const FString& KitId) const;
	UStaticMeshComponent* KitPart(const FString& KitId, const FString& PartId) const { const TObjectPtr<UStaticMeshComponent>* P = KitParts.Find(KitId + TEXT("/") + PartId); return P ? P->Get() : nullptr; }
	USceneComponent* KitRoot(const FString& KitId) const { const FKitMount* K = KitMounts.Find(KitId); return K ? K->Root.Get() : nullptr; }
	/** Brighten a kit glow: 0 = resting, 1 = full flash. */
	void SetKitGlow(const FString& KitId, float Flash);
	/** No kits for bodies without bones (blobs...). */
	virtual bool CanHoldKits() const { return true; }

	/** The mesh that is actually rendered (kits attach here); a posed copy for some characters. */
	virtual USkinnedMeshComponent* BodyMesh() const { return GetMesh(); }

	// ---- 2D looks ----
	/** Show sprite sheet SPR_<Sheet> instead of the 3D body (no-op in the 3D look). */
	void UseSprite(const FString& Sheet);
	/** Hide the 3D body and kits (they keep animating, so montage hit timing is unchanged). */
	virtual void HideBody();
	float SpriteAttackAt = -100.f;   // when the last attack/cast/shot started (sprite plays its swing frames)
	bool bSpriteHold = false;        // hold the wind-up frame (drawing a bow)
	UPROPERTY() TObjectPtr<UTSSpriteComponent> Sprite;

	UAnimInstance* Anim() const;
	float PlayMontage(UAnimMontage* Montage, float Rate = 1.f, FName Section = NAME_None);

	// ITSAttacker (default: no melee)
	virtual void DoAttackTrace(FName SourceBone) override {}
	virtual void CheckCombo() override {}

protected:
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY() TArray<TObjectPtr<USceneComponent>> WeaponParts;
	struct FKitMount { TObjectPtr<USceneComponent> Root; FName Bone; FTransform OnBone; bool bOffBone = false;
		FName HolsterBone; FTransform Holster; bool bHasHolster = false; bool bHolstered = false; };
	struct FKitGlow { TObjectPtr<UStaticMeshComponent> Mesh; TObjectPtr<UMaterialInstanceDynamic> Mat; TObjectPtr<class UPointLightComponent> Light; FVector BaseScale = FVector::OneVector; };
	TMap<FString, FKitGlow> KitGlows;
	UPROPERTY() TMap<FString, TObjectPtr<UStaticMeshComponent>> KitParts;
	TMap<FString, FKitMount> KitMounts;

	bool bDead = false;
	float FlashTime = 0.f;
	float DeathTime = 0.f;
	FVector KnockVelocity = FVector::ZeroVector;
	/** The overlay material for hit flashes (<world>.assets.flash). */
	UMaterialInterface* FlashMaterial() const;
	virtual void ClearFlash();
};
