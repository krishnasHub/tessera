#include "TSChannel.h"
#include "GameFramework/Actor.h"

UTSChannel::UTSChannel()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UTSChannel::Start(const FString& InLabel, float Duration, TFunction<bool()> InKeep, TFunction<void()> InDone, TFunction<void()> InBroken)
{
	if (bActive) Cancel();
	if (Duration <= 0.f) return false;
	Label = InLabel;
	Length = Duration;
	Elapsed = 0.f;
	FailPoint = -1.f;
	Followed = nullptr;
	StartAt = GetOwner()->GetActorLocation();
	Keep = MoveTemp(InKeep);
	Done = MoveTemp(InDone);
	Broken = MoveTemp(InBroken);
	bActive = true;
	return true;
}

void UTSChannel::Cancel()
{
	if (bActive) Finish(false);
}

void UTSChannel::Finish(bool bDone)
{
	bActive = false;
	// Copy out first: the callback may start another channel.
	TFunction<void()> Call = bDone ? MoveTemp(Done) : MoveTemp(Broken);
	Keep = nullptr; Done = nullptr; Broken = nullptr;
	if (Call) Call();
}

void UTSChannel::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	if (SnapClock >= 0.f) SnapClock += Dt;
	if (!bActive) return;
	// Stay put, or (following someone) stay with them.
	const bool bAstray = Followed.IsValid() ? FVector::Dist2D(GetOwner()->GetActorLocation(), Followed->GetActorLocation()) > FollowSlack
		: FVector::Dist2D(GetOwner()->GetActorLocation(), StartAt) > MoveTolerance;
	if (bAstray || (Keep && !Keep())) { Finish(false); return; }
	Elapsed += Dt;
	if (FailPoint >= 0.f && Progress() >= FailPoint)
	{
		// It gives way here: the attempt snaps (the HUD shows the bar breaking for a moment).
		SnapProgress = FailPoint;
		SnapLabel = Label;
		SnapClock = 0.f;
		Finish(false);
		return;
	}
	if (Elapsed >= Length) Finish(true);
}
