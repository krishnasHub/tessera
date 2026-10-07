#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSSky.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class APostProcessVolume;

/**
 * Sky, light and time of day: sun, moon (a second atmosphere light), a shadowless fill, a real-time sky light,
 * volumetric clouds, height fog and a colour grade, all dynamic (Lumen + virtual shadow maps).
 *
 * The clock runs at <world>.dayNight.secondsPerHour from startHour: the sun crosses the sky 6-20, the moon 20-6,
 * dusk warms the light, fog thickens toward night, and exposure stays inside exposureMinEV..exposureMaxEV with a
 * darker, cooler grade at night (auto exposure would otherwise turn night back into day).
 *
 * Deep night: only what is near the hero or near a registered light can be seen (dayNight.nightVision:
 * heroSight, strength, lightReach). Games ask IsLit() and register their fires and lamps with AddNightLight().
 *
 * Data (<world>): sun { pitch, yaw, intensityLux }, dayNight { enabled, startHour, secondsPerHour, moonLux,
 * nightExposure, exposureMinEV, exposureMaxEV, fog { enabled, day, dusk, night }, nightVision { ... } }.
 * Command line (game prefix): -<P>Hour=<h> start hour, -<P>Sun=pitch,yaw,lux[,r,g,b] freeze the sun, -<P>Fog=0|1.
 */
UCLASS()
class TESSERAWORLD_API ATSSky : public AActor
{
	GENERATED_BODY()

public:
	ATSSky();

	/** Read the night settings and forget the last world's lights. Call before registering lights. */
	static void BeginWorld(const UObject* WorldContext);
	/** Spawn and build the sky for this world. */
	static ATSSky* Spawn(UWorld* World);

	virtual void Tick(float DeltaSeconds) override;

	/** The hour of the day (0-24). */
	static float Hour();
	/** 0 by day, 1 at night (follows the sun). */
	static float Night();
	/** How strongly the dark swallows everything out of reach (0 by day, up to ~1 in deep night). */
	static float Darkness();
	/** How far the hero sees in the dark (uu): nightVision.heroSight plus what the hero carries (SetCarriedLight). */
	static float HeroSight();
	/** A light the hero carries (a lamp, a glowing staff...) adds this much (uu) to their sight in the dark; the game
	 *  sets it (e.g. from UTSDayNight::OnNightLevel). */
	static void SetCarriedLight(float ExtraSight);
	/** Fires, lamps...: (x, y, radius they light). */
	static const TArray<FVector>& NightLights();
	/** A light that keeps the dark back within Radius (scaled by nightVision.lightReach). */
	static void AddNightLight(float X, float Y, float Radius);
	/** Can a point be seen right now (daylight, near the hero, or near a light)? */
	static bool IsLit(const FVector& At, const FVector& Hero);

private:
	void Build();
	void UpdateSky();

	UPROPERTY() TObjectPtr<UDirectionalLightComponent> SunLight;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> MoonLight;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> FillLight;   // shadowless fill from above: dusk shadows aren't black
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyFill;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<APostProcessVolume> Grade;               // time-of-day exposure and colour
	float FogDay = 0.012f, FogDusk = 0.03f, FogNight = 0.05f;
	bool bFog = true;
	bool bDayCycle = false;
	float NightExposure = -2.2f, ExposureMinEV = 2.f, ExposureMaxEV = 5.f;
	float SecondsPerHour = 30.f, SunLux = 9.f, MoonLux = 0.6f;
};
