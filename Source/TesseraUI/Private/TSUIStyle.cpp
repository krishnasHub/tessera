#include "TSUIStyle.h"

#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

FTSUIStyle& FTSUIStyle::Get()
{
	static FTSUIStyle Style;
	return Style;
}

namespace TSUI
{
	FSlateFontInfo Font(int32 Size, const TCHAR* Style) { return FCoreStyle::GetDefaultFontStyle(Style, Size); }
	const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }

	TSharedRef<SWidget> Bar(float Width, float Height, TAttribute<FSlateColor> Color, TFunction<float()> Frac, TFunction<FString()> Label)
	{
		return SNew(SBox).WidthOverride(Width).HeightOverride(Height)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[ SNew(SImage).Image(White()).ColorAndOpacity(FLinearColor(0, 0, 0, 0.62f)) ]
			+ SOverlay::Slot().HAlign(HAlign_Left)
			[
				SNew(SBox).WidthOverride_Lambda([Width, Frac]() { return FOptionalSize(Width * FMath::Clamp(Frac(), 0.f, 1.f)); })
				[ SNew(SImage).Image(White()).ColorAndOpacity(Color) ]
			]
			+ SOverlay::Slot().VAlign(VAlign_Center).Padding(8, 0)
			[
				SNew(STextBlock).Font(Font(10, TEXT("Bold"))).ShadowOffset(FVector2D(1, 1))
				.Text_Lambda([Label]() { return Label ? Text(Label()) : FText::GetEmpty(); })
			]
		];
	}

	void SetTextureBrush(FSlateBrush& Brush, UTexture2D* Texture, const FVector2D& Size)
	{
		Brush = FSlateBrush();
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		if (!Texture) return;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size.IsZero() ? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : Size;
	}
}
