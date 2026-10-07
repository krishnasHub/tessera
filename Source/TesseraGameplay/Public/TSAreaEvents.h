#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TSAreaEvents.generated.h"

DECLARE_MULTICAST_DELEGATE_FourParams(FTSAreaStatus, const FVector& /*Center*/, float /*Radius*/, FName /*Tag*/, float /*Duration*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FTSAreaPush, const FVector& /*Center*/, float /*Radius*/);

/**
 * Area effects announced to the world, for things that aren't characters (ambient animals, props, water...):
 * an area ability that applies a tag (a frost nova's "Frozen") broadcasts OnStatus, a barrier going up broadcasts
 * OnPush (whatever is inside is thrown out), and whatever the game has listening decides what that means for it.
 * Characters handle these themselves; scenery (trees, walls) isn't meant to react.
 */
UCLASS()
class TESSERAGAMEPLAY_API UTSAreaEvents : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTSAreaEvents* Get(const UObject* WorldContext);
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

	/** Tag applied for Duration seconds within Radius (flat) of Center. */
	FTSAreaStatus OnStatus;
	/** Everything within Radius (flat) of Center is thrown outward, clear of it. */
	FTSAreaPush OnPush;
};
