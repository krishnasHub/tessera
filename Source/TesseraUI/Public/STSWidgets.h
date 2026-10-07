#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "TSJson.h"

class UWorld;
class UTexture2D;
class UMaterialInstanceDynamic;
class UTSAbilityComponent;

/**
 * The game's own mouse cursor, drawn above all the UI: a picture per icon name, chosen every frame by IconFn
 * (NAME_None hides it: menus, cutscenes). Icon pictures are <IconFolder>/<IconPrefix><name> textures; icons in
 * Centred are centred on the point, the rest have their tip there (top-left).
 */
class TESSERAUI_API STSCursor : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STSCursor) : _Size(40.f) {}
		SLATE_ARGUMENT(FString, IconFolder)
		SLATE_ARGUMENT(FString, IconPrefix)
		SLATE_ARGUMENT(TArray<FName>, Icons)
		SLATE_ARGUMENT(TArray<FName>, Centred)
		SLATE_ARGUMENT(float, Size)
		SLATE_ARGUMENT(TFunction<FName()>, IconFn)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1, 1); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	TFunction<FName()> IconFn;
	TArray<FName> Centred;
	float Size = 40.f;
	TMap<FName, FSlateBrush> Brushes;
	TArray<TStrongObjectPtr<UTexture2D>> Keep;
};

/**
 * Deep night (drawn under the HUD): near-black everywhere except around the hero and around fires / torches,
 * which keep their own pools of light (ATSSky::Darkness, HeroSight, NightLights). A UI material
 * (<world>.assets.nightShade: scalar Night, vectors Hero and Light0..7 as screen ellipses) fed the world circles
 * projected through the camera, so its perspective is right.
 */
class TESSERAUI_API STSNightShade : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STSNightShade) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1, 1); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& G, const double Time, const float Dt) override;

private:
	TWeakObjectPtr<UWorld> World;
	TStrongObjectPtr<UMaterialInstanceDynamic> Mat;
	FSlateBrush Brush;
	float Strength = 0.f;
};

/** UTSFeedback's toasts: a stack of short messages, each fading out at the end of its life. */
class TESSERAUI_API STSToasts : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSToasts) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
};

/**
 * The ability picker (UTSHeroControl: modifier + wheel): the abilities as cards over a translucent backdrop, the
 * highlighted one bigger and in the accent colour, its name, cost, cooldown and description below. Collapsed while
 * Slot() is -1. Each ability's "name", "desc", "color", "unlockLevel", "cooldown". Words: <world>.text
 * { pickerTitle, pickerHint, cooldown }.
 */
class TESSERAUI_API STSAbilityPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STSAbilityPicker) : _Cards(4) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
		SLATE_ARGUMENT(int32, Cards)
		SLATE_ARGUMENT(TFunction<UTSAbilityComponent*()>, Abilities)
		SLATE_ARGUMENT(TFunction<int32()>, Slot)
		/** The cost line for an ability ("20 energy"); "" for none. */
		SLATE_ARGUMENT(TFunction<FString(const TSJson::FObj&)>, CostText)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);

private:
	TSJson::FObj Def(int32 I) const;
	TFunction<UTSAbilityComponent*()> Abilities;
	TFunction<int32()> Slot;
	TFunction<FString(const TSJson::FObj&)> CostText;
};
