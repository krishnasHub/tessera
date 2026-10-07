#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSFX.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

/**
 * Short-lived visual effects built from engine shapes and the game's materials (<world>.assets.glow / telegraph):
 *   Ring      expanding ground ring (area spells, level-up, heals)
 *   Bolt      glowing segments between points (chain lightning, a beam of words)
 *   Burst     brief glow sphere + light (impacts, teleports)
 *   Sphere    a see-through sphere that swells to its radius around a point, then fades (a frost nova)
 *   Stain     a coloured disc on the ground that lingers, then fades (frozen ground, scorch, poison pool)
 *   Smoke     particle smoke (<world>.assets.smoke), switched off after its duration
 */
UCLASS()
class TESSERAGAMEPLAY_API ATSFX : public AActor
{
	GENERATED_BODY()

public:
	ATSFX();
	virtual void Tick(float DeltaSeconds) override;

	static void Ring(UWorld* W, const FVector& At, float Radius, const FLinearColor& Color, float Life = 0.4f);
	static void Bolt(UWorld* W, const TArray<FVector>& Points, const FLinearColor& Color, float Life = 0.25f);
	static void Burst(UWorld* W, const FVector& At, float Radius, const FLinearColor& Color, float Life = 0.3f);
	static void Sphere(UWorld* W, const FVector& At, float Radius, const FLinearColor& Color, float Life = 0.7f);
	static void Stain(UWorld* W, const FVector& At, float Radius, const FLinearColor& Color, float Life = 3.f);
	/** A smoke cloud that lasts Duration seconds, then stops puffing and drifts away (the emitter loops otherwise). */
	static void Smoke(UWorld* W, const FVector& At, float Radius, float Duration);

private:
	enum class EKind : uint8 { Ring, Bolt, Burst, Smoke, Sphere, Stain } Kind = EKind::Ring;
	float Age = 0.f, Life = 0.4f, Radius = 100.f;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	float LightBase = 0.f;
	UPROPERTY() TArray<TObjectPtr<class UParticleSystemComponent>> Emitters;
	float SmokeFade = 2.f;   // seconds the last puffs get to drift away after the cloud stops
	bool bSmokeStopped = false;

	static ATSFX* Make(UWorld* W, const FVector& At, EKind Kind, float Life);
	UStaticMeshComponent* AddPart(const TCHAR* Shape, const TCHAR* MaterialKey, const FLinearColor& Color, float Intensity);
};
