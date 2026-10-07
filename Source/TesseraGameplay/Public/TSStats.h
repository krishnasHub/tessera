#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TSJson.h"
#include "TSStats.generated.h"

/** A timed effect: heal or damage over time, stat bonuses, a damage absorb, an on-hit payload (e.g. a poison). */
struct FTSEffect
{
	FName Id;
	FString Name;
	float Duration = 0, Remaining = 0, Period = 0, TickTimer = 0;
	float Heal = 0;                  // per tick
	float Dot = 0;                   // damage per tick
	float Absorb = 0;                // damage soaked before health
	TMap<FName, float> Mods;         // stat bonuses while active
	TSJson::FObj OnHit;              // data the owner's attacks apply while this lasts (game-defined)
	TWeakObjectPtr<AActor> Source;
};

/** Gameplay tags with optional durations (Invulnerable, Staggered, Dodging, Hidden, Slowed, Marked...). */
struct FTSTags
{
	TMap<FName, float> Map;   // tag -> seconds left (BIG_NUMBER = until removed)

	void Add(FName Tag, float Duration = BIG_NUMBER) { float& T = Map.FindOrAdd(Tag); T = FMath::Max(T, Duration); }
	bool Has(FName Tag) const { return Map.Contains(Tag); }
	void Remove(FName Tag) { Map.Remove(Tag); }
	void Clear() { Map.Empty(); }
	void Tick(float Dt)
	{
		for (auto It = Map.CreateIterator(); It; ++It)
		{
			if (It->Value >= BIG_NUMBER * 0.5f) continue;
			It->Value -= Dt;
			if (It->Value <= 0) It.RemoveCurrent();
		}
	}
};

/** A resource pool's live state (health, stamina, mana, energy...). */
struct FTSPool
{
	float Current = 0.f;
	float Delay = 0.f;        // seconds until it regenerates again
	float RegenMul = 1.f;     // e.g. lowered while blocking
	float Lock = 0.f;         // seconds of no regeneration
};

/**
 * Attributes, resource pools, modifiers and timed effects. Every formula is data (the "stats" section):
 *
 *   "stats": {
 *     "health": "hp",                                     // the pool damage comes out of
 *     "pools": { "<id>": { "max": Rule, "regen": Rule, "regenDelay": n, "round": bool } },
 *     "crit": Rule,  "critMultiplier": n,                 // crit chance in percent
 *     "armorStat": "armor", "armorConstant": n,           // damage x K / (K + armor)
 *     "variance": n, "scaling": { "<stat>": n },          // damage x random(1 +- variance) x (1 + stat x n)
 *     "staggerTime": n, "poiseRegenDelay": n
 *   }
 *   Rule = { "flat": "<stat>", "base": n, "per": { "<stat>": n } }   ->  stat(flat) + base + sum(stat x n)
 *   n    = a number, or the name of a number in "tuning"
 *
 * Attributes are free-form names (Base + equipment modifiers + effect bonuses). An ability or item cost keyed by
 * a pool id ({ "mana": 10 }) spends that pool.
 */
UCLASS(ClassGroup = (Tessera), meta = (BlueprintSpawnableComponent))
class TESSERAGAMEPLAY_API UTSStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSStatsComponent();

	TMap<FName, float> Base;
	TArray<FTSEffect> Effects;

	float Get(FName Stat) const;

	/** The health pool ("stats.health"). */
	float& Health() { return Pool(HealthId()).Current; }
	float Health() const { return Cur(HealthId()); }
	float MaxHealth() const { return Max(HealthId()); }
	FName HealthId() const;

	/** A pool by id; Pool() adds it if needed. */
	FTSPool& Pool(FName Id) { return Pools.FindOrAdd(Id); }
	float Cur(FName Id) const { const FTSPool* P = Pools.Find(Id); return P ? P->Current : 0.f; }
	float Max(FName Id) const;
	bool IsPool(FName Id) const;
	/** Spend Amount (false, and nothing spent, if there isn't enough); pauses the pool's regeneration. */
	bool Spend(FName Id, float Amount);

	/** Crit chance 0..1. */
	float CritChance() const;
	float Armor() const;
	/** Damage multiplier from a scaling stat (1 + stat x per-point). */
	float ScaleBy(FName Stat) const;

	void Fill();
	void ClampPools();
	void AddModifiers(FName Source, const TMap<FName, float>& InMods);
	void RemoveModifiers(FName Source);
	void AddEffect(const FTSEffect& E);

	/** Called by the owner each frame: regeneration and effect ticks (reported through the callbacks). */
	void TickStats(float Dt, TFunctionRef<void(float)> OnHeal, TFunctionRef<void(float, AActor*)> OnDot);

	/** The "stats" rules (for combat). */
	TSJson::FObj Rules() const;
	/** A rules number ("critMultiplier" -> 1.6), resolving tuning names. */
	double Rule(const FString& Key, double Default) const;

private:
	struct FMod { FName Source; FName Stat; float Value; };
	TArray<FMod> Mods;
	TMap<FName, FTSPool> Pools;
	float Eval(const TSJson::FObj& R) const;
	TSJson::FObj PoolDef(FName Id) const;
};
