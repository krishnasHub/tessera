#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "Misc/Attribute.h"

struct FSlateBrush;
class SWidget;

/** The colours Tessera's widgets use. A game sets its own once at startup: FTSUIStyle::Get().Accent = ... */
struct TESSERAUI_API FTSUIStyle
{
	FLinearColor Panel = FLinearColor(0.055f, 0.06f, 0.075f, 0.93f);      // panel / dialogue box backgrounds
	FLinearColor Accent = FLinearColor(1.f, 0.83f, 0.3f);                 // titles, highlights, shared dialogue verbs
	FLinearColor Muted = FLinearColor(0.62f, 0.64f, 0.68f);               // hints, numbers, secondary text
	FLinearColor Text = FLinearColor(0.95f, 0.95f, 0.93f);                // button labels
	FLinearColor Body = FLinearColor(0.92f, 0.91f, 0.89f);                // dialogue lines
	// Menu items (FTSChoose): resting, highlighted (hover, arrows, wheel), just chosen.
	FLinearColor Idle = FLinearColor(0.12f, 0.13f, 0.17f);
	FLinearColor Highlighted = FLinearColor(0.09f, 0.2f, 0.46f);
	FLinearColor Chosen = FLinearColor(0.9f, 0.56f, 0.06f);

	static FTSUIStyle& Get();
};

/** Small helpers for building Slate UI in code. */
namespace TSUI
{
	TESSERAUI_API FSlateFontInfo Font(int32 Size, const TCHAR* Style = TEXT("Regular"));
	TESSERAUI_API const FSlateBrush* White();
	inline FText Text(const FString& S) { return FText::FromString(S); }
	/** A horizontal bar: dark track, coloured fill (fraction from a lambda), optional label. */
	TESSERAUI_API TSharedRef<SWidget> Bar(float Width, float Height, TAttribute<FSlateColor> Color, TFunction<float()> Frac, TFunction<FString()> Label = nullptr);
	/** A texture as a brush (null -> a brush that draws nothing); zero Size = the texture's size. Brushes don't keep
	 *  their textures alive: hold a TStrongObjectPtr to it. */
	TESSERAUI_API void SetTextureBrush(FSlateBrush& Brush, class UTexture2D* Texture, const FVector2D& Size = FVector2D::ZeroVector);
}
