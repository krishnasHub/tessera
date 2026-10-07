#include "TSHeroControl.h"
#include "TSInteractable.h"
#include "TSCameraRig.h"
#include "TSCharacter.h"
#include "TSCombat.h"
#include "TSAbilities.h"
#include "TSFeedback.h"
#include "TSData.h"
#include "TSLook.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "EngineUtils.h"

UTSHeroControl::UTSHeroControl()
{
	PrimaryComponentTick.bCanEverTick = false;   // the hero calls Update() from its own tick
}

void UTSHeroControl::BeginPlay()
{
	Super::BeginPlay();
	bTopDown = UTSCameraRig::IsTopDown(this);
}

ATSCharacter* UTSHeroControl::Hero() const { return Cast<ATSCharacter>(GetOwner()); }

APlayerController* UTSHeroControl::PC() const
{
	const ATSCharacter* H = Hero();
	return H ? Cast<APlayerController>(H->GetController()) : nullptr;
}

bool UTSHeroControl::IsModifierDown() const
{
	const APlayerController* P = PC();
	return P && P->IsInputKeyDown(ModifierKey);
}

// ---------------------------------------------------------------------------------------------
// Cursor
// ---------------------------------------------------------------------------------------------

bool UTSHeroControl::CursorRay(FVector& Origin, FVector& Dir) const
{
	const APlayerController* P = PC();
	return bTopDown && !bInputLocked && !bScripted && P && P->DeprojectMousePositionToWorld(Origin, Dir);
}

bool UTSHeroControl::CursorGround(FVector& Out) const
{
	FVector O, R;
	if (!CursorRay(O, R)) return false;
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CursorGround), false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(H, O, O + R * 30000.f, ECC_Visibility, Q)) { Out = H.ImpactPoint; return true; }
	if (R.Z >= -0.01f) return false;
	Out = O + R * ((GetOwner()->GetActorLocation().Z - O.Z) / R.Z);
	return true;
}

bool UTSHeroControl::CursorAtHeight(float Z, FVector& Out) const
{
	FVector O, R;
	if (!CursorRay(O, R) || R.Z >= -0.01f) return false;
	Out = O + R * ((Z - O.Z) / R.Z);
	return true;
}

ATSCharacter* UTSHeroControl::UnderCursor(bool& bHostile) const
{
	bHostile = false;
	FVector O, R;
	const ATSCharacter* Me = Hero();
	if (!Me || !CursorRay(O, R)) return nullptr;
	const TArray<ATSCharacter*> Foes = TSCombat::Opponents(Me);
	ATSCharacter* Best = nullptr;
	float BestMiss = 0.f;
	for (TActorIterator<ATSCharacter> It(GetWorld()); It; ++It)
	{
		ATSCharacter* C = *It;
		if (C == Me || C->IsDead() || C->IsLeaving()) continue;
		const bool bFoe = Foes.Contains(C);
		if (!bFoe && C->DialogueRoot.IsEmpty()) continue;
		// Generous: anywhere on the body, plus a little slack around it.
		float Miss = FMath::Min(FMath::PointDistToLine(C->Chest(), R, O), FMath::PointDistToLine(C->GetActorLocation() - FVector(0, 0, 40), R, O));
		if (TSLook::IsSprite())
		{
			// A sprite is drawn on a card standing at the feet: test points up the card, where its body is drawn.
			const FVector Up = TSLook::StandingUp() * TSLook::StandingStretch();
			const FVector Feet = C->GetActorLocation() - FVector(0, 0, C->GetSimpleCollisionHalfHeight());
			for (const float H : { 30.f, 90.f, 150.f })
				Miss = FMath::Min(Miss, FMath::PointDistToLine(Feet + Up * H * C->GetActorScale3D().Z, R, O));
		}
		Miss -= C->Radius();
		if (Miss > 45.f || (Best && Miss >= BestMiss)) continue;
		Best = C;
		BestMiss = Miss;
		bHostile = bFoe;
	}
	return Best;
}

ATSInteractable* UTSHeroControl::ObjectUnderCursor() const
{
	FVector O, R;
	if (!CursorRay(O, R)) return nullptr;
	ATSInteractable* Best = nullptr;
	float BestMiss = 30.f;
	for (TActorIterator<ATSInteractable> It(GetWorld()); It; ++It)
	{
		if (!It->CanUse()) continue;
		const float Miss = It->CursorMiss(O, R);
		if (Miss < BestMiss) { BestMiss = Miss; Best = *It; }
	}
	return Best;
}

const ATSCharacter* UTSHeroControl::CursorAssist(float MaxDistance, float Slack) const
{
	FVector O, R;
	const ATSCharacter* Me = Hero();
	if (!Me || !CursorRay(O, R)) return nullptr;
	const ATSCharacter* Best = nullptr;
	float BestMiss = 0.f;
	for (ATSCharacter* E : TSCombat::Opponents(Me))
	{
		if (FVector::Dist(E->Chest(), Me->Chest()) > MaxDistance) continue;
		const float Miss = FMath::PointDistToLine(E->Chest(), R, O) - E->Radius();
		if (Miss > Slack || (Best && Miss >= BestMiss)) continue;
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AimAssist), false, Me);
		Q.AddIgnoredActor(E);
		if (GetWorld()->LineTraceSingleByChannel(H, Me->Chest(), E->Chest(), ECC_Visibility, Q)) continue;
		BestMiss = Miss;
		Best = E;
	}
	return Best;
}

// ---------------------------------------------------------------------------------------------
// Click-to-move
// ---------------------------------------------------------------------------------------------

bool UTSHeroControl::HandlePrimaryPress()
{
	if (Picker >= 0) { ClosePicker(true); return true; }   // click while choosing: cast it now
	FVector O, R;
	if (!CursorRay(O, R) || IsModifierDown()) return false;
	bool bHostile = false;
	ATSCharacter* On = UnderCursor(bHostile);
	if (On && (bTalkMode || !bHostile)) { TryTalk(On); return true; }
	if (!On) if (ATSInteractable* It = ObjectUnderCursor()) { SetTalkMode(false); TryUse(It); return true; }
	SetTalkMode(false);
	FVector Ground = GetOwner()->GetActorLocation();
	CursorGround(Ground);
	Click(On, bHostile, Ground);
	return true;
}

void UTSHeroControl::TestClick(const FVector& Point, ATSCharacter* On)
{
	const bool bHostile = On && TSCombat::Opponents(Hero()).Contains(On);
	Click(On, bHostile, Point);
	bAttackHeld = bMoveHeld = false;   // a single click, not a hold
}

void UTSHeroControl::Click(ATSCharacter* On, bool bHostile, const FVector& Ground)
{
	const ATSCharacter* Me = Hero();
	if (!Me || Me->IsDead()) return;
	Path.Reset();
	RepathIn = 0.f;
	if (On)
	{
		Goal = bHostile ? ETSClickGoal::Attack : ETSClickGoal::Talk;
		GoalActor = On;
		GoalPoint = On->GetActorLocation();
		bAttackHeld = bHostile;   // held LMB keeps attacking once in range
		bMoveHeld = false;
		return;
	}
	Goal = ETSClickGoal::Move;
	GoalActor = nullptr;
	GoalPoint = Ground;
	bMoveHeld = true;
	bAttackHeld = false;
}

void UTSHeroControl::ClearGoal()
{
	Goal = ETSClickGoal::None;
	GoalActor = nullptr;
	GoalObj = nullptr;
	Path.Reset();
	bMoveHeld = false;
}

bool UTSHeroControl::ClickDestination(FVector& Out) const
{
	if (Goal != ETSClickGoal::Move) return false;
	Out = GoalPoint;
	return true;
}

bool UTSHeroControl::NextCorner(FVector& Out) const
{
	if (Goal == ETSClickGoal::None || !Path.IsValidIndex(PathIndex)) return false;
	Out = Path[PathIndex];
	return true;
}

void UTSHeroControl::Repath(const FVector& To)
{
	const FVector From = GetOwner()->GetActorLocation();
	Path.Reset();
	PathIndex = 1;
	RepathIn = 0.3f;
	if (const UNavigationPath* P = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, To, GetOwner()))
		if (P->IsValid() && P->PathPoints.Num() >= 2) { Path = P->PathPoints; return; }
	Path = { From, To };   // no navmesh (yet): straight line
}

FVector UTSHeroControl::Update(float Dt)
{
	if (Goal == ETSClickGoal::None) return FVector::ZeroVector;
	const FVector At = GetOwner()->GetActorLocation();
	ATSCharacter* T = GoalActor.Get();
	if (Goal == ETSClickGoal::Use)
	{
		ATSInteractable* It = GoalObj.Get();
		if (!It || !It->CanUse()) { ClearGoal(); return FVector::ZeroVector; }
		GoalPoint = It->GetActorLocation();
		if (FVector::Dist2D(GoalPoint, At) - It->Radius <= TalkRange)
		{
			ClearGoal();
			if ((!UseBlocker || UseBlocker(It).IsEmpty()) && Use) Use(It);
			return FVector::ZeroVector;
		}
	}
	else if (Goal != ETSClickGoal::Move && (!T || T->IsDead() || T->IsLeaving())) { ClearGoal(); bAttackHeld = false; return FVector::ZeroVector; }

	if (Goal == ETSClickGoal::Attack)
	{
		if (InAttackRange && InAttackRange(T))
		{
			// In range: swing / shoot. Holding LMB keeps the goal (and the attacks) going; a single click attacks once.
			Path.Reset();
			const bool bHeld = bAttackHeld;
			if (!(IsAttacking && IsAttacking()) && Attack) Attack();
			if (!bHeld) { bAttackHeld = false; ClearGoal(); }
			return FVector::ZeroVector;
		}
		if (IsAttacking && IsAttacking()) return FVector::ZeroVector;   // finish the swing before chasing
	}
	if (Goal == ETSClickGoal::Talk && FVector::Dist2D(T->GetActorLocation(), At) - T->Radius() <= TalkRange)
	{
		ClearGoal();
		if ((!TalkBlocker || TalkBlocker(T).IsEmpty()) && Talk) Talk(T);   // (they may have turned hostile on the way)
		return FVector::ZeroVector;
	}

	// Holding LMB after a ground click: the destination follows the cursor.
	if (Goal == ETSClickGoal::Move && bMoveHeld) { FVector G; if (CursorGround(G)) GoalPoint = G; }
	if (T) GoalPoint = T->GetActorLocation();

	RepathIn -= Dt;
	if (Path.IsEmpty() || RepathIn <= 0.f) Repath(GoalPoint);
	while (PathIndex < Path.Num() && FVector::Dist2D(Path[PathIndex], At) < 45.f) ++PathIndex;
	if (PathIndex >= Path.Num())
	{
		if (Goal == ETSClickGoal::Move && !bMoveHeld) ClearGoal();
		return FVector::ZeroVector;
	}
	return (Path[PathIndex] - At).GetSafeNormal2D();
}

// ---------------------------------------------------------------------------------------------
// Talk mode
// ---------------------------------------------------------------------------------------------

void UTSHeroControl::TryTalk(ATSCharacter* C)
{
	SetTalkMode(false);
	const FString Why = TalkBlocker ? TalkBlocker(C) : FString();
	if (!Why.IsEmpty())
	{
		if (C) UTSFeedback::Get(this)->Float(C->Head() + FVector(0, 0, 40), Why, FLinearColor(0.85f, 0.85f, 0.8f), 0.9f);
		return;
	}
	if (C) Click(C, false, C->GetActorLocation());
}

void UTSHeroControl::TryUse(ATSInteractable* It)
{
	if (!It) return;
	const FString Why = UseBlocker ? UseBlocker(It) : FString();
	if (!Why.IsEmpty()) { UTSFeedback::Get(this)->Float(It->Top(), Why, FLinearColor(0.85f, 0.85f, 0.8f), 0.9f); return; }
	const ATSCharacter* Me = Hero();
	if (!Me || Me->IsDead()) return;
	ClearGoal();
	Goal = ETSClickGoal::Use;
	GoalObj = It;
	GoalPoint = It->GetActorLocation();
}

// ---------------------------------------------------------------------------------------------
// Ability picker: modifier + wheel, slow motion while choosing, release the modifier (or click) to cast
// ---------------------------------------------------------------------------------------------

bool UTSHeroControl::HandleWheel(float Wheel)
{
	if (!IsModifierDown()) return false;
	if (Picker < 0) OpenPicker();
	else CyclePicker(Wheel > 0.f ? -1 : 1);
	return true;
}

void UTSHeroControl::OpenPicker()
{
	const UTSAbilityComponent* Abilities = GetOwner()->FindComponentByClass<UTSAbilityComponent>();
	if (!Abilities) return;
	int32 Start = INDEX_NONE;
	for (int32 I = 0; I < Abilities->Ids.Num() && Start == INDEX_NONE; ++I)
	{
		const int32 S = (LastPicked + I) % Abilities->Ids.Num();
		if (Abilities->Unlocked(Abilities->Ids[S])) Start = S;
	}
	if (Start == INDEX_NONE)
	{
		if (const ATSCharacter* H = Hero())
			UTSFeedback::Get(this)->Float(H->Head() + FVector(0, 0, 30), TSText::Get(this, TEXT("noAbilities"), TEXT("No abilities yet")), FLinearColor(0.8f, 0.8f, 0.8f), 0.8f);
		return;
	}
	Picker = Start;
	const TSJson::FObj Cfg = TSJson::Obj(TSJson::Obj(UTSData::Get(this).World(), TEXT("camera")), TEXT("abilityPicker"));
	UGameplayStatics::SetGlobalTimeDilation(this, float(TSJson::Num(Cfg, TEXT("timeScale"), 0.2)));
}

void UTSHeroControl::CyclePicker(int32 Step)
{
	const UTSAbilityComponent* Abilities = GetOwner()->FindComponentByClass<UTSAbilityComponent>();
	const int32 N = Abilities ? Abilities->Ids.Num() : 0;
	for (int32 I = 1; I <= N; ++I)
	{
		const int32 S = ((Picker + Step * I) % N + N) % N;   // wraps, skipping locked slots
		if (Abilities->Unlocked(Abilities->Ids[S])) { Picker = S; return; }
	}
}

void UTSHeroControl::ClosePicker(bool bCast)
{
	if (Picker < 0) return;
	const int32 Slot = Picker;
	Picker = -1;
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	if (!bCast) return;
	LastPicked = Slot;
	if (UTSAbilityComponent* Abilities = GetOwner()->FindComponentByClass<UTSAbilityComponent>()) Abilities->TryActivate(Slot);
}

bool UTSHeroControl::CancelModes()
{
	if (Picker >= 0) { ClosePicker(false); return true; }
	if (bTalkMode) { SetTalkMode(false); return true; }
	return false;
}

void UTSHeroControl::TestPicker(int32 Steps, bool bOpenOnly)
{
	if (Picker < 0) OpenPicker();
	for (int32 I = 0; I < FMath::Abs(Steps); ++I) CyclePicker(Steps > 0 ? 1 : -1);
	if (!bOpenOnly) ClosePicker(true);
}
