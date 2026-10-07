#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "TSPoseMesh.generated.h"

/**
 * Procedural pose layer: named bone poses blended over a hidden animated mesh (until real animations exist).
 *
 * The animated skeletal mesh stays hidden and keeps running the combat anim blueprint (locomotion,
 * montages, notifies). This poseable copy is what you see: every frame, after animation has finished,
 * it copies the animated pose, then eases selected bones toward the active poses — the left arm into a
 * shield guard, both arms into a bow draw, the staff arm into a cast. Weapons attach to this mesh, so they
 * follow the posed arms.
 *
 * Several poses can be active at once (e.g. "bow_aim" for the bow arm + "bow_draw" for the string arm);
 * later ones win where they share bones. Each bone eases on its own, so switching pose -> pose (draw ->
 * release, cast -> kick) is smooth, and dropping a pose eases back to pure animation.
 *
 * Targets are directions in the character's own space (X forward, Y right, Z up): the bone's X axis and a
 * hint for its Z axis. Note the UE5 mannequin's left-arm bones point toward the hand, right-arm bones point
 * back toward the shoulder (measured by logging bone axes), so right-arm targets are given reversed.
 */
UCLASS()
class TESSERAGAMEPLAY_API UTSPoseMesh : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UTSPoseMesh();

	struct FBoneTarget { FName Bone; FVector AxisX; FVector AxisZHint; };

	/** Named poses, each a list of bone targets. */
	TMap<FName, TArray<FBoneTarget>> Poses;
	/** Poses to show this frame (empty = pure animation). */
	TArray<FName> ActivePoses;
	/** How fast bones ease toward their target (1/s). */
	float BlendSpeed = 18.f;

	TWeakObjectPtr<USkeletalMeshComponent> Source;
	/** Runs right after the pose is applied each frame (for things that must stick to posed bones, e.g. a drawn bowstring). */
	TFunction<void()> OnPosed;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TMap<FName, FQuat> Current;   // character-space rotation currently shown, per posed bone
};
