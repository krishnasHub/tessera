#pragma once

#include "CoreMinimal.h"

class UCanvas;
class APlayerController;
class ATSCharacter;

/**
 * World-anchored drawing for a game's AHUD::DrawHUD (canvas, every frame): text and bars at projected points,
 * UTSFeedback's floating text, a pulsing ring on the ground, threat-sense arrows. Sizes are for 1080p; pass
 * Scale = Canvas->ClipY / 1080 to follow the resolution.
 */
namespace TSHUDDraw
{
	/** Text centred on (or starting at) X, with Y its middle; an optional drop shadow. */
	TESSERAUI_API void Text(UCanvas* Canvas, const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter = true, bool bShadow = true);
	/** A bar: dark frame, coloured fill. */
	TESSERAUI_API void Bar(UCanvas* Canvas, float X, float Y, float W, float H, float Frac, const FLinearColor& C);
	/** UTSFeedback's floaters (damage numbers, "BLOCK"...), fading at the end of their life. */
	TESSERAUI_API void Floaters(UCanvas* Canvas, const UObject* WorldContext, float Scale);
	/** A pulsing circle on the ground at At (a click-to-move destination). */
	TESSERAUI_API void GroundRing(UCanvas* Canvas, const FVector& At, float Radius, const FLinearColor& C, float Thickness);
	/** Threat sense: arrows at the screen edge pointing at off-screen foes hunting Hero (TSPerception::Hunters
	 *  within ThreatRange), relative to the camera's yaw; pulsing red while one winds up an attack. */
	TESSERAUI_API void ThreatArrows(UCanvas* Canvas, const APlayerController* PC, const ATSCharacter* Hero, float Scale);
}
