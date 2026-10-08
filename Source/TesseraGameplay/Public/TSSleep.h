#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TSSleep.generated.h"

class ATSCharacter;

/** When a character sleeps. */
UENUM()
enum class ETSSleepHours : uint8 { Never, Night, Day };

DECLARE_MULTICAST_DELEGATE_OneParam(FTSSleepEvent, ATSCharacter* /*Who*/);

/**
 * Keeping hours: a character that sleeps by night (villagers, a bandit camp) or by day (the undead, a cave brute).
 *
 * The hours come from Tessera's day/night events (UTSDayNight): night sleepers turn in at dusk and get up at dawn,
 * day sleepers the other way round. At bedtime the character walks to its rest place (the navmesh; none = where it
 * stands), lies down and is tagged "Asleep". A bed behind a door (a house) has an entry: it walks to the entry and
 * steps through to the bed (its collision off while it's in there), and comes back out through it when it gets up.
 *
 *   asleep     TSPerception: no sight cone, hearing x tuning.sleep.hearMul; TSCombat: hits x tuning.sleep.hitMul,
 *              then it wakes; the sprite lies down; a "z" drifts up now and then
 *   woken      Wake(): up for tuning.sleep.wakeFor seconds (a hit, a noise, someone talking to it), then back to bed
 *              if it's still its hours
 *
 * The component doesn't move anything: the owner's tick asks Direction() and walks that way, and skips its own AI
 * while IsAsleep(). The game reads its own data (e.g. "sleeps": "night" | "day", a bed, a house) and calls Add.
 */
UCLASS(ClassGroup = (Tessera), meta = (BlueprintSpawnableComponent))
class TESSERAGAMEPLAY_API UTSSleep : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSSleep();

	static ETSSleepHours Parse(const FString& S);
	/** Give the owner its hours; bHasBed false = sleeps where it stands. */
	static UTSSleep* Add(ATSCharacter* Owner, ETSSleepHours Hours, const FVector& Bed, bool bHasBed);
	/** The bed is behind a door: walk to Entry, then step through to the bed. */
	void SetEntry(const FVector& InEntry) { Entry = InEntry; bHasEntry = true; }

	ETSSleepHours Hours = ETSSleepHours::Never;
	FVector Bed = FVector::ZeroVector;
	bool bHasBed = false;
	FVector Entry = FVector::ZeroVector;
	bool bHasEntry = false;
	/** In its bed behind the door right now. */
	bool IsIndoors() const { return bIn; }
	/** Which way it lies (yaw); default: as it stood. */
	float BedYaw = 0.f;
	bool bHasBedYaw = false;
	/** How close to the bed counts as there. */
	float BedReach = 70.f;

	/** Its hours right now (the time of day says it should be in bed). */
	bool IsBedtime() const;
	bool IsAsleep() const;
	/** Bedtime and awake: walking to bed (follow Direction()). */
	bool IsTurningIn() const { return bTurningIn; }
	/** The way to its bed (zero: there, or not bedtime). */
	FVector Direction(float Dt);

	/** Woken (a hit, a noise, a word): up for tuning.sleep.wakeFor seconds, or StayUp if given. */
	void Wake(float StayUp = -1.f);
	/** Lie down now: in its bed (through the door, if there is one; bSnap: straight into a bed out in the open, or else
	 *  right where it stands). */
	void FallAsleep(bool bSnap = true);
	/** Start over (respawned, sent home): out of bed and awake; if it's its hours it walks to bed again. */
	void Reset();
	/** The way to Goal along the navmesh (zero: there). For the owner's own walks (home again in the morning). */
	FVector DirectionTo(const FVector& Goal, float Dt);
	/** Don't sleep for now (busy: hunting, talking, carrying...). The owner sets it each tick it applies. */
	void Hold() { HeldFor = 0.25f; }

	static bool IsAsleep(const ATSCharacter* C);
	static UTSSleep* Of(const AActor* A);

	FTSSleepEvent OnFellAsleep, OnWoke;
	/** Anyone, anywhere: fell asleep / woke (the game can listen once for everyone). */
	static FTSSleepEvent& OnAnyFellAsleep();
	static FTSSleepEvent& OnAnyWoke();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	ATSCharacter* Char() const;
	void Repath(const FVector& To);
	void StepIn();
	void StepOut();

	bool bTurningIn = false, bIn = false, bSettled = false;
	FVector PathGoal = FVector(BIG_NUMBER);
	float AwakeFor = 0.f, HeldFor = 0.f, ZzzIn = 0.f, RepathIn = 0.f;
	float Closest = BIG_NUMBER, NoProgress = 0.f;   // walking to bed: the nearest it got, and for how long it hasn't got nearer
	TArray<FVector> Path;
	int32 PathIndex = 0;
};
