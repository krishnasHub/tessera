#include "TSRoutine.h"
#include "TSSky.h"

#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "GameFramework/Actor.h"

UTSRoutine::UTSRoutine()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTSRoutine::AddStop(const FVector& At, float Wait, FName When, FName Tag)
{
	FStop S;
	S.At = At;
	S.Wait = Wait;
	S.When = When;
	S.Tag = Tag;
	Stops.Add(S);
}

bool UTSRoutine::Fits(const FStop& S) const
{
	const float Night = ATSSky::Night();
	return S.When == TEXT("day") ? Night < 0.5f : S.When == TEXT("night") ? Night >= 0.5f : true;
}

int32 UTSRoutine::NextStop(int32 From) const
{
	for (int32 I = 1; I <= Stops.Num(); ++I)
	{
		const int32 N = (From + I) % Stops.Num();
		if (Fits(Stops[N])) return N;
	}
	return From;   // nothing fits the hour: stay put
}

void UTSRoutine::Begin()
{
	if (Stops.IsEmpty()) return;
	const FVector At = GetOwner()->GetActorLocation();
	Index = 0;
	for (int32 I = 1; I < Stops.Num(); ++I)
		if (FVector::Dist2D(Stops[I].At, At) < FVector::Dist2D(Stops[Index].At, At)) Index = I;
	bRunning = true;
	bArrived = false;
	WaitLeft = 0.f;
	Path.Reset();
	OnDepart.Broadcast(Index, Stops[Index].Tag);
}

void UTSRoutine::Repath(const FVector& To)
{
	const FVector From = GetOwner()->GetActorLocation();
	PathGoal = To;
	Path.Reset();
	PathIndex = 1;
	RepathIn = 1.f;
	if (const UNavigationPath* P = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, To, GetOwner()))
		if (P->IsValid() && P->PathPoints.Num() >= 2) { Path = P->PathPoints; return; }
	Path = { From, To };
}

FVector UTSRoutine::Direction(float Dt)
{
	if (!bRunning || Stops.IsEmpty()) return FVector::ZeroVector;
	const FVector At = GetOwner()->GetActorLocation();

	if (bArrived)
	{
		// Waiting at a stop; the hour can also send it off early (a day stop at nightfall).
		WaitLeft -= Dt;
		if (WaitLeft > 0.f && Fits(Stops[Index])) return FVector::ZeroVector;
		const int32 Next = NextStop(Index);
		if (Next == Index) { WaitLeft = 1.f; return FVector::ZeroVector; }
		Index = Next;
		bArrived = false;
		WaitLeft = 0.f;
		Path.Reset();
		OnDepart.Broadcast(Index, Stops[Index].Tag);
	}

	// The stop is through a door: head for the door, and go through.
	FVector Goal = Stops[Index].At, Door;
	if (FindDoor && FindDoor(At, Goal, Door))
	{
		if (FVector::Dist2D(Door, At) <= ArriveDistance)
		{
			Path.Reset();
			if (ThroughDoor) ThroughDoor(Door);
			return FVector::ZeroVector;
		}
		Goal = Door;
	}
	else if (FVector::Dist2D(Goal, At) <= ArriveDistance)
	{
		bArrived = true;
		WaitLeft = Stops[Index].Wait;
		Path.Reset();
		OnArrive.Broadcast(Index, Stops[Index].Tag);
		return FVector::ZeroVector;
	}
	RepathIn -= Dt;
	if (Path.IsEmpty() || RepathIn <= 0.f || !PathGoal.Equals(Goal, 1.f)) Repath(Goal);
	while (PathIndex < Path.Num() - 1 && FVector::Dist2D(Path[PathIndex], At) < 60.f) ++PathIndex;
	return (Path[FMath::Min(PathIndex, Path.Num() - 1)] - At).GetSafeNormal2D();
}
