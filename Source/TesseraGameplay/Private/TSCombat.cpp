#include "TSCombat.h"
#include "TSSleep.h"
#include "TSCharacter.h"
#include "TSFeedback.h"
#include "TSData.h"
#include "TSAssets.h"

#include "EngineUtils.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	float Angle2D(const FVector& Forward, const FVector& To)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward.GetSafeNormal2D(), To.GetSafeNormal2D()), -1.f, 1.f)));
	}
}

TArray<ATSCharacter*> TSCombat::Opponents(const ATSCharacter* Of)
{
	TArray<ATSCharacter*> Out;
	if (!Of) return Out;
	for (TActorIterator<ATSCharacter> It(Of->GetWorld()); It; ++It)
	{
		ATSCharacter* C = *It;
		if (C == Of || C->IsDead() || C->IsLeaving() || C->Team == ETSTeam::Neutral || C->Team == Of->Team) continue;
		Out.Add(C);
	}
	return Out;
}

void TSCombat::Heal(ATSCharacter* Target, float Amount)
{
	if (!Target || Target->IsDead()) return;
	UTSStatsComponent* S = Target->Stats;
	const float Before = S->Health();
	S->Health() = FMath::Min(S->MaxHealth(), S->Health() + Amount);
	const int32 Gained = FMath::RoundToInt(S->Health() - Before);
	if (Gained > 0) UTSFeedback::Get(Target)->Float(Target->Head(), FString::Printf(TEXT("+%d"), Gained), FLinearColor(0.37f, 0.88f, 0.54f), 0.9f);
}

void TSCombat::Dot(ATSCharacter* Target, float Amount, AActor* Src)
{
	if (!Target || Target->IsDead()) return;
	UTSStatsComponent* S = Target->Stats;
	int32 N = FMath::Max(1, FMath::RoundToInt(Amount));
	if (const float Floor = Target->HealthFloor(); Floor > 0.f) N = FMath::Min(N, FMath::Max(0, FMath::FloorToInt(S->Health() - Floor)));
	if (N <= 0) return;
	S->Health() -= N;
	UTSFeedback::Get(Target)->Float(Target->Head(), FString::FromInt(N), FLinearColor(0.5f, 0.82f, 0.5f), 0.8f);
	if (S->Health() <= 0) { S->Health() = 0; Target->Die(Src); }
}

bool TSCombat::Deal(ATSCharacter* Src, ATSCharacter* Target, const FTSHit& Hit)
{
	if (!Src || !Target || Target->IsDead() || Target->Tags.Has(TEXT("Invulnerable"))) return false;
	const UTSData& D = UTSData::Get(Target);
	UTSFeedback* Fb = UTSFeedback::Get(Target);
	UTSStatsComponent* TS = Target->Stats;
	const bool bTargetIsPlayer = Target->Team == ETSTeam::Player;
	const FVector TextAt = Target->Head() + FVector(0, 0, 20);
	const float StaggerTime = float(TS->Rule(TEXT("staggerTime"), 0.55));

	Target->OnStruck(Src);

	// --- damage roll ---
	float Dmg = Hit.Base * Src->Stats->ScaleBy(Hit.Scaling);
	bool bCrit = false;
	if (FMath::FRand() < Src->Stats->CritChance()) { Dmg *= float(TS->Rule(TEXT("critMultiplier"), 1.5)); bCrit = true; }
	const float K = float(TS->Rule(TEXT("armorConstant"), 100));
	Dmg *= K / (K + TS->Armor());
	if (Target->Tags.Has(TEXT("Marked"))) Dmg *= Target->MarkMul;
	// Caught asleep: a heavier blow (tuning.sleep.hitMul), and it's wide awake now.
	if (UTSSleep* Sleep = UTSSleep::Of(Target); Sleep && Sleep->IsAsleep())
	{
		Dmg *= float(TSJson::Num(TSJson::Obj(D.Section(TEXT("tuning")), TEXT("sleep")), TEXT("hitMul"), 2.0));
		Sleep->Wake();
		Fb->Float(TextAt + FVector(0, 0, 25), TEXT("AMBUSH"), FLinearColor(0.7f, 0.8f, 1.f), 0.9f);
	}
	const float Var = float(TS->Rule(TEXT("variance"), 0.1));
	Dmg *= FMath::FRandRange(1.f - Var, 1.f + Var);
	int32 Amount = FMath::Max(1, FMath::RoundToInt(Dmg));
	float Poise = Hit.Poise, Knock = Hit.Knockback;

	// --- guard / perfect guard ---
	if (const TSJson::FObj Guard = Target->GuardStyle())
	{
		const FVector From = Hit.From.IsSet() ? Hit.From.GetValue() : Src->GetActorLocation();
		if (Angle2D(Target->GetActorForwardVector(), From - Target->GetActorLocation()) <= TSJson::Num(Guard, TEXT("arc"), 120) * 0.5)
		{
			const float Perfect = float(TSJson::Num(Guard, TEXT("perfectWindow"), 0));
			if (Perfect > 0.f && Target->GuardTime <= Perfect)
			{
				Fb->Float(TextAt + FVector(0, 0, 20), TSText::Get(Target, TEXT("perfectBlock"), TEXT("PERFECT BLOCK")), FLinearColor::White, 1.1f);
				if (Src != Target && !Src->IsDead() && FVector::Dist2D(Src->GetActorLocation(), Target->GetActorLocation()) < D.Px(140))
				{
					Src->Stagger(StaggerTime * 1.8f);
					Fb->Float(Src->Head() + FVector(0, 0, 40), TSText::Get(Target, TEXT("stagger"), TEXT("STAGGER")), FLinearColor(0.56f, 0.82f, 1.f), 0.9f);
				}
				Fb->Shake(3.f);
				return true;
			}
			// The guard costs a pool per point of damage it stops ("pool", default stamina).
			const FName GuardPool(TSJson::Str(Guard, TEXT("pool"), TEXT("stamina")));
			FTSPool& P = TS->Pool(GuardPool);
			const float Cost = Amount * float(TSJson::Num(Guard, TEXT("staminaPerDamage"), 0.8));
			if (P.Current >= Cost)
			{
				P.Current -= Cost;
				P.Delay = float(D.Value(TSJson::Obj(TSJson::Obj(TS->Rules(), TEXT("pools")), GuardPool.ToString()), TEXT("regenDelay"), 0.55));
				Amount = FMath::RoundToInt(Amount * (1.f - float(TSJson::Num(Guard, TEXT("reduction"), 0.7))));
				Poise *= 0.3f; Knock *= 0.3f;
				Fb->Float(TextAt + FVector(0, 0, 25), TSText::Get(Target, TEXT("block"), TEXT("BLOCK")), FLinearColor(0.78f, 0.83f, 0.88f), 0.8f);
			}
			else
			{
				P.Current = 0;
				Target->Stagger(StaggerTime * 1.5f);
				Fb->Float(TextAt + FVector(0, 0, 30), TSText::Get(Target, TEXT("guardBreak"), TEXT("GUARD BREAK")), FLinearColor(1.f, 0.6f, 0.35f), 1.1f);
			}
		}
	}

	// --- absorb effects (wards, shields) ---
	for (FTSEffect& E : TS->Effects)
	{
		if (E.Absorb <= 0.f || Amount <= 0) continue;
		const int32 Soaked = FMath::Min(int32(E.Absorb), Amount);
		E.Absorb -= Soaked; Amount -= Soaked;
		Fb->Float(TextAt, FString::Printf(TEXT("(%d)"), Soaked), FLinearColor(1.f, 0.91f, 0.66f), 0.9f);
	}

	// --- health ---
	Target->Flash();
	if (Amount > 0)
	{
		TS->Health() -= Amount;
		const FLinearColor C = bTargetIsPlayer ? FLinearColor(1.f, 0.35f, 0.35f) : (bCrit ? FLinearColor(1.f, 0.83f, 0.3f) : FLinearColor::White);
		Fb->Float(TextAt, bCrit ? FString::Printf(TEXT("%d!"), Amount) : FString::FromInt(Amount), C, bCrit ? 1.35f : 1.f);
	}
	if (UNiagaraSystem* FX = TSAssets::Get<UNiagaraSystem>(Target, TEXT("hitEffect")))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Target->GetWorld(), FX, Target->Chest(), FRotator::ZeroRotator, FVector(0.6f));
	}

	// --- the target may end it here (a duel opponent yields instead of dying) ---
	if (!Hit.bIgnoreYield && Target->OnHurt(Src)) return true;

	// --- knockback ---
	if (Knock > 0.f)
	{
		FVector Dir = Hit.Dir.IsSet() ? Hit.Dir.GetValue() : (Target->GetActorLocation() - Src->GetActorLocation());
		Target->Knock(Dir.GetSafeNormal2D() * D.Px(Knock) * Target->KnockbackMul());
	}

	// --- provoke / alert, poise, stagger ---
	Target->OnDamaged(Src);
	Target->Poise -= Poise;
	Target->PoiseTimer = float(TS->Rule(TEXT("poiseRegenDelay"), 2));
	if (Target->Poise <= 0.f)
	{
		Target->Poise = Target->MaxPoise;
		Target->Stagger(StaggerTime);
		Fb->Float(TextAt + FVector(0, 0, 35), TSText::Get(Target, TEXT("stagger"), TEXT("STAGGER")), FLinearColor(0.56f, 0.82f, 1.f), 0.9f);
	}
	if (Target->HitIframes > 0.f) Target->Tags.Add(TEXT("Invulnerable"), Target->HitIframes);
	Fb->Shake(bTargetIsPlayer ? 7.f : (bCrit ? 3.f : 1.5f));

	if (TS->Health() <= 0.f) { TS->Health() = 0; Target->Die(Src); }
	return true;
}
