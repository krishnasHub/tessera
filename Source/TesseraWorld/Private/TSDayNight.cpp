#include "TSDayNight.h"
#include "Engine/World.h"

UTSDayNight* UTSDayNight::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UTSDayNight>() : nullptr;
}

void UTSDayNight::Update(float InHour, float InNight)
{
	const bool bFirst = Night < 0.f;
	const float Prev = bFirst ? InNight : Night;

	if (bFirst || FMath::Abs(InNight - Night) >= 0.004f || (InNight != Night && (InNight <= 0.f || InNight >= 1.f)))
	{
		Night = InNight;
		OnNightLevel.Broadcast(Night);
	}

	// Day below 0.1, night above 0.9; in between it's dusk while it darkens and dawn while it lightens.
	ETSDayPhase P = CurPhase;
	if (InNight <= 0.1f) P = ETSDayPhase::Day;
	else if (InNight >= 0.9f) P = ETSDayPhase::Night;
	else if (InNight > Prev) P = ETSDayPhase::Dusk;
	else if (InNight < Prev) P = ETSDayPhase::Dawn;
	else if (bFirst) P = InHour < 12.f ? ETSDayPhase::Dawn : ETSDayPhase::Dusk;
	if (bFirst || P != CurPhase) { CurPhase = P; OnPhase.Broadcast(CurPhase); }

	const int32 H = FMath::FloorToInt(InHour) % 24;
	if (H != CurHour) { CurHour = H; OnHour.Broadcast(CurHour); }
}
