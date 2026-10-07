#include "TSCameraRig.h"
#include "Tessera.h"
#include "TSData.h"
#include "TSLook.h"
#include "TSFeedback.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

UTSCameraRig::UTSCameraRig()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UTSCameraRig::IsTopDown(const UObject* WorldContext)
{
	return TSJson::Str(TSJson::Obj(UTSData::Get(WorldContext).World(), TEXT("camera")), TEXT("mode")) == TEXT("topdown");
}

void UTSCameraRig::Setup(USpringArmComponent* InBoom, UCameraComponent* InCamera)
{
	Boom = InBoom;
	Camera = InCamera;
	const UTSData& D = UTSData::Get(this);
	const TSJson::FObj Cam = TSJson::Obj(D.World(), TEXT("camera"));
	Boom->TargetArmLength = float(TSJson::Num(Cam, TEXT("armLength"), 430));
	Boom->CameraLagSpeed = float(TSJson::Num(Cam, TEXT("lagSpeed"), 12));
	const TArray<TSharedPtr<FJsonValue>> Off = TSJson::Arr(Cam, TEXT("socketOffset"));
	if (Off.Num() == 3) Boom->SocketOffset = FVector(Off[0]->AsNumber(), Off[1]->AsNumber(), Off[2]->AsNumber());
	Camera->FieldOfView = float(TSJson::Num(Cam, TEXT("fov"), 80));

	// Top-down: a fixed 3/4 view that follows the hero but never turns, so the screen matches the map.
	bTopDown = IsTopDown(this);
	if (!bTopDown) return;
	const TSJson::FObj Td = TSJson::Obj(Cam, TEXT("topdown"));
	Boom->bUsePawnControlRotation = false;
	Boom->SetUsingAbsoluteRotation(true);
	Boom->SetWorldRotation(FRotator(float(TSJson::Num(Td, TEXT("pitch"), -55)), float(TSJson::Num(Td, TEXT("yaw"), -90)), 0.f));
	Boom->bDoCollisionTest = false;   // walls and roofs get cut away instead (ATSWorldBuilder cutaways)
	Boom->SocketOffset = FVector::ZeroVector;
	Boom->TargetArmLength = ZoomTarget = float(TSJson::Num(Td, TEXT("armLength"), 2000));
	Boom->CameraLagSpeed = float(TSJson::Num(Td, TEXT("lagSpeed"), 10));
	MinArm = float(TSJson::Num(Td, TEXT("minArm"), 1100));
	MaxArm = float(TSJson::Num(Td, TEXT("maxArm"), 3000));
	ZoomStep = float(TSJson::Num(Td, TEXT("zoomStep"), 220));
	Camera->FieldOfView = float(TSJson::Num(Td, TEXT("fov"), 50));

	// 2D looks: HD-2D frames lower and tighter; Flat 2D looks straight down through an orthographic lens.
	const TSJson::FObj L2 = TSJson::Obj(D.World(), TEXT("looks2d"));
	Boom->SetWorldRotation(TSLook::CameraRotation());
	if (TSLook::Mode() == TSLook::EMode::HD2D)
	{
		const TSJson::FObj H = TSJson::Obj(L2, TEXT("hd2dCamera"));
		Boom->TargetArmLength = ZoomTarget = float(TSJson::Num(H, TEXT("armLength"), 3000));
		MinArm = float(TSJson::Num(H, TEXT("minArm"), 2400));
		MaxArm = float(TSJson::Num(H, TEXT("maxArm"), 3800));
		Camera->FieldOfView = float(TSJson::Num(H, TEXT("fov"), 30));
		// Tilt-shift: focus on the hero, so the top and bottom of the screen go soft (a big virtual sensor makes
		// the depth of field shallow enough to show at this distance). Off unless hd2dCamera.tiltShift.
		FPostProcessSettings& PP = Camera->PostProcessSettings;
		PP.bOverride_DepthOfFieldFocalDistance = TSJson::Bool(H, TEXT("tiltShift"), false) && !TSCmd::Has(TEXT("NoDOF"));
		PP.DepthOfFieldFocalDistance = Boom->TargetArmLength;
		PP.bOverride_DepthOfFieldFstop = true;         PP.DepthOfFieldFstop = float(TSJson::Num(H, TEXT("focusFstop"), 0.5));
		PP.bOverride_DepthOfFieldMinFstop = true;      PP.DepthOfFieldMinFstop = 0.f;
		PP.bOverride_DepthOfFieldSensorWidth = true;   PP.DepthOfFieldSensorWidth = float(TSJson::Num(H, TEXT("sensorWidth"), 400));
		Camera->PostProcessBlendWeight = 1.f;
	}
	else if (TSLook::Mode() == TSLook::EMode::Flat2D)
	{
		const TSJson::FObj Fc = TSJson::Obj(L2, TEXT("flatCamera"));
		Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
		Camera->SetOrthoWidth(ZoomTarget = float(TSJson::Num(Fc, TEXT("orthoWidth"), 3800)));
		MinArm = float(TSJson::Num(Fc, TEXT("minWidth"), 2800));
		MaxArm = float(TSJson::Num(Fc, TEXT("maxWidth"), 5200));
		ZoomStep = 300.f;
		Boom->TargetArmLength = 4000.f;
	}
}

void UTSCameraRig::Zoom(float Wheel)
{
	if (bTopDown) ZoomTarget = FMath::Clamp(ZoomTarget - Wheel * ZoomStep, MinArm, MaxArm);
}

void UTSCameraRig::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	if (!Boom || !Camera) return;
	// Shake from hits.
	const UTSFeedback* Feedback = UTSFeedback::Get(this);
	const float Shake = Feedback ? Feedback->ShakeAmount : 0.f;
	Camera->SetRelativeLocation(Shake > 0.2f ? FVector(0, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * Shake : FVector::ZeroVector);
	if (!bTopDown) return;
	if (TSLook::Mode() == TSLook::EMode::Flat2D) Camera->SetOrthoWidth(FMath::FInterpTo(Camera->OrthoWidth, ZoomTarget, Dt, 8.f));
	else Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, ZoomTarget, Dt, 8.f);
	Camera->PostProcessSettings.DepthOfFieldFocalDistance = Boom->TargetArmLength;   // keep the hero in focus while zooming
}
