#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TSCameraRig.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * The hero's camera, set up from data on a spring arm + camera the character owns.
 *
 *   <world>.camera.mode       "topdown": a fixed 3/4 view that follows the hero but never turns (the screen matches
 *                             the map; the cursor aims). Anything else: an over-the-shoulder boom aimed with the mouse.
 *   <world>.camera.topdown    { pitch, yaw, armLength, minArm, maxArm, zoomStep, fov, lagSpeed }
 *   <world>.camera            { armLength, lagSpeed, socketOffset: [x, y, z], fov }   (over the shoulder)
 *   <world>.looks2d.hd2dCamera { armLength, minArm, maxArm, fov, tiltShift, focusFstop, sensorWidth }   (HD-2D look)
 *   <world>.looks2d.flatCamera { orthoWidth, minWidth, maxWidth }   (flat 2D look: orthographic, straight down)
 *
 * Ticks the zoom (mouse wheel, eased, within limits), keeps the tilt-shift focus on the hero, and shakes with
 * UTSFeedback's shake. -<Prefix>NoDOF turns the tilt-shift off.
 */
UCLASS()
class TESSERAHERO_API UTSCameraRig : public UActorComponent
{
	GENERATED_BODY()

public:
	UTSCameraRig();

	static bool IsTopDown(const UObject* WorldContext);

	void Setup(USpringArmComponent* InBoom, UCameraComponent* InCamera);
	bool IsTopDown() const { return bTopDown; }
	/** Mouse wheel: closer (positive) or further (negative), within the look's limits. */
	void Zoom(float Wheel);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY() TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	bool bTopDown = false;
	float ZoomTarget = 0.f, MinArm = 0.f, MaxArm = 0.f, ZoomStep = 0.f;
};
