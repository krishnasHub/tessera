#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TSJson.h"
#include "TSCombat.h"
#include "TSAbilities.generated.h"

class ATSCharacter;
class UTSData;

/** What an ability type's implementation gets: who casts it, its data, and helpers. */
struct TESSERAGAMEPLAY_API FTSAbilityContext
{
	ATSCharacter* Caster = nullptr;
	TSJson::FObj Def;
	UWorld* World = nullptr;
	const UTSData* Data = nullptr;
	FLinearColor Color = FLinearColor::White;
	FVector Ground = FVector::ZeroVector;    // the caster's feet

	/** A hit from the ability's data (scaling, poise, knockback) with this base damage. */
	FTSHit MakeHit(float Base) const;
	/** A number from the ability's data, scaled by its "scaling" stat. */
	float Scaled(float V) const;
	/** Data number / distance (converted with UTSData::Px). */
	double Num(const TCHAR* Key, double Default) const;
	float Dist(const TCHAR* Key, double Default) const;
	/** The opponent nearest the caster's aim, within Range and in sight. */
	ATSCharacter* TargetNearAim(float Range) const;
	/** Say why it didn't work, over the caster's head. */
	void Fail(const FString& Msg) const;
};

/**
 * Abilities (keys 1-4, a picker...). Each is data ("abilities": { <id>: { type, name, color, cooldown,
 * unlockLevel, requires, <pool>: cost, ... } }); behaviour comes from its "type", one implementation per type.
 *
 * Built in: projectile, aoe, cone, dashStrike, buff, blink, chain, smoke, weaponBuff, heal, daze, mark.
 * A game adds its own with RegisterType("hack", ...). A cast with no valid target costs nothing and starts no
 * cooldown. Any key naming a stat pool ("mana": 10, "stamina": 20) is a cost.
 * Words (<world>.text): unlocksAt, notReady, notEnough, noTarget, notReasonable, notFighting, dazed, staggered.
 */
UCLASS(ClassGroup = (Tessera))
class TESSERAGAMEPLAY_API UTSAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSAbilityComponent();

	/** Returns true if the ability fired (false: nothing happened, nothing is spent). */
	using FTypeFn = TFunction<bool(const FTSAbilityContext& Ctx)>;
	static void RegisterType(const FString& Type, FTypeFn Fn);

	TArray<FString> Ids;
	TMap<FString, float> Cooldowns;

	void Setup(const TArray<FString>& InIds);
	TSJson::FObj Def(const FString& Id) const;
	bool Unlocked(const FString& Id) const;
	bool CanAfford(const TSJson::FObj& D) const;
	bool TryActivate(int32 Slot);
	void TickCooldowns(float Dt);

private:
	ATSCharacter* Caster() const;
	static TMap<FString, FTypeFn>& Types();
	static void RegisterBuiltIns();
};
