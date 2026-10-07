#include "TSSky.h"
#include "Tessera.h"
#include "TSData.h"
#include "TSAssets.h"
#include "TSLook.h"

#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// Time of day and night vision (one world at a time).
	float GHour = 14.f, GNight = 0.f;
	float GHeroSight = 1000.f, GDarkStrength = 0.97f, GLightReach = 1.f;
	TArray<FVector> GNightLights;   // (x, y, radius)
}

ATSSky::ATSSky()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
}

void ATSSky::BeginWorld(const UObject* WorldContext)
{
	const TSJson::FObj NV = TSJson::Obj(TSJson::Obj(UTSData::Get(WorldContext).World(), TEXT("dayNight")), TEXT("nightVision"));
	GHeroSight = float(TSJson::Num(NV, TEXT("heroSight"), 1000));
	GDarkStrength = float(TSJson::Num(NV, TEXT("strength"), 0.97));
	GLightReach = float(TSJson::Num(NV, TEXT("lightReach"), 1.0));
	GNightLights.Reset();
}

ATSSky* ATSSky::Spawn(UWorld* World)
{
	ATSSky* Sky = World->SpawnActor<ATSSky>();
	if (Sky) Sky->Build();
	return Sky;
}

void ATSSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bDayCycle) return;
	GHour = FMath::Fmod(GHour + DeltaSeconds / FMath::Max(SecondsPerHour, 0.1f), 24.f);
	UpdateSky();
}

float ATSSky::Hour() { return GHour; }
float ATSSky::Night() { return GNight; }
float ATSSky::Darkness() { return GDarkStrength * FMath::SmoothStep(0.45f, 1.f, GNight); }
float ATSSky::HeroSight() { return GHeroSight; }
const TArray<FVector>& ATSSky::NightLights() { return GNightLights; }
void ATSSky::AddNightLight(float X, float Y, float Radius) { GNightLights.Add(FVector(X, Y, Radius * GLightReach)); }

bool ATSSky::IsLit(const FVector& At, const FVector& Hero)
{
	if (Darkness() < 0.5f) return true;
	if (FVector::Dist2D(At, Hero) <= GHeroSight * 0.8f) return true;
	for (const FVector& L : GNightLights) if (FVector::Dist2D(At, FVector(L.X, L.Y, 0.f)) <= L.Z * 0.75f) return true;
	return false;
}

void ATSSky::UpdateSky()
{
	// Sun: rises in the east (+X) at 6, highest in the south at 13, sets in the west at 20 (long summer
	// evenings: golden light from about 17). A light's rotation is the way its light travels, so it points away
	// from where the sun is in the sky.
	auto Place = [](UDirectionalLightComponent* L, float Elevation, float Yaw)
	{
		L->SetWorldRotation(FRotator(-Elevation, Yaw, 0.f));
	};
	constexpr float Rise = 6.f, Set = 20.f;
	const float DayLen = Set - Rise;
	const float SunElev = GHour >= Rise && GHour <= Set ? 58.f * FMath::Sin(PI * (GHour - Rise) / DayLen) : -20.f;
	const float SunK = FMath::SmoothStep(-3.f, 9.f, SunElev);
	GNight = 1.f - SunK;
	Place(SunLight, FMath::Max(SunElev, -6.f), 180.f + (GHour - Rise) * 180.f / DayLen);
	const float Warm = FMath::Clamp(SunElev / 32.f, 0.f, 1.f);
	SunLight->SetLightColor(FMath::Lerp(FLinearColor(1.f, 0.48f, 0.22f), FLinearColor(1.f, 0.96f, 0.9f), Warm));
	SunLight->SetIntensity(SunLux * SunK * (0.3f + 0.7f * Warm));   // weaker as it nears the horizon
	SunLight->SetCastShadows(SunK > 0.02f);

	// Moon: the same arc, twelve hours later.
	const float MoonHour = GHour < Rise ? GHour + 24.f : GHour;
	const float NightLen = 24.f - DayLen;
	const float MoonElev = MoonHour >= Set ? 48.f * FMath::Sin(PI * (MoonHour - Set) / NightLen) : -20.f;
	const float MoonK = FMath::SmoothStep(-3.f, 9.f, MoonElev) * (1.f - SunK);
	Place(MoonLight, FMath::Max(MoonElev, -6.f), 180.f + (MoonHour - Set) * 180.f / NightLen);
	MoonLight->SetIntensity(MoonLux * MoonK);
	MoonLight->SetCastShadows(MoonK > 0.02f);
	// One directional light drives fog / translucency / water: whichever of sun and moon is up (never the fill).
	const bool bSunMain = SunK >= 0.5f;
	if (SunLight->ForwardShadingPriority != (bSunMain ? 2 : 1)) SunLight->SetForwardShadingPriority(bSunMain ? 2 : 1);
	if (MoonLight->ForwardShadingPriority != (bSunMain ? 1 : 2)) MoonLight->SetForwardShadingPriority(bSunMain ? 1 : 2);

	// Low sun: the sky fills the long shadows (otherwise they go black and exposure blows the sunlit patches out).
	if (SkyFill) SkyFill->SetIntensity(1.f + 3.f * SunK * (1.f - Warm));
	if (FillLight)
	{
		const float Golden = SunK * (1.f - Warm);
		FillLight->SetIntensity(SunLux * (0.35f * Golden + 0.06f * (1.f - SunK)));
		FillLight->SetLightColor(FMath::Lerp(FLinearColor(1.f, 0.7f, 0.5f), FLinearColor(0.5f, 0.6f, 1.f), 1.f - SunK));
	}

	// Fog: a light haze by day, thicker and warm at dusk, dense and blue at night (volumetric, so lights glow in it).
	if (Fog)
	{
		const float Dusk = SunK * (1.f - Warm);
		const float Night = 1.f - SunK;
		Fog->SetFogDensity(FogDay + (FogDusk - FogDay) * Dusk + (FogNight - FogDay) * Night);
		Fog->SetFogInscatteringColor(FMath::Lerp(FMath::Lerp(FLinearColor(0.45f, 0.55f, 0.7f), FLinearColor(0.75f, 0.5f, 0.35f), Dusk), FLinearColor(0.08f, 0.11f, 0.2f), Night));
		Fog->SetVolumetricFogExtinctionScale(0.6f + 1.4f * Night);
	}

	// Grade: auto exposure would brighten the night back to day, so the night runs a couple of stops darker,
	// cooler and less saturated; low sun warms everything a little.
	if (Grade)
	{
		FPostProcessSettings& S = Grade->Settings;
		const float Night = 1.f - SunK;
		const float Golden = SunK * (1.f - Warm);
		// Exposure may only adapt within a narrow window, so long dusk shadows can't push it into blowing the
		// sunlit patches out, and night stays night.
		S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = ExposureMinEV;
		S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = ExposureMaxEV;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = -0.5f + NightExposure * Night;
		S.bOverride_SceneColorTint = true;
		S.SceneColorTint = FMath::Lerp(FMath::Lerp(FLinearColor(1.02f, 1.f, 0.97f), FLinearColor(1.08f, 0.95f, 0.85f), Golden), FLinearColor(0.62f, 0.8f, 1.3f), Night);
		S.bOverride_ColorSaturation = true;
		const float Sat = FMath::Lerp(1.f, 0.85f, Night);
		S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	}
}

void ATSSky::Build()
{
	UWorld* W = GetWorld();
	const UTSData& D = UTSData::Get(this);
	const TSJson::FObj Sun = TSJson::Obj(D.World(), TEXT("sun"));
	FRotator SunRot(float(TSJson::Num(Sun, TEXT("pitch"), -36)), float(TSJson::Num(Sun, TEXT("yaw"), 125)), 0.f);
	float InitialLux = float(TSJson::Num(Sun, TEXT("intensityLux"), 9.0));
	FLinearColor SunColor(1.f, 0.96f, 0.9f);
	// Freeze the sun for look tests: -<P>Sun=pitch,yaw,lux[,r,g,b]   (dusk: -7,250,4,1,0.55,0.32)
	FString SunSpec;
	if (TSCmd::Value(TEXT("Sun"), SunSpec, true))
	{
		TArray<FString> P;
		SunSpec.ParseIntoArray(P, TEXT(","));
		if (P.Num() >= 3) { SunRot = FRotator(FCString::Atof(*P[0]), FCString::Atof(*P[1]), 0.f); InitialLux = FCString::Atof(*P[2]); }
		if (P.Num() >= 6) SunColor = FLinearColor(FCString::Atof(*P[3]), FCString::Atof(*P[4]), FCString::Atof(*P[5]));
	}

	// Sun. Spawned deferred so mobility is set before the components register.
	ADirectionalLight* SunActor = W->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(SunRot));
	UDirectionalLightComponent* SunC = CastChecked<UDirectionalLightComponent>(SunActor->GetLightComponent());
	SunC->SetMobility(EComponentMobility::Movable);
	SunC->SetAtmosphereSunLight(true);
	SunC->Intensity = InitialLux;
	SunC->LightSourceAngle = 1.2f;
	SunC->SetLightColor(SunColor);
	SunActor->FinishSpawning(FTransform(SunRot));
	SunLight = CastChecked<UDirectionalLightComponent>(SunActor->GetLightComponent());

	// Day/night: a moon (second atmosphere light, so it lights the night sky too) and the clock.
	const TSJson::FObj DN = TSJson::Obj(D.World(), TEXT("dayNight"));
	bDayCycle = TSJson::Bool(DN, TEXT("enabled"), true) && SunSpec.IsEmpty();
	SecondsPerHour = float(TSJson::Num(DN, TEXT("secondsPerHour"), 30));
	SunLux = SunLight->Intensity;
	MoonLux = float(TSJson::Num(DN, TEXT("moonLux"), 0.6));
	NightExposure = float(TSJson::Num(DN, TEXT("nightExposure"), -2.2));
	ExposureMinEV = float(TSJson::Num(DN, TEXT("exposureMinEV"), 2.0));
	ExposureMaxEV = float(TSJson::Num(DN, TEXT("exposureMaxEV"), 5.0));
	const TSJson::FObj FogCfg = TSJson::Obj(DN, TEXT("fog"));
	FogDay = float(TSJson::Num(FogCfg, TEXT("day"), 0.012));
	FogDusk = float(TSJson::Num(FogCfg, TEXT("dusk"), 0.03));
	FogNight = float(TSJson::Num(FogCfg, TEXT("night"), 0.05));
	bFog = TSJson::Bool(FogCfg, TEXT("enabled"), true);
	int32 FogOverride = -1;
	if (TSCmd::Value(TEXT("Fog"), FogOverride)) bFog = FogOverride != 0;   // look comparisons
	GHour = float(TSJson::Num(DN, TEXT("startHour"), 14));
	TSCmd::Value(TEXT("Hour"), GHour);
	if (bDayCycle)
	{
		ADirectionalLight* MoonActor = W->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-40, 0, 0)));
		MoonLight = CastChecked<UDirectionalLightComponent>(MoonActor->GetLightComponent());
		MoonLight->SetMobility(EComponentMobility::Movable);
		MoonLight->SetAtmosphereSunLight(true);
		MoonLight->SetAtmosphereSunLightIndex(1);
		MoonLight->Intensity = 0.f;
		MoonLight->LightSourceAngle = 0.6f;
		MoonLight->SetLightColor(FLinearColor(0.55f, 0.66f, 1.f));
		MoonActor->FinishSpawning(FTransform(FRotator(-40, 0, 0)));

		ADirectionalLight* FillActor = W->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-75, 110, 0)));
		FillLight = CastChecked<UDirectionalLightComponent>(FillActor->GetLightComponent());
		FillLight->SetMobility(EComponentMobility::Movable);
		FillLight->SetCastShadows(false);
		FillLight->ForwardShadingPriority = 0;
		FillLight->Intensity = 0.f;
		FillActor->FinishSpawning(FTransform(FRotator(-75, 110, 0)));
		UpdateSky();
	}

	W->SpawnActor<ASkyAtmosphere>();

	ASkyLight* Sky = W->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity);
	USkyLightComponent* SkyC = Sky->GetLightComponent();
	SkyC->SetMobility(EComponentMobility::Movable);
	SkyC->bRealTimeCapture = true;
	SkyC->SourceType = SLS_CapturedScene;
	SkyC->Intensity = 1.f;
	Sky->FinishSpawning(FTransform::Identity);
	SkyFill = SkyC;

	if (AVolumetricCloud* Clouds = W->SpawnActor<AVolumetricCloud>())
	{
		if (UMaterialInterface* CloudMat = TSAssets::Load<UMaterialInterface>(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
		{
			Clouds->GetComponentByClass<UVolumetricCloudComponent>()->SetMaterial(CloudMat);
		}
	}

	if (AExponentialHeightFog* FogActor = bFog ? W->SpawnActor<AExponentialHeightFog>(FVector(0, 0, -200), FRotator::ZeroRotator) : nullptr)
	{
		UExponentialHeightFogComponent* F = FogActor->GetComponent();
		Fog = F;
		F->SetFogDensity(0.012f);
		F->SetFogHeightFalloff(0.15f);
		F->SetVolumetricFog(true);
		F->SetVolumetricFogScatteringDistribution(0.5f);
		F->SetVolumetricFogExtinctionScale(0.6f);
	}

	APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>();
	PP->bUnbound = true;
	Grade = PP;
	FPostProcessSettings& S = PP->Settings;
	S.bOverride_BloomIntensity = true;            S.BloomIntensity = 0.45f;
	S.bOverride_VignetteIntensity = true;         S.VignetteIntensity = 0.3f;
	S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.55f;
	S.bOverride_AutoExposureBias = true;          S.AutoExposureBias = -0.5f;
	S.bOverride_ColorSaturation = true;           S.ColorSaturation = FVector4(1.06f, 1.06f, 1.06f, 1.f);
	S.bOverride_SceneColorTint = true;            S.SceneColorTint = FLinearColor(1.02f, 1.0f, 0.97f);

	if (bDayCycle) UpdateSky();
	if (TSLook::Mode() == TSLook::EMode::Flat2D)
	{
		// Flat cards under an orthographic camera: screen-space AO and Lumen just smear dark bands between
		// stacked sprites. Plain direct light + sky light reads cleaner.
		S.bOverride_AmbientOcclusionIntensity = true;      S.AmbientOcclusionIntensity = 0.f;
		S.bOverride_DynamicGlobalIlluminationMethod = true; S.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		S.bOverride_ReflectionMethod = true;               S.ReflectionMethod = EReflectionMethod::None;
		S.bOverride_VignetteIntensity = true;              S.VignetteIntensity = 0.2f;
	}
	if (TSLook::Mode() == TSLook::EMode::HD2D)
	{
		// The HD-2D signature: richer bloom and colour, a heavier vignette.
		// (Depth of field belongs on the gameplay camera, so other cameras aren't blurred by a focus distance meant
		// for the game view.)
		S.bOverride_BloomIntensity = true;            S.BloomIntensity = 0.75f;
		S.bOverride_VignetteIntensity = true;         S.VignetteIntensity = 0.55f;
		S.bOverride_ColorSaturation = true;           S.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.f);
		S.bOverride_ColorContrast = true;             S.ColorContrast = FVector4(1.04f, 1.04f, 1.04f, 1.f);
	}
}
