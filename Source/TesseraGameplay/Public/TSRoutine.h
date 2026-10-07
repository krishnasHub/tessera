#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TSJson.h"
#include "TSRoutine.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FTSRoutineEvent, int32 /*Stop*/, FName /*Tag*/);

/**
 * A daily routine for a character: walk from stop to stop along the navmesh, wait a while at each, round and round.
 * Stops can be for the day or the night (the sky's night level decides); off-hours the character heads for the first
 * stop that fits the hour, and waits there (a miner sleeps at the mine).
 *
 * The component doesn't move anything: the owner's tick asks Direction() and walks that way (so its own AI, speed and
 * animation stay in charge), and listens to OnArrive / OnDepart (e.g. show a sack while carrying it).
 *
 * Data (Load): { "stops": [ { "at": [x, y] (world uu; the game converts tiles), "wait": 5, "when": "day" | "night" |
 *   "any", "tag": "mine" } ] }
 */
UCLASS(ClassGroup = (Tessera), meta = (BlueprintSpawnableComponent))
class TESSERAGAMEPLAY_API UTSRoutine : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSRoutine();

	struct FStop { FVector At = FVector::ZeroVector; float Wait = 3.f; FName When = TEXT("any"); FName Tag; };
	TArray<FStop> Stops;

	void AddStop(const FVector& At, float Wait, FName When = TEXT("any"), FName Tag = NAME_None);
	/** Start (or restart) at the stop nearest the owner. */
	void Begin();
	void Stop() { bRunning = false; }
	bool IsRunning() const { return bRunning; }

	/** Steps the routine; returns the way to walk (zero: waiting, or there). */
	FVector Direction(float Dt);
	int32 Current() const { return Index; }
	bool IsWaiting() const { return WaitLeft > 0.f; }
	FName CurrentTag() const { return Stops.IsValidIndex(Index) ? Stops[Index].Tag : NAME_None; }

	/** Arrived at a stop / set off for the next one. */
	FTSRoutineEvent OnArrive, OnDepart;
	/** How close counts as there. */
	float ArriveDistance = 90.f;

	/** Stops in another area (a cave off the main map): the game names the door to walk to from here (true), and
	 *  takes the character through it when it gets there. */
	TFunction<bool(const FVector& From, const FVector& Goal, FVector& Door)> FindDoor;
	TFunction<void(const FVector& Door)> ThroughDoor;

private:
	bool Fits(const FStop& S) const;
	int32 NextStop(int32 From) const;
	void Repath(const FVector& To);
	FVector PathGoal = FVector(BIG_NUMBER);

	bool bRunning = false, bArrived = false;
	int32 Index = 0;
	float WaitLeft = 0.f, RepathIn = 0.f;
	TArray<FVector> Path;
	int32 PathIndex = 0;
};
