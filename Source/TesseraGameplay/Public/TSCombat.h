#pragma once

#include "CoreMinimal.h"

class ATSCharacter;
class AActor;

/** One hit. Damage numbers are data units; Knockback is in data units / s (converted with UTSData::Px). */
struct FTSHit
{
	float Base = 0.f;
	FName Scaling;                       // a stats.scaling stat, or none
	float Poise = 0.f;
	float Knockback = 0.f;
	TOptional<FVector> Dir;              // knockback direction (default: away from the source)
	TOptional<FVector> From;             // where the hit comes from, for blocking (default: the source)
	bool bIgnoreYield = false;           // skip the target's OnHurt (e.g. it must really die)
};

/**
 * The damage pipeline, in order:
 *   OnStruck -> damage roll (scaling, crit, armor, Marked, variance) -> guard / perfect guard -> absorb effects ->
 *   health -> OnHurt (may end the hit, e.g. a yield) -> knockback -> OnDamaged (provoke) -> poise / stagger ->
 *   hit invulnerability -> death.
 * Numbers come from the "stats" rules (UTSStatsComponent); words from <world>.text (block, perfectBlock,
 * guardBreak, stagger); the hit effect from <world>.assets.hitEffect.
 */
namespace TSCombat
{
	/** True if the hit connected (including blocked / absorbed), false if it passed through (i-frames). */
	TESSERAGAMEPLAY_API bool Deal(ATSCharacter* Src, ATSCharacter* Target, const FTSHit& Hit);
	TESSERAGAMEPLAY_API void Heal(ATSCharacter* Target, float Amount);
	/** Damage over time: no crit / armor / poise; stops at the target's HealthFloor. */
	TESSERAGAMEPLAY_API void Dot(ATSCharacter* Target, float Amount, AActor* Src);
	/** Live opponents of a character (different team, neither neutral; not leaving). */
	TESSERAGAMEPLAY_API TArray<ATSCharacter*> Opponents(const ATSCharacter* Of);
}
