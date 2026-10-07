#include "TSFeedback.h"
#include "Tessera.h"
#include "Engine/World.h"

UTSFeedback* UTSFeedback::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UTSFeedback>() : nullptr;
}

void UTSFeedback::Float(const FVector& At, const FString& Text, const FLinearColor& Color, float Size)
{
	FTSFloater F;
	F.World = At + FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), 0);
	F.Text = Text; F.Color = Color; F.Size = Size;
	Floaters.Add(F);
}

void UTSFeedback::Toast(const FString& Text, const FLinearColor& Color)
{
	Toasts.Add({ Text, Color, 0.f });
	if (Toasts.Num() > MaxToasts) Toasts.RemoveAt(0);
	UE_LOG(LogTessera, Display, TEXT("[toast] %s"), *Text);
}

void UTSFeedback::Tick(float Dt)
{
	for (FTSToast& T : Toasts) T.Age += Dt;
	Toasts.RemoveAll([this](const FTSToast& T) { return T.Age > ToastLife; });
	if (GetWorld()->IsPaused()) return;
	for (FTSFloater& F : Floaters) { F.Age += Dt; F.World.Z += 60.f * Dt; }
	Floaters.RemoveAll([](const FTSFloater& F) { return F.Age > F.Life; });
	ShakeAmount *= FMath::Pow(0.002f, Dt);
}
