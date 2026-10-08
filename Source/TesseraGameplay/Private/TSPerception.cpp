#include "TSPerception.h"
#include "TSCharacter.h"
#include "TSCombat.h"
#include "TSData.h"
#include "TSSleep.h"

#include "Engine/World.h"
#include "EngineUtils.h"

FTSSenses FTSSenses::Read(const UObject* WorldContext, const TSJson::FObj& Def, float InRange, const FString& DefaultsKey)
{
	const UTSData& D = UTSData::Get(WorldContext);
	const TSJson::FObj Base = TSJson::Obj(D.Section(TEXT("tuning")), DefaultsKey);
	FTSSenses S;
	S.Range = InRange;
	S.Hear = D.Px(TSJson::Num(Def, TEXT("hearRadius"), TSJson::Num(Base, TEXT("hearRadius"), 70)));
	S.Cone = float(TSJson::Num(Def, TEXT("visionAngle"), TSJson::Num(Base, TEXT("coneAngle"), 120)));
	return S;
}

namespace TSPerception
{
	bool IsHidden(const ATSCharacter* C) { return C && C->Tags.Has(TEXT("Hidden")); }
	void Hide(ATSCharacter* C, float Duration) { if (C) C->Tags.Add(TEXT("Hidden"), Duration); }
	void Reveal(ATSCharacter* C) { if (C) C->Tags.Remove(TEXT("Hidden")); }

	bool HasLineOfSight(const ATSCharacter* From, const ATSCharacter* To)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(TSSight), false, From);
		Q.AddIgnoredActor(To);
		return !From->GetWorld()->LineTraceSingleByChannel(Hit, From->Head(), To->Head(), ECC_Visibility, Q);
	}

	bool CanNotice(const ATSCharacter* Viewer, const ATSCharacter* Target, const FTSSenses& S)
	{
		if (!Viewer || !Target || Target->IsDead() || IsHidden(Target)) return false;
		const float Dist = FVector::Dist2D(Target->GetActorLocation(), Viewer->GetActorLocation());
		if (Dist >= S.Range) return false;
		// Sneaking (tag "Sneaking": crouched, slow): heard only that much closer (tuning.sneak.hearMul).
		FTSSenses Heard = S;
		if (Target->Tags.Has(TEXT("Sneaking")))
			Heard.Hear *= float(TSJson::Num(TSJson::Obj(UTSData::Get(Viewer).Section(TEXT("tuning")), TEXT("sneak")), TEXT("hearMul"), 0.25));
		const FVector To = (Target->GetActorLocation() - Viewer->GetActorLocation()).GetSafeNormal2D();
		// Asleep: eyes shut, and only a noise right beside it gets through (tuning.sleep.hearMul).
		if (UTSSleep::IsAsleep(Viewer))
		{
			const float HearMul = float(TSJson::Num(TSJson::Obj(UTSData::Get(Viewer).Section(TEXT("tuning")), TEXT("sleep")), TEXT("hearMul"), 0.35));
			return Dist < Heard.Hear * HearMul && HasLineOfSight(Viewer, Target);
		}
		const bool bInView = Dist < Heard.Hear || FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Viewer->Facing(), To))) <= S.Cone * 0.5f;
		return bInView && HasLineOfSight(Viewer, Target);
	}

	float ThreatRange(const UObject* WorldContext)
	{
		const UTSData& D = UTSData::Get(WorldContext);
		return D.Px(TSJson::Num(TSJson::Obj(D.Section(TEXT("tuning")), TEXT("threatSense")), TEXT("range"), 450));
	}

	float RevealAfterAttack(const UObject* WorldContext)
	{
		return float(TSJson::Num(TSJson::Obj(UTSData::Get(WorldContext).Section(TEXT("tuning")), TEXT("threatSense")), TEXT("revealAfterAttack"), 1.5));
	}

	TArray<ATSCharacter*> Hunters(const ATSCharacter* Of, float MaxDistance)
	{
		TArray<ATSCharacter*> Out;
		if (!Of) return Out;
		for (ATSCharacter* C : TSCombat::Opponents(Of))
			if (C->IsHunting() && FVector::Dist(C->GetActorLocation(), Of->GetActorLocation()) <= MaxDistance) Out.Add(C);
		return Out;
	}

	ATSCharacter* NearestHunter(const ATSCharacter* Of, float MaxDistance)
	{
		ATSCharacter* Best = nullptr;
		float BestD = MaxDistance;
		if (!Of) return Best;
		for (ATSCharacter* C : TSCombat::Opponents(Of))
		{
			if (!C->IsHunting()) continue;
			const float Dist = FVector::Dist2D(C->GetActorLocation(), Of->GetActorLocation());
			if (Dist < BestD) { BestD = Dist; Best = C; }
		}
		return Best;
	}
}
