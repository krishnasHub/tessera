#pragma once

#include "CoreMinimal.h"
#include "TSJson.h"

class ATSCharacter;

/** How far and how wide a character notices others. */
struct TESSERAGAMEPLAY_API FTSSenses
{
	float Range = 0.f;      // nothing further away is noticed (Unreal units, measured flat)
	float Cone = 120.f;     // sight cone in degrees, centred on the facing
	float Hear = 0.f;       // noticed inside this radius whichever way it faces (Unreal units)

	/** Range (Unreal units) plus a definition's "visionAngle" and "hearRadius" (data pixels), falling back to
	 *  tuning.<DefaultsKey> { coneAngle, hearRadius }. */
	static FTSSenses Read(const UObject* WorldContext, const TSJson::FObj& Def, float Range, const FString& DefaultsKey);
};

/**
 * Perception and stealth: who notices whom.
 *
 *   hidden      a character tagged "Hidden" (a smoke bomb...) can't be noticed; acting (attacking, shooting,
 *               casting, talking) reveals it
 *   noticing    inside the senses' range, in the sight cone or within hearing, with a clear line of sight
 *               (head to head, ECC_Visibility)
 *   threat      foes hunting a character (ATSCharacter::IsHunting): tuning.threatSense { range, revealAfterAttack }
 *
 * The game adds its own rules on top (territory, factions...).
 */
namespace TSPerception
{
	TESSERAGAMEPLAY_API bool IsHidden(const ATSCharacter* C);
	TESSERAGAMEPLAY_API void Hide(ATSCharacter* C, float Duration);
	TESSERAGAMEPLAY_API void Reveal(ATSCharacter* C);

	/** Viewer notices Target: Target alive and not hidden, within Dist2D < Range, in the cone or within hearing,
	 *  and in sight. */
	TESSERAGAMEPLAY_API bool CanNotice(const ATSCharacter* Viewer, const ATSCharacter* Target, const FTSSenses& Senses);
	TESSERAGAMEPLAY_API bool HasLineOfSight(const ATSCharacter* From, const ATSCharacter* To);

	/** tuning.threatSense.range (Unreal units). */
	TESSERAGAMEPLAY_API float ThreatRange(const UObject* WorldContext);
	/** tuning.threatSense.revealAfterAttack: seconds a foe stays shown after it attacks. */
	TESSERAGAMEPLAY_API float RevealAfterAttack(const UObject* WorldContext);
	/** Opponents hunting Of within MaxDistance (3D). */
	TESSERAGAMEPLAY_API TArray<ATSCharacter*> Hunters(const ATSCharacter* Of, float MaxDistance);
	/** The closest opponent hunting Of, within MaxDistance (flat), or null. */
	TESSERAGAMEPLAY_API ATSCharacter* NearestHunter(const ATSCharacter* Of, float MaxDistance);
}
