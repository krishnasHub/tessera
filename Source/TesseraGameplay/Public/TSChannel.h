#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TSChannel.generated.h"

/**
 * Something that takes a moment of work while the character stays put: picking a lock, lifting a purse, reading a
 * tablet. Start it with a label and a duration; it finishes after Duration seconds unless the character moves away
 * (MoveTolerance) or Keep says no (seen, out of reach...), and then Done or Broken is called. The HUD reads Label and
 * Progress to draw a ring or bar.
 *
 * A doomed attempt (FailAt 0..1, e.g. a lock beyond the hero's skill): it fills to FailAt and fails there (Broken is
 * called); for a moment afterwards IsSnapped() / SnappedAt() / SnapAge() let the HUD show the bar breaking.
 */
UCLASS(ClassGroup = (Tessera), meta = (BlueprintSpawnableComponent))
class TESSERAGAMEPLAY_API UTSChannel : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSChannel();

	/** Begin (replacing any channel under way, which breaks). False if Duration <= 0. */
	bool Start(const FString& InLabel, float Duration, TFunction<bool()> InKeep, TFunction<void()> InDone, TFunction<void()> InBroken);
	/** Stop it now (calls Broken). */
	void Cancel();
	/** Work done on someone who keeps moving (lifting a walking mark's purse): it holds as long as the owner stays within
	 *  Slack of them, wherever they go (instead of the owner staying put). Call right after Start. */
	void FollowActor(AActor* Who, float Slack) { Followed = Who; FollowSlack = Slack; }
	AActor* GetFollowed() const { return bActive ? Followed.Get() : nullptr; }
	/** Fail when Progress reaches At (0..1); call right after Start. */
	void FailAt(float At) { FailPoint = At; }
	/** The last attempt failed at its FailAt point, SnapAge() seconds ago (the HUD shows it breaking for a moment). */
	bool IsSnapped() const { return SnapClock >= 0.f && SnapClock < SnapShow; }
	float SnappedAt() const { return SnapProgress; }
	float SnapAge() const { return SnapClock; }
	FString SnapLabel;
	float SnapShow = 1.1f;
	bool IsActive() const { return bActive; }
	/** 0..1 */
	float Progress() const { return bActive && Length > 0.f ? FMath::Clamp(Elapsed / Length, 0.f, 1.f) : 0.f; }
	FString Label;
	/** How far (uu, flat) the owner may drift before it breaks. */
	float MoveTolerance = 60.f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Finish(bool bDone);
	bool bActive = false;
	float Elapsed = 0.f, Length = 0.f;
	float FailPoint = -1.f, SnapClock = -1.f, SnapProgress = 0.f;
	FVector StartAt = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Followed;
	float FollowSlack = 0.f;
	TFunction<bool()> Keep;
	TFunction<void()> Done, Broken;
};
