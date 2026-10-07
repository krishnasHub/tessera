#include "TSAbilities.h"
#include "Tessera.h"
#include "TSCharacter.h"
#include "TSData.h"
#include "TSFeedback.h"
#include "TSFX.h"
#include "TSProjectile.h"
#include "TSPerception.h"
#include "TSAreaEvents.h"
#include "EngineUtils.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor Muted(0.67f, 0.67f, 0.73f);
}

// ---------------------------------------------------------------------------------------------
// Context helpers
// ---------------------------------------------------------------------------------------------

FTSHit FTSAbilityContext::MakeHit(float Base) const
{
	FTSHit H;
	H.Base = Base;
	H.Scaling = FName(TSJson::Str(Def, TEXT("scaling")));
	H.Poise = float(TSJson::Num(Def, TEXT("poise"), 0));
	H.Knockback = float(TSJson::Num(Def, TEXT("knockback"), 0));
	return H;
}

float FTSAbilityContext::Scaled(float V) const { return V * Caster->Stats->ScaleBy(FName(TSJson::Str(Def, TEXT("scaling")))); }
double FTSAbilityContext::Num(const TCHAR* Key, double Default) const { return TSJson::Num(Def, Key, Default); }
float FTSAbilityContext::Dist(const TCHAR* Key, double Default) const { return Data->Px(TSJson::Num(Def, Key, Default)); }

void FTSAbilityContext::Fail(const FString& Msg) const
{
	UTSFeedback::Get(Caster)->Float(Caster->Head() + FVector(0, 0, 30), Msg, Muted, 0.8f);
}

ATSCharacter* FTSAbilityContext::TargetNearAim(float Range) const
{
	// The opponent closest to the aim line, within range and line of sight.
	const FVector Aim = Caster->AimDirection();
	ATSCharacter* Best = nullptr;
	float BestScore = -1.f;
	for (ATSCharacter* E : TSCombat::Opponents(Caster))
	{
		const FVector To = E->Chest() - Caster->Chest();
		if (To.Size() > Range) continue;
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AbilityTarget), false, Caster);
		Q.AddIgnoredActor(E);
		if (World->LineTraceSingleByChannel(H, Caster->Chest(), E->Chest(), ECC_Visibility, Q)) continue;
		const float Score = FVector::DotProduct(Aim, To.GetSafeNormal());
		if (Score > 0.75f && Score > BestScore) { BestScore = Score; Best = E; }
	}
	return Best;
}

// ---------------------------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------------------------

UTSAbilityComponent::UTSAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TMap<FString, UTSAbilityComponent::FTypeFn>& UTSAbilityComponent::Types()
{
	static TMap<FString, FTypeFn> T;
	return T;
}

void UTSAbilityComponent::RegisterType(const FString& Type, FTypeFn Fn) { RegisterBuiltIns(); Types().Add(Type, MoveTemp(Fn)); }

ATSCharacter* UTSAbilityComponent::Caster() const { return Cast<ATSCharacter>(GetOwner()); }

void UTSAbilityComponent::Setup(const TArray<FString>& InIds)
{
	Ids = InIds;
	Cooldowns.Reset();
	for (const FString& Id : Ids) Cooldowns.Add(Id, 0.f);
}

TSJson::FObj UTSAbilityComponent::Def(const FString& Id) const { return UTSData::Get(this).Entry(TEXT("abilities"), Id); }

bool UTSAbilityComponent::Unlocked(const FString& Id) const
{
	return Caster() && Caster()->Level() >= int32(TSJson::Num(Def(Id), TEXT("unlockLevel"), 1));
}

bool UTSAbilityComponent::CanAfford(const TSJson::FObj& D) const
{
	const ATSCharacter* C = Caster();
	if (!C || !D) return false;
	for (const auto& KV : D->Values)
		if (C->Stats->IsPool(FName(*KV.Key)) && C->Stats->Cur(FName(*KV.Key)) < KV.Value->AsNumber()) return false;
	return true;
}

void UTSAbilityComponent::TickCooldowns(float Dt)
{
	for (auto& KV : Cooldowns) KV.Value = FMath::Max(0.f, KV.Value - Dt);
}

bool UTSAbilityComponent::TryActivate(int32 Slot)
{
	ATSCharacter* C = Caster();
	if (!C || !Ids.IsValidIndex(Slot) || C->IsDead()) return false;
	RegisterBuiltIns();
	const FString Id = Ids[Slot];
	FTSAbilityContext Ctx;
	Ctx.Caster = C;
	Ctx.Def = Def(Id);
	Ctx.World = C->GetWorld();
	Ctx.Data = &UTSData::Get(C);
	Ctx.Color = TSJson::Color(TSJson::Str(Ctx.Def, TEXT("color")), FLinearColor::White);
	Ctx.Ground = C->GetActorLocation() - FVector(0, 0, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.f);
	const TSJson::FObj& D = Ctx.Def;

	if (!Unlocked(Id))
	{
		Ctx.Fail(TSText::Get(C, TEXT("unlocksAt"), TEXT("{name} unlocks at Lv {level}"),
			{ { TEXT("name"), TSJson::Str(D, TEXT("name")) }, { TEXT("level"), FString::FromInt(int32(TSJson::Num(D, TEXT("unlockLevel"), 1))) } }));
		return false;
	}
	if (!C->CanAct()) return false;
	if (Cooldowns.FindRef(Id) > 0.f) { Ctx.Fail(TSText::Get(C, TEXT("notReady"), TEXT("Not ready"))); return false; }
	for (const auto& KV : D->Values)
	{
		const FName Pool(*KV.Key);
		if (C->Stats->IsPool(Pool) && C->Stats->Cur(Pool) < KV.Value->AsNumber())
		{
			Ctx.Fail(TSText::Get(C, TEXT("notEnough"), TEXT("Not enough {pool}"), { { TEXT("pool"), Pool.ToString() } }));
			return false;
		}
	}
	FString Why;
	if (TSJson::Has(D, TEXT("requires")) && !C->MeetsRequirement(TSJson::Str(D, TEXT("requires")), Why)) { Ctx.Fail(Why); return false; }

	const FTypeFn* Fn = Types().Find(TSJson::Str(D, TEXT("type")));
	if (!Fn) { UE_LOG(LogTessera, Warning, TEXT("Unknown ability type %s"), *TSJson::Str(D, TEXT("type"))); return false; }
	TSPerception::Reveal(C);   // acting breaks stealth (a smoke bomb re-applies it)
	if (!(*Fn)(Ctx)) return false;
	C->OnAbilityUsed(D);
	for (const auto& KV : D->Values) if (C->Stats->IsPool(FName(*KV.Key))) C->Stats->Spend(FName(*KV.Key), float(KV.Value->AsNumber()));
	Cooldowns.Add(Id, float(TSJson::Num(D, TEXT("cooldown"), 1)));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Built-in types
// ---------------------------------------------------------------------------------------------

void UTSAbilityComponent::RegisterBuiltIns()
{
	static bool bDone = false;
	if (bDone) return;
	bDone = true;
	TMap<FString, FTypeFn>& T = Types();

	T.Add(TEXT("projectile"), [](const FTSAbilityContext& X)   // a bolt, a fireball, a volley of arrows
	{
		ATSCharacter* P = X.Caster;
		P->FaceAim();
		const bool bArrows = TSJson::Bool(X.Def, TEXT("arrow"));
		if (bArrows) P->OnAbilityUsed(X.Def);   // the bow comes out before we find the muzzle
		const FVector From = P->Muzzle();
		const FVector Dir = P->AimDirection(From);
		const int32 N = int32(X.Num(TEXT("count"), 1));
		const float Spread = float(X.Num(TEXT("spread"), 0));
		const float Range = X.Dist(TEXT("range"), 420);
		const FVector Land = bArrows ? P->ArrowTarget(From, Range) : FVector::ZeroVector;
		for (int32 I = 0; I < N; ++I)
		{
			const float Angle = (I - (N - 1) * 0.5f) * Spread;
			const FVector ShotDir = Dir.RotateAngleAxis(Angle, FVector::UpVector);
			if (bArrows)
			{
				// Arrows arc: fan the landing points around the aim point.
				const FVector To = From + (Land - From).RotateAngleAxis(Angle, FVector::UpVector);
				ATSProjectile::FireArrow(P, From + ShotDir * 15.f, To, X.Dist(TEXT("speed"), 400), X.Dist(TEXT("radius"), 6), X.Color, X.MakeHit(float(X.Num(TEXT("damage"), 10))));
				continue;
			}
			if (ATSProjectile* Shot = ATSProjectile::Fire(P, From + ShotDir * 15.f, ShotDir, X.Dist(TEXT("speed"), 400), Range, X.Dist(TEXT("radius"), 6), X.Color, false, X.MakeHit(float(X.Num(TEXT("damage"), 10)))))
				Shot->Scar = TSJson::Obj(X.Def, TEXT("scar"));   // marks the ground where it bursts
		}
		return true;
	});

	// A cleave, a frost nova. Data: radius, damage; applyTag { tag, duration, everyone } (everyone: every character in
	// the area gets the tag, not just the opponents hit, and the area is announced through UTSAreaEvents);
	// fx { shape: "ring" | "sphere", ground: seconds of a stain on the ground, groundColor }; scar (ATSFX::Scar).
	T.Add(TEXT("aoe"), [](const FTSAbilityContext& X)
	{
		ATSCharacter* P = X.Caster;
		const float R = X.Dist(TEXT("radius"), 80);
		const TSJson::FObj Fx = TSJson::Obj(X.Def, TEXT("fx"));
		if (TSJson::Str(Fx, TEXT("shape"), TEXT("ring")) == TEXT("sphere")) ATSFX::Sphere(X.World, X.Ground, R, X.Color, float(TSJson::Num(Fx, TEXT("time"), 0.7)));
		else ATSFX::Ring(X.World, X.Ground, R, X.Color, 0.4f);
		const double Ground = TSJson::Num(Fx, TEXT("ground"), 0);
		if (Ground > 0)
			ATSFX::Stain(X.World, X.Ground, R, TSJson::Has(Fx, TEXT("groundColor")) ? TSJson::Color(TSJson::Str(Fx, TEXT("groundColor"))) : X.Color, float(Ground));
		ATSFX::Scar(X.World, X.Ground, TSJson::Obj(X.Def, TEXT("scar")), R * 0.9f);   // a mark left on the ground (data: scar)

		const TSJson::FObj Tag = TSJson::Obj(X.Def, TEXT("applyTag"));
		const FName TagName = Tag ? FName(TSJson::Str(Tag, TEXT("tag"))) : NAME_None;
		const float TagTime = Tag ? float(TSJson::Num(Tag, TEXT("duration"), 3)) : 0.f;
		auto InArea = [&](const ATSCharacter* E) { return FVector::Dist2D(E->GetActorLocation(), P->GetActorLocation()) <= R + E->Radius(); };
		for (ATSCharacter* E : TSCombat::Opponents(P))
		{
			if (!InArea(E)) continue;
			FTSHit H = X.MakeHit(float(X.Num(TEXT("damage"), 10)));
			H.Dir = (E->GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
			if (TSCombat::Deal(P, E, H) && Tag) E->Tags.Add(TagName, TagTime);
		}
		if (Tag && TSJson::Bool(Tag, TEXT("everyone")))
		{
			for (TActorIterator<ATSCharacter> It(X.World); It; ++It)
				if (*It != P && !It->IsDead() && InArea(*It)) It->Tags.Add(TagName, TagTime);
			if (UTSAreaEvents* Events = UTSAreaEvents::Get(X.World)) Events->OnStatus.Broadcast(X.Ground, R, TagName, TagTime);
		}
		return true;
	});

	T.Add(TEXT("cone"), [](const FTSAbilityContext& X)   // a shield bash: always staggers
	{
		ATSCharacter* P = X.Caster;
		P->FaceAim();
		const float Range = X.Dist(TEXT("range"), 40), Arc = float(X.Num(TEXT("arc"), 90));
		ATSFX::Burst(X.World, P->Chest() + P->Facing() * 70.f, 45.f, X.Color, 0.2f);
		for (ATSCharacter* E : TSCombat::Opponents(P))
		{
			const FVector To = E->GetActorLocation() - P->GetActorLocation();
			if (To.Size2D() - E->Radius() > Range + P->Radius()) continue;
			if (FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(P->Facing(), To.GetSafeNormal2D()))) > Arc * 0.5f) continue;
			if (TSCombat::Deal(P, E, X.MakeHit(float(X.Num(TEXT("damage"), 8)))) && !E->IsDead()) E->Stagger(float(X.Num(TEXT("stagger"), 1.0)));
		}
		return true;
	});

	T.Add(TEXT("dashStrike"), [](const FTSAbilityContext& X)   // a charge, a shadow dash
	{
		ATSCharacter* P = X.Caster;
		P->FaceAim();
		const float Duration = float(X.Num(TEXT("duration"), 0.2));
		P->StartDash(P->AimDirection().GetSafeNormal2D(), X.Dist(TEXT("distance"), 190) / Duration, Duration, X.Def);
		return true;
	});

	T.Add(TEXT("buff"), [](const FTSAbilityContext& X)   // stat bonuses for a while, and / or a damage absorb
	{
		FTSEffect E;
		E.Id = FName(TSJson::Str(X.Def, TEXT("name")));
		E.Name = TSJson::Str(X.Def, TEXT("name"));
		E.Duration = float(X.Num(TEXT("duration"), 8));
		if (const TSJson::FObj Mods = TSJson::Obj(X.Def, TEXT("mods"))) for (const auto& KV : Mods->Values) E.Mods.Add(FName(*KV.Key), float(KV.Value->AsNumber()));
		if (TSJson::Has(X.Def, TEXT("absorb"))) E.Absorb = FMath::RoundToFloat(X.Scaled(float(X.Num(TEXT("absorb"), 30))));
		X.Caster->Stats->AddEffect(E);
		ATSFX::Ring(X.World, X.Ground, 140.f, X.Color, 0.5f);
		return true;
	});

	T.Add(TEXT("blink"), [](const FTSAbilityContext& X)   // a short teleport; stops at the first wall or water
	{
		ATSCharacter* P = X.Caster;
		const FVector Dir = P->AimDirection().GetSafeNormal2D();
		const float Max = X.Dist(TEXT("distance"), 170);
		const float R = P->Radius(), HalfH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FVector Best = P->GetActorLocation();
		bool bAny = false;
		for (float S = 30.f; S <= Max; S += 30.f)
		{
			const FVector Test = P->GetActorLocation() + Dir * S;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(Blink), false, P);
			if (X.World->OverlapAnyTestByChannel(Test, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(R, HalfH - 5.f), Q)) break;
			Best = Test; bAny = true;
		}
		if (!bAny) return false;
		ATSFX::Burst(X.World, P->Chest(), 50.f, X.Color, 0.3f);
		P->SetActorLocation(Best, false, nullptr, ETeleportType::TeleportPhysics);
		P->SetActorRotation(Dir.Rotation());
		P->Tags.Add(TEXT("Invulnerable"), float(X.Num(TEXT("iframes"), 0.15)));
		ATSFX::Burst(X.World, P->Chest(), 50.f, X.Color, 0.3f);
		return true;
	});

	T.Add(TEXT("chain"), [](const FTSAbilityContext& X)   // chain lightning: jumps between foes, weaker each time
	{
		ATSCharacter* P = X.Caster;
		ATSCharacter* Target = X.TargetNearAim(X.Dist(TEXT("range"), 320));
		if (!Target) { X.Fail(TSText::Get(P, TEXT("noTarget"), TEXT("No target"))); return false; }
		TArray<FVector> Points = { P->Chest() + P->Facing() * 40.f };
		TSet<ATSCharacter*> Hit;
		float Base = float(X.Num(TEXT("damage"), 20));
		const int32 Jumps = int32(X.Num(TEXT("jumps"), 3));
		const float JumpRange = X.Dist(TEXT("jumpRange"), 140);
		for (int32 I = 0; I <= Jumps && Target; ++I)
		{
			Hit.Add(Target);
			Points.Add(Target->Chest());
			ATSFX::Scar(X.World, ATSFX::GroundBelow(X.World, Target->GetActorLocation()), TSJson::Obj(X.Def, TEXT("scar")), 60.f);   // burns where it strikes
			FTSHit H = X.MakeHit(Base);
			H.Knockback = 60.f;
			TSCombat::Deal(P, Target, H);
			Base *= float(X.Num(TEXT("falloff"), 0.75));
			ATSCharacter* From = Target;
			Target = nullptr;
			float BestD = JumpRange;
			for (ATSCharacter* E : TSCombat::Opponents(P))
			{
				const float Dd = FVector::Dist(From->GetActorLocation(), E->GetActorLocation());
				if (!Hit.Contains(E) && Dd <= BestD) { BestD = Dd; Target = E; }
			}
		}
		P->FaceAim();
		ATSFX::Bolt(X.World, Points, X.Color);
		return true;
	});

	T.Add(TEXT("smoke"), [](const FTSAbilityContext& X)   // hidden for a while; foes in the blast stagger, hunters lose you
	{
		ATSCharacter* P = X.Caster;
		const float R = X.Dist(TEXT("radius"), 120);
		const float Duration = float(X.Num(TEXT("duration"), 3));
		const float StaggerTime = float(X.Num(TEXT("stagger"), 0));
		ATSFX::Smoke(X.World, X.Ground, R, Duration);
		TSPerception::Hide(P, Duration);
		for (ATSCharacter* E : TSCombat::Opponents(P))
		{
			const float Dist = FVector::Dist2D(E->GetActorLocation(), P->GetActorLocation());
			// Caught in the blast: staggered (neutral factions are left alone - it isn't an attack).
			if (StaggerTime > 0.f && Dist < R + E->Radius() && !E->IsPassive())
			{
				E->Stagger(StaggerTime);
				UTSFeedback::Get(P)->Float(E->Head() + FVector(0, 0, 30), TSText::Get(P, TEXT("staggered"), TEXT("Staggered")), FLinearColor(0.75f, 0.78f, 0.82f), 0.8f);
			}
			if (Dist < R * 3.f) E->LoseTrack();   // further out, anyone hunting you loses track
		}
		return true;
	});

	T.Add(TEXT("weaponBuff"), [](const FTSAbilityContext& X)   // the caster's attacks carry X.Def.poison (or onHit) for a while
	{
		FTSEffect E;
		E.Id = FName(TSJson::Str(X.Def, TEXT("name")));
		E.Name = TSJson::Str(X.Def, TEXT("name"));
		E.Duration = float(X.Num(TEXT("duration"), 8));
		E.OnHit = TSJson::Has(X.Def, TEXT("onHit")) ? TSJson::Obj(X.Def, TEXT("onHit")) : TSJson::Obj(X.Def, TEXT("poison"));
		X.Caster->Stats->AddEffect(E);
		ATSFX::Ring(X.World, X.Ground, 90.f, X.Color, 0.4f);
		return true;
	});

	T.Add(TEXT("heal"), [](const FTSAbilityContext& X)   // heal over time
	{
		FTSEffect E;
		E.Id = FName(TSJson::Str(X.Def, TEXT("name")));
		E.Name = TSJson::Str(X.Def, TEXT("name"));
		E.Duration = float(X.Num(TEXT("duration"), 4));
		E.Period = float(X.Num(TEXT("period"), 0.5));
		E.Heal = X.Scaled(float(X.Num(TEXT("healPerTick"), 7)));
		X.Caster->Stats->AddEffect(E);
		ATSFX::Ring(X.World, X.Ground, 110.f, X.Color, 0.6f);
		return true;
	});

	T.Add(TEXT("daze"), [](const FTSAbilityContext& X)   // stuns a foe that understands words, and opens a conversation
	{
		ATSCharacter* P = X.Caster;
		UTSFeedback* Fb = UTSFeedback::Get(P);
		ATSCharacter* Tg = X.TargetNearAim(X.Dist(TEXT("range"), 260));
		if (!Tg) { X.Fail(TSText::Get(P, TEXT("noTarget"), TEXT("No target"))); return false; }
		if (!Tg->IsReasonable()) { Fb->Float(Tg->Head(), TSText::Get(P, TEXT("notReasonable"), TEXT("It doesn't understand words")), Muted, 0.8f); return false; }
		if (Tg->IsPassive()) { Fb->Float(Tg->Head(), TSText::Get(P, TEXT("notFighting"), TEXT("They're not fighting you")), Muted, 0.8f); return false; }
		Tg->Stagger(float(X.Num(TEXT("daze"), 3)));
		Fb->Float(Tg->Head() + FVector(0, 0, 40), TSText::Get(P, TEXT("dazed"), TEXT("DAZED")), X.Color, 1.1f);
		ATSFX::Bolt(X.World, { P->Chest(), Tg->Chest() }, X.Color);
		const FString Node = Tg->ParleyNode();
		if (!Node.IsEmpty()) P->OnParley(Tg, Node);   // talk, mid-fight
		return true;
	});

	T.Add(TEXT("mark"), [](const FTSAbilityContext& X)   // the target takes more damage for a while
	{
		ATSCharacter* Tg = X.TargetNearAim(X.Dist(TEXT("range"), 320));
		if (!Tg) { X.Fail(TSText::Get(X.Caster, TEXT("noTarget"), TEXT("No target"))); return false; }
		Tg->Tags.Add(TEXT("Marked"), float(X.Num(TEXT("duration"), 8)));
		Tg->MarkMul = float(X.Num(TEXT("damageTakenMul"), 1.25));
		ATSFX::Ring(X.World, Tg->GetActorLocation() - FVector(0, 0, 85), 90.f, X.Color, 0.6f);
		return true;
	});
}
