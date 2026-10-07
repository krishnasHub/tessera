#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TSCharacterEvents.generated.h"

class ATSCharacter;

DECLARE_MULTICAST_DELEGATE_TwoParams(FTSDiedEvent, ATSCharacter* /*Who*/, AActor* /*Killer*/);

/**
 * What happens to characters, announced for the game to act on (a ghost rising, a bounty paid, a quest moving on).
 * Tessera only announces; the reaction is the game's own code.
 *
 *   OnDied   a character died (ATSCharacter::Die), and who killed it (may be null: poison, a fall...)
 */
UCLASS()
class TESSERAGAMEPLAY_API UTSCharacterEvents : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTSCharacterEvents* Get(const UObject* WorldContext);
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }

	FTSDiedEvent OnDied;
};
