#include "TSCharacterEvents.h"
#include "Engine/World.h"

UTSCharacterEvents* UTSCharacterEvents::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UTSCharacterEvents>() : nullptr;
}
