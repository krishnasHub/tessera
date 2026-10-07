#include "TSStats.h"
#include "TSData.h"

UTSStatsComponent::UTSStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // ticked by the owning character
}

TSJson::FObj UTSStatsComponent::Rules() const { return UTSData::Get(this).Section(TEXT("stats")); }
double UTSStatsComponent::Rule(const FString& Key, double Default) const { return UTSData::Get(this).Value(Rules(), Key, Default); }
TSJson::FObj UTSStatsComponent::PoolDef(FName Id) const { return TSJson::Obj(TSJson::Obj(Rules(), TEXT("pools")), Id.ToString()); }
bool UTSStatsComponent::IsPool(FName Id) const { return PoolDef(Id).IsValid(); }
FName UTSStatsComponent::HealthId() const { return FName(TSJson::Str(Rules(), TEXT("health"), TEXT("hp"))); }

float UTSStatsComponent::Get(FName Stat) const
{
	float V = Base.FindRef(Stat);
	for (const FMod& M : Mods) if (M.Stat == Stat) V += M.Value;
	for (const FTSEffect& E : Effects) if (const float* B = E.Mods.Find(Stat)) V += *B;
	return V;
}

float UTSStatsComponent::Eval(const TSJson::FObj& R) const
{
	if (!R) return 0.f;
	const UTSData& D = UTSData::Get(this);
	float V = float(D.Value(R, TEXT("base"), 0));
	if (TSJson::Has(R, TEXT("flat"))) V += Get(FName(TSJson::Str(R, TEXT("flat"))));
	if (const TSJson::FObj Per = TSJson::Obj(R, TEXT("per")))
		for (const auto& KV : Per->Values) V += Get(FName(*KV.Key)) * float(D.Value(Per, FString(*KV.Key), 0));
	return V;
}

float UTSStatsComponent::Max(FName Id) const
{
	const TSJson::FObj Def = PoolDef(Id);
	const float V = Eval(TSJson::Obj(Def, TEXT("max")));
	return TSJson::Bool(Def, TEXT("round")) ? FMath::RoundToFloat(V) : V;
}

bool UTSStatsComponent::Spend(FName Id, float Amount)
{
	FTSPool& P = Pool(Id);
	if (P.Current < Amount) return false;
	P.Current -= Amount;
	P.Delay = float(UTSData::Get(this).Value(PoolDef(Id), TEXT("regenDelay"), 0));
	return true;
}

float UTSStatsComponent::CritChance() const { return FMath::Max(0.f, Eval(TSJson::Obj(Rules(), TEXT("crit")))) / 100.f; }
float UTSStatsComponent::Armor() const { return FMath::Max(0.f, Get(FName(TSJson::Str(Rules(), TEXT("armorStat"), TEXT("armor"))))); }

float UTSStatsComponent::ScaleBy(FName Stat) const
{
	const TSJson::FObj S = TSJson::Obj(Rules(), TEXT("scaling"));
	if (Stat.IsNone() || !TSJson::Has(S, Stat.ToString())) return 1.f;
	return 1.f + Get(Stat) * float(UTSData::Get(this).Value(S, Stat.ToString(), 0));
}

void UTSStatsComponent::Fill()
{
	if (const TSJson::FObj Defs = TSJson::Obj(Rules(), TEXT("pools")))
		for (const auto& KV : Defs->Values) Pool(FName(*KV.Key)).Current = Max(FName(*KV.Key));
}

void UTSStatsComponent::ClampPools()
{
	for (auto& KV : Pools) KV.Value.Current = FMath::Min(KV.Value.Current, Max(KV.Key));
}

void UTSStatsComponent::AddModifiers(FName Source, const TMap<FName, float>& InMods)
{
	for (const auto& KV : InMods) Mods.Add({ Source, KV.Key, KV.Value });
	ClampPools();
}

void UTSStatsComponent::RemoveModifiers(FName Source)
{
	Mods.RemoveAll([&](const FMod& M) { return M.Source == Source; });
	ClampPools();
}

void UTSStatsComponent::AddEffect(const FTSEffect& E)
{
	Effects.RemoveAll([&](const FTSEffect& X) { return X.Id == E.Id; });
	FTSEffect Copy = E;
	Copy.Remaining = E.Duration;
	Copy.TickTimer = E.Period;
	Effects.Add(Copy);
}

void UTSStatsComponent::TickStats(float Dt, TFunctionRef<void(float)> OnHeal, TFunctionRef<void(float, AActor*)> OnDot)
{
	if (const TSJson::FObj Defs = TSJson::Obj(Rules(), TEXT("pools")))
	{
		for (const auto& KV : Defs->Values)
		{
			const TSJson::FObj Regen = TSJson::Obj(KV.Value->AsObject(), TEXT("regen"));
			if (!Regen) continue;
			FTSPool& P = Pool(FName(*KV.Key));
			if (P.Delay > 0) { P.Delay -= Dt; continue; }
			if (P.Lock > 0) { P.Lock -= Dt; continue; }
			P.Current = FMath::Min(Max(FName(*KV.Key)), P.Current + Eval(Regen) * P.RegenMul * Dt);
		}
	}

	for (FTSEffect& E : Effects)
	{
		E.Remaining -= Dt;
		if (E.Period > 0)
		{
			E.TickTimer -= Dt;
			while (E.TickTimer <= 0)
			{
				E.TickTimer += E.Period;
				if (E.Heal > 0) OnHeal(E.Heal);
				if (E.Dot > 0) OnDot(E.Dot, E.Source.Get());
			}
		}
	}
	Effects.RemoveAll([](const FTSEffect& E) { return E.Remaining <= 0; });
	ClampPools();
}
