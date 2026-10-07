#include "TSAreaEvents.h"
#include "Engine/World.h"

UTSAreaEvents* UTSAreaEvents::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UTSAreaEvents>() : nullptr;
}
