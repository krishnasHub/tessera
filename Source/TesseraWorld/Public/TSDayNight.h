#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TSDayNight.generated.h"

/** Where the day is: by Night() level, rising (dusk) or falling (dawn). */
UENUM()
enum class ETSDayPhase : uint8 { Day, Dusk, Night, Dawn };

DECLARE_MULTICAST_DELEGATE_OneParam(FTSDayPhaseEvent, ETSDayPhase /*Phase*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FTSHourEvent, int32 /*Hour*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FTSNightLevelEvent, float /*Night 0-1*/);

/**
 * The day/night cycle as events, for the game to act on (ATSSky drives it). Tessera only announces; what a game
 * does with nightfall (lamps, a glowing staff, shops closing, guards changing) is the game's own code.
 *
 *   OnPhase       Day -> Dusk -> Night -> Dawn -> Day
 *   OnHour        each whole hour
 *   OnNightLevel  how dark it is, 0 (day) to 1 (night), whenever it moves (smooth enough to fade things with)
 *
 * A listener that starts late reads the current state from Phase() / Hour() / NightLevel() (or ATSSky).
 */
UCLASS()
class TESSERAWORLD_API UTSDayNight : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTSDayNight* Get(const UObject* WorldContext);
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

	FTSDayPhaseEvent OnPhase;
	FTSHourEvent OnHour;
	FTSNightLevelEvent OnNightLevel;

	ETSDayPhase Phase() const { return CurPhase; }
	int32 Hour() const { return CurHour; }
	float NightLevel() const { return Night; }

	/** ATSSky, whenever the sky moves. */
	void Update(float InHour, float InNight);

private:
	ETSDayPhase CurPhase = ETSDayPhase::Day;
	int32 CurHour = -1;
	float Night = -1.f;
};
