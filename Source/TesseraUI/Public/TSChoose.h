#pragma once

#include "CoreMinimal.h"
#include "TSUIStyle.h"

/**
 * How every menu reacts to a choice: the highlighted item lights up (hover, arrows, wheel); choosing it turns it
 * the Chosen colour for a moment, the menu fades out, then the choice happens (and a menu that stays up fades back in).
 * Input is ignored while that plays, so one click is one choice. Colours: FTSUIStyle Idle / Highlighted / Chosen.
 */
struct FTSChoose
{
	static FLinearColor Idle() { return FTSUIStyle::Get().Idle; }
	static FLinearColor Highlighted() { return FTSUIStyle::Get().Highlighted; }
	static FLinearColor Chosen() { return FTSUIStyle::Get().Chosen; }
	/** Button colour for item I given the highlighted item. */
	FLinearColor Color(int32 I, int32 Highlight) const { return Item == I ? Chosen() : Highlight == I ? Highlighted() : Idle(); }

	bool Busy() const { return Item >= 0; }
	void Start(int32 InItem, TFunction<void()> InThen) { if (Busy()) return; Item = InItem; T = 0.f; Then = MoveTemp(InThen); }
	/** Advance; returns the menu's opacity for this frame. */
	float Tick(float Dt)
	{
		constexpr float Flash = 0.16f, Fade = 0.22f, FadeIn = 0.18f;
		if (Busy())
		{
			T += Dt;
			if (T < Flash) return 1.f;
			if (T < Flash + Fade) return 1.f - (T - Flash) / Fade;
			TFunction<void()> Run = MoveTemp(Then);
			Item = -1;
			InT = 0.f;
			if (Run) Run();
			return 0.f;
		}
		InT = FMath::Min(InT + Dt, FadeIn);
		return InT / FadeIn;
	}
	/** Restart the fade-in (the menu was just shown). */
	void Reset() { Item = -1; Then = nullptr; InT = 0.f; }

	int32 Item = -1;
	float T = 0.f, InT = 1.f;
	TFunction<void()> Then;
};
