#include "TSSleep.h"
#include "TSCharacter.h"
#include "TSDayNight.h"
#include "TSFeedback.h"
#include "TSData.h"

#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName AsleepTag(TEXT("Asleep"));

	float SleepTuning(const UObject* WorldContext, const TCHAR* Key, double Default)
	{
		const UTSData& D = UTSData::Get(WorldContext);
		return float(TSJson::Num(TSJson::Obj(D.Section(TEXT("tuning")), TEXT("sleep")), Key, Default));
	}
}

UTSSleep::UTSSleep()
{
	PrimaryComponentTick.bCanEverTick = true;
}

ETSSleepHours UTSSleep::Parse(const FString& S)
{
	return S == TEXT("night") ? ETSSleepHours::Night : S == TEXT("day") ? ETSSleepHours::Day : ETSSleepHours::Never;
}

UTSSleep* UTSSleep::Add(ATSCharacter* Owner, ETSSleepHours Hours, const FVector& Bed, bool bHasBed)
{
	if (!Owner || Hours == ETSSleepHours::Never) return nullptr;
	UTSSleep* S = NewObject<UTSSleep>(Owner, TEXT("Sleep"));
	S->Hours = Hours;
	S->Bed = Bed;
	S->bHasBed = bHasBed;
	S->RegisterComponent();
	return S;
}

FTSSleepEvent& UTSSleep::OnAnyFellAsleep() { static FTSSleepEvent E; return E; }
FTSSleepEvent& UTSSleep::OnAnyWoke() { static FTSSleepEvent E; return E; }

ATSCharacter* UTSSleep::Char() const { return Cast<ATSCharacter>(GetOwner()); }

UTSSleep* UTSSleep::Of(const AActor* A) { return A ? A->FindComponentByClass<UTSSleep>() : nullptr; }

bool UTSSleep::IsAsleep(const ATSCharacter* C) { return C && C->Tags.Has(AsleepTag); }

bool UTSSleep::IsAsleep() const { return IsAsleep(Char()); }

bool UTSSleep::IsBedtime() const
{
	const UTSDayNight* DN = UTSDayNight::Get(this);
	if (!DN || Hours == ETSSleepHours::Never) return false;
	const ETSDayPhase P = DN->Phase();
	// Night sleepers turn in as it darkens and get up as it lightens; day sleepers the other way round.
	return Hours == ETSSleepHours::Night ? (P == ETSDayPhase::Dusk || P == ETSDayPhase::Night) : (P == ETSDayPhase::Dawn || P == ETSDayPhase::Day);
}

void UTSSleep::FallAsleep(bool bSnap)
{
	ATSCharacter* C = Char();
	if (!C || C->IsDead() || IsAsleep()) return;
	if (bHasEntry && !bIn) StepIn();
	else if (bSnap && bHasBed && !bHasEntry && FVector::Dist2D(Bed, C->GetActorLocation()) > BedReach)
		C->TeleportTo(Bed + FVector(0, 0, C->GetSimpleCollisionHalfHeight() + 10.f), C->GetActorRotation(), false, true);
	bTurningIn = false;
	C->Tags.Add(AsleepTag);
	if (bHasBedYaw) C->SetActorRotation(FRotator(0, BedYaw, 0));
	C->GetCharacterMovement()->StopMovementImmediately();
	bTurningIn = false;
	Path.Reset();
	ZzzIn = FMath::FRandRange(0.3f, 1.5f);
	OnFellAsleep.Broadcast(C);
	OnAnyFellAsleep().Broadcast(C);
}

void UTSSleep::Wake(float StayUp)
{
	ATSCharacter* C = Char();
	AwakeFor = StayUp >= 0.f ? StayUp : SleepTuning(this, TEXT("wakeFor"), 30);
	if (!C || !IsAsleep()) return;
	C->Tags.Remove(AsleepTag);
	OnWoke.Broadcast(C);
	OnAnyWoke().Broadcast(C);
}

void UTSSleep::StepIn()
{
	// Behind a door: out of the world's way (no collision, no walking) until it comes out again.
	ATSCharacter* C = Char();
	bIn = true;
	C->GetCharacterMovement()->DisableMovement();
	C->SetActorEnableCollision(false);
	C->SetActorLocation(Bed + FVector(0, 0, C->GetSimpleCollisionHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
}

void UTSSleep::ClearEntry()
{
	bHasEntry = false;
	Path.Reset();
	if (!bIn) return;
	bIn = false;
	if (ATSCharacter* C = Char())
	{
		C->SetActorEnableCollision(true);
		C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void UTSSleep::StepOut()
{
	ATSCharacter* C = Char();
	bIn = false;
	// Out at the entry, a little to one side (TeleportTo finds a free spot: never inside someone already there).
	const float A = FMath::FRandRange(0.f, UE_TWO_PI), R = FMath::FRandRange(40.f, 140.f);
	C->SetActorEnableCollision(true);
	if (!C->TeleportTo(Entry + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, C->GetSimpleCollisionHalfHeight() + 10.f), C->GetActorRotation(), false, false))
		C->SetActorLocation(Entry + FVector(0, 0, C->GetSimpleCollisionHalfHeight() + 10.f), false, nullptr, ETeleportType::TeleportPhysics);
	C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void UTSSleep::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	ATSCharacter* C = Char();
	if (!C || C->IsDead()) return;
	HeldFor -= Dt;
	AwakeFor -= Dt;
	// The first tick the clock is known: anyone whose hours these are starts the game already in bed.
	if (!bSettled)
	{
		const UTSDayNight* DN = UTSDayNight::Get(this);
		if (!DN || DN->Hour() < 0) return;
		bSettled = true;
		if (IsBedtime()) { FallAsleep(); return; }
	}
	const bool bBedtime = IsBedtime();

	if (IsAsleep())
	{
		if (!bBedtime)
		{
			// Morning (or nightfall, for the day sleepers): up, and out of the door if there is one. Not all at once:
			// each in its own moment (a crowd stepping out on one spot would jam in the doorway).
			if (UpIn < 0.f) UpIn = FMath::FRandRange(0.f, SleepTuning(this, TEXT("staggerUp"), 6));
			UpIn -= Dt;
			if (UpIn > 0.f) return;
			UpIn = -1.f;
			C->Tags.Remove(AsleepTag);
			if (bIn) StepOut();
			OnWoke.Broadcast(C);
			OnAnyWoke().Broadcast(C);
			return;
		}
		ZzzIn -= Dt;
		if (ZzzIn <= 0.f)
		{
			ZzzIn = FMath::FRandRange(1.6f, 2.6f);
			UTSFeedback::Get(this)->Float(C->Head() + FVector(FMath::FRandRange(-15.f, 15.f), 0, 10.f), TEXT("z"), FLinearColor(0.7f, 0.8f, 1.f), 0.8f);
		}
		return;
	}

	if (bIn && !bBedtime) { StepOut(); return; }   // woken in bed, and now it's morning anyway
	if (!bBedtime || AwakeFor > 0.f || HeldFor > 0.f) { bTurningIn = false; return; }
	if (bIn) { FallAsleep(); return; }             // woken in bed, dozed off again

	if (!bTurningIn) { Closest = BIG_NUMBER; NoProgress = 0.f; }
	bTurningIn = true;
	const FVector Goal = bHasEntry ? Entry : bHasBed ? Bed : C->GetActorLocation();
	const float Dist = FVector::Dist2D(Goal, C->GetActorLocation());
	// There, or stuck short of it (someone in the way, the bed just off the navmesh): lie down (FallAsleep puts it in bed).
	if (Dist < Closest - 20.f) { Closest = Dist; NoProgress = 0.f; }
	else NoProgress += Dt;
	// Short of an open-air bed: lie down right there (no visible hop); a door it still has to reach, it goes through.
	if (Dist <= BedReach) FallAsleep();
	else if (NoProgress > 2.5f)
	{
		// Stuck on the way: a door it still has to reach, it goes through. An open-air bed nobody's near enough to watch:
		// it's simply in bed. Watched: keep sidestepping and trying for a while, then lie down where it stands.
		const APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0);
		const bool bWatched = Hero && FVector::Dist2D(Hero->GetActorLocation(), C->GetActorLocation()) < SleepTuning(this, TEXT("snapUnwatched"), 2500);
		if (bHasEntry || !bWatched) FallAsleep(/*bSnap*/ !bWatched);
		else if (NoProgress > SleepTuning(this, TEXT("giveUpAfter"), 8)) FallAsleep(/*bSnap*/ false);
	}
	// Not getting nearer: step aside a moment (round a corner, out of a queue), then on again.
	if (NoProgress > 1.f && SidestepFor <= 0.f && !IsAsleep())
	{
		const FVector Ahead = (Goal - C->GetActorLocation()).GetSafeNormal2D();
		Sidestep = FVector(-Ahead.Y, Ahead.X, 0.f) * (FMath::RandBool() ? 1.f : -1.f) - Ahead * 0.3f;
		SidestepFor = 0.6f;
		RepathIn = 0.f;
	}
}

void UTSSleep::Reset()
{
	ATSCharacter* C = Char();
	if (!C) return;
	C->Tags.Remove(AsleepTag);
	if (bIn) { bIn = false; C->SetActorEnableCollision(true); C->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
	bTurningIn = false;
	AwakeFor = HeldFor = 0.f;
	Path.Reset();
}

void UTSSleep::Repath(const FVector& To)
{
	const FVector From = GetOwner()->GetActorLocation();
	PathGoal = To;
	Path.Reset();
	PathIndex = 1;
	RepathIn = 1.5f;
	if (const UNavigationPath* P = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, To, GetOwner()))
		if (P->IsValid() && P->PathPoints.Num() >= 2) { Path = P->PathPoints; return; }
	Path = { From, To };
}

FVector UTSSleep::Direction(float Dt)
{
	if (!bTurningIn) return FVector::ZeroVector;
	if (SidestepFor > 0.f) { SidestepFor -= Dt; return Sidestep.GetSafeNormal2D(); }
	return DirectionTo(bHasEntry ? Entry : bHasBed ? Bed : GetOwner()->GetActorLocation(), Dt);
}

FVector UTSSleep::DirectionTo(const FVector& Goal, float Dt)
{
	const FVector At = GetOwner()->GetActorLocation();
	if (FVector::Dist2D(Goal, At) <= BedReach) return FVector::ZeroVector;
	RepathIn -= Dt;
	if (Path.IsEmpty() || RepathIn <= 0.f || !PathGoal.Equals(Goal, 1.f)) Repath(Goal);
	while (PathIndex < Path.Num() - 1 && FVector::Dist2D(Path[PathIndex], At) < 60.f) ++PathIndex;
	return (Path[FMath::Min(PathIndex, Path.Num() - 1)] - At).GetSafeNormal2D();
}
