#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "TSHeroControl.generated.h"

class ATSCharacter;
class ATSInteractable;
class APlayerController;

/** What a top-down click asked for. */
UENUM()
enum class ETSClickGoal : uint8 { None, Move, Attack, Talk, Use };

/**
 * Diablo-style mouse control for the hero (a component on an ATSCharacter the player possesses):
 *
 *   cursor        the ray under the mouse, the ground there, the character under it (generous: anywhere on the
 *                 body or its sprite card), aim assist toward it
 *   click         LMB on the ground walks there along the navmesh (hold: follow the cursor); on a foe walks into
 *                 range and attacks (hold: keep attacking); on someone to talk to walks up and talks; on a thing
 *                 (ATSInteractable) walks up and uses it.
 *                 Modifier + LMB (Shift) attacks in place
 *   talk mode     the next click on a character talks instead of attacking (cursor shows it)
 *   picker        Modifier + wheel opens the ability picker: slow motion (<world>.camera.abilityPicker.timeScale),
 *                 the wheel cycles unlocked abilities, releasing the modifier (or a click) casts
 *
 * The component doesn't bind keys or tick itself: the game routes its input here and calls Update() from the
 * hero's tick, then adds the returned direction as movement. Game rules come in through the hooks below.
 * Words: <world>.text.noAbilities.
 */
UCLASS()
class TESSERAHERO_API UTSHeroControl : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSHeroControl();

	// ---- game hooks ----
	/** Close enough for the primary attack to reach Target (melee reach, or range with line of sight). */
	TFunction<bool(const ATSCharacter* Target)> InAttackRange;
	/** Mid-attack: finish it before chasing again. */
	TFunction<bool()> IsAttacking;
	/** The primary attack, in place. */
	TFunction<void()> Attack;
	/** Worth picking out with the cursor even with nothing to say and not a foe (e.g. pockets to pick). */
	TFunction<bool(const ATSCharacter* Who)> Interesting;
	/** Why the hero can't talk to Who right now ("" = they can). */
	TFunction<FString(const ATSCharacter* Who)> TalkBlocker;
	/** Open the conversation with Who (they're in range and willing). */
	TFunction<void(ATSCharacter* Who)> Talk;
	/** Why the hero can't use It right now ("" = they can). */
	TFunction<FString(const ATSInteractable* It)> UseBlocker;
	/** Use It (the hero is beside it). */
	TFunction<void(ATSInteractable* It)> Use;
	/** How close the hero walks before talking (from the other's edge). */
	float TalkRange = 200.f;
	/** How close to walk up to Who before Talk fires (from its edge), when the game wants it closer than TalkRange
	 *  (e.g. close enough to lift a purse). Unset or negative: TalkRange. */
	TFunction<float(const ATSCharacter* Who)> ApproachRange;
	/** Held with LMB: attack in place; with the wheel: the ability picker. */
	FKey ModifierKey = EKeys::LeftShift;

	// ---- input state ----
	/** Ignore the real keyboard and mouse (automated runs). */
	bool bInputLocked = false;
	/** Inside a scripted press: never read the real cursor. */
	bool bScripted = false;
	/** The primary button is held (keep attacking). */
	bool bAttackHeld = false;
	bool IsModifierDown() const;
	/** Drop held buttons and modes (a UI opened, so the game won't see their release). */
	void ClearHeldInput() { bAttackHeld = false; ClearGoal(); ClosePicker(false); SetTalkMode(false); }

	// ---- cursor ----
	/** The ray under the mouse cursor. False when not top-down, in automated runs, or with no cursor. */
	bool CursorRay(FVector& Origin, FVector& Dir) const;
	/** The ground (anything blocking Visibility) under the cursor. */
	bool CursorGround(FVector& Out) const;
	/** Where the cursor ray crosses height Z. */
	bool CursorAtHeight(float Z, FVector& Out) const;
	/** The character under the cursor (a foe, or someone with dialogue); bHostile: a click would attack them. */
	ATSCharacter* UnderCursor(bool& bHostile) const;
	/** The usable thing (ATSInteractable) under the cursor. */
	ATSInteractable* ObjectUnderCursor() const;
	/** Aim assist: the opponent nearest the cursor ray (within Slack of its body), in sight, within MaxDistance. */
	const ATSCharacter* CursorAssist(float MaxDistance = 3000.f, float Slack = 70.f) const;

	// ---- click-to-move ----
	/** LMB pressed: the picker casts, or (top-down, no modifier) a click walks / fights / talks. False: attack in place. */
	bool HandlePrimaryPress();
	void OnPrimaryReleased() { bAttackHeld = false; bMoveHeld = false; }
	/** A click on On (or, with none, on the ground at Ground). */
	void Click(ATSCharacter* On, bool bHostile, const FVector& Ground);
	void ClearGoal();
	ETSClickGoal GetGoal() const { return Goal; }
	/** Who a click-to-attack / talk is going for. */
	ATSCharacter* GoalTarget() const { return GoalActor.Get(); }
	/** Where a click-to-move is heading (for the HUD marker). */
	bool ClickDestination(FVector& Out) const;
	/** The current path (navmesh corners). */
	const TArray<FVector>& GetPath() const { return Path; }
	/** The next path corner while following a click. */
	bool NextCorner(FVector& Out) const;
	/** Steps the click goal: arrives, attacks or talks when in range; returns the direction to walk (zero to stand). */
	FVector Update(float Dt);
	/** Self-test hook: a single click on On (attack or talk) or, with none, on the ground at Point. */
	void TestClick(const FVector& Point, ATSCharacter* On = nullptr);

	// ---- talk mode ----
	bool IsTalkMode() const { return bTalkMode; }
	void SetTalkMode(bool bOn) { bTalkMode = bOn; }
	/** Walk up to C and talk, or float why not. */
	void TryTalk(ATSCharacter* C);
	/** Walk up to It and use it, or float why not. */
	void TryUse(ATSInteractable* It);
	/** Who / what a click is going for (an interactable). */
	ATSInteractable* GoalObject() const { return GoalObj.Get(); }

	// ---- ability picker ----
	/** The highlighted slot, -1 when closed. */
	int32 PickerSlot() const { return Picker; }
	/** Modifier + wheel opens / cycles the picker (true); a plain wheel is left to the camera (false). */
	bool HandleWheel(float Wheel);
	void OpenPicker();
	void CyclePicker(int32 Step);
	/** Close it, casting the highlighted ability (bCast) or not. */
	void ClosePicker(bool bCast);
	/** RMB / Esc: back out of the picker or talk mode first. True if something was cancelled. */
	bool CancelModes();
	/** Self-test hook: open the picker, move it Steps slots, then cast (unless bOpenOnly). */
	void TestPicker(int32 Steps, bool bOpenOnly);

protected:
	virtual void BeginPlay() override;

private:
	ATSCharacter* Hero() const;
	APlayerController* PC() const;
	void Repath(const FVector& To);

	bool bTopDown = false;
	ETSClickGoal Goal = ETSClickGoal::None;
	FVector GoalPoint = FVector::ZeroVector;
	TWeakObjectPtr<ATSCharacter> GoalActor;
	TWeakObjectPtr<ATSInteractable> GoalObj;
	bool bMoveHeld = false;          // LMB held after a ground click: keep walking toward the cursor
	TArray<FVector> Path;
	int32 PathIndex = 0;
	float RepathIn = 0.f;
	bool bTalkMode = false;
	int32 Picker = -1, LastPicked = 0;
};
