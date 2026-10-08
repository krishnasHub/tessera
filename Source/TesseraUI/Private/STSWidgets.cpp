#include "STSWidgets.h"
#include "TSUIStyle.h"
#include "TSAssets.h"
#include "TSData.h"
#include "TSSky.h"
#include "TSFeedback.h"
#include "TSAbilities.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

// =============================================================================================
// Cursor
// =============================================================================================

void STSCursor::Construct(const FArguments& Args)
{
	IconFn = Args._IconFn;
	Centred = Args._Centred;
	Size = Args._Size;
	SetVisibility(EVisibility::HitTestInvisible);
	for (const FName& N : Args._Icons)
	{
		UTexture2D* Tex = TSAssets::Load<UTexture2D>(TSAssets::ObjPath(Args._IconFolder, Args._IconPrefix + N.ToString()));
		if (!Tex) continue;
		Keep.Add(TStrongObjectPtr<UTexture2D>(Tex));
		FSlateBrush& B = Brushes.Add(N);
		B.SetResourceObject(Tex);
		B.ImageSize = FVector2D(Size, Size);
		B.DrawAs = ESlateBrushDrawType::Image;
	}
}

int32 STSCursor::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	if (!IconFn || !FSlateApplication::IsInitialized()) return Layer;
	const FName Icon = IconFn();
	const FSlateBrush* B = Brushes.Find(Icon);
	if (!B) return Layer;
	const FVector2D At = G.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos()) - (Centred.Contains(Icon) ? FVector2D(Size * 0.5f) : FVector2D(2, 2));
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(Size, Size), FSlateLayoutTransform(At)), B, ESlateDrawEffect::None, FLinearColor::White);
	return Layer + 1;
}

// =============================================================================================
// Night shade
// =============================================================================================

void STSNightShade::Construct(const FArguments& Args)
{
	World = Args._World;
	if (UMaterialInterface* Base = TSAssets::Get<UMaterialInterface>(World.Get(), TEXT("nightShade")))
		Mat.Reset(UMaterialInstanceDynamic::Create(Base, GetTransientPackage()));
	Brush.SetResourceObject(Mat.Get());
	Brush.DrawAs = ESlateBrushDrawType::Image;
}

void STSNightShade::Tick(const FGeometry& G, const double Time, const float Dt)
{
	SLeafWidget::Tick(G, Time, Dt);
	Strength = ATSSky::ShadeStrength();   // (a hero with night eyes sees a dim grey beyond their sight, not black)
	APlayerController* PC = World.IsValid() ? World->GetFirstPlayerController() : nullptr;
	const APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!Mat || !P || Strength < 0.001f || !GEngine || !GEngine->GameViewport) return;
	FVector2D Vp;
	GEngine->GameViewport->GetViewportSize(Vp);
	if (Vp.X < 1.f || Vp.Y < 1.f) return;

	// A world circle (centre, radius) as a screen ellipse in 0..1 UV: project the centre and a point east and north.
	const float Z = P->GetActorLocation().Z;
	auto Ellipse = [&](const FVector& At, float R) -> FLinearColor
	{
		FVector2D S0, SX, SY;
		const FVector C(At.X, At.Y, Z);
		if (!PC->ProjectWorldLocationToScreen(C, S0) || !PC->ProjectWorldLocationToScreen(C + FVector(R, 0, 0), SX) || !PC->ProjectWorldLocationToScreen(C + FVector(0, -R, 0), SY))
			return FLinearColor(0, 0, 0, 0);
		return FLinearColor(S0.X / Vp.X, S0.Y / Vp.Y, FMath::Max(FVector2D::Distance(S0, SX) / Vp.X, 0.001f), FMath::Max(FVector2D::Distance(S0, SY) / Vp.Y, 0.001f));
	};
	Mat->SetScalarParameterValue(TEXT("Night"), Strength);
	Mat->SetVectorParameterValue(TEXT("Hero"), Ellipse(P->GetActorLocation(), ATSSky::HeroSight()));
	const FVector At = P->GetActorLocation();
	TArray<FVector> Lights = ATSSky::NightLights();
	Lights.Sort([&At](const FVector& A, const FVector& B) { return FVector::DistSquared2D(A, At) < FVector::DistSquared2D(B, At); });
	for (int32 I = 0; I < 8; ++I)
		Mat->SetVectorParameterValue(*FString::Printf(TEXT("Light%d"), I), Lights.IsValidIndex(I) ? Ellipse(FVector(Lights[I].X, Lights[I].Y, 0), Lights[I].Z) : FLinearColor(0, 0, 0, 0));
}

int32 STSNightShade::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	if (Strength >= 0.001f && Mat) FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), &Brush, ESlateDrawEffect::None, FLinearColor::White);
	return Layer + 1;
}

// =============================================================================================
// Toasts
// =============================================================================================

void STSToasts::Construct(const FArguments& Args)
{
	TWeakObjectPtr<UWorld> W = Args._World;
	auto Toast = [W](int32 I) -> const FTSToast* { const UTSFeedback* F = W.IsValid() ? UTSFeedback::Get(W.Get()) : nullptr; return F && F->Toasts.IsValidIndex(I) ? &F->Toasts[I] : nullptr; };
	const UTSFeedback* F = W.IsValid() ? UTSFeedback::Get(W.Get()) : nullptr;
	auto List = SNew(SVerticalBox);
	for (int32 I = 0; I < (F ? F->MaxToasts : 5); ++I)
	{
		List->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 3)
		[
			SNew(SBorder).BorderImage(TSUI::White()).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.6f)).Padding(FMargin(14, 5))
			.Visibility_Lambda([Toast, I]() { return Toast(I) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(STextBlock).Font(TSUI::Font(13))
				.Text_Lambda([Toast, I]() { const FTSToast* T = Toast(I); return T ? TSUI::Text(T->Text) : FText::GetEmpty(); })
				.ColorAndOpacity_Lambda([W, Toast, I]() {
					const FTSToast* T = Toast(I);
					if (!T) return FSlateColor(FLinearColor::White);
					FLinearColor C = T->Color;
					C.A = FMath::Clamp((UTSFeedback::Get(W.Get())->ToastLife - T->Age) * 2.f, 0.f, 1.f);
					return FSlateColor(C); })
			]
		];
	}
	ChildSlot[ List ];
}

// =============================================================================================
// Ability picker
// =============================================================================================

TSJson::FObj STSAbilityPicker::Def(int32 I) const
{
	const UTSAbilityComponent* A = Abilities ? Abilities() : nullptr;
	return A && A->Ids.IsValidIndex(I) ? A->Def(A->Ids[I]) : nullptr;
}

void STSAbilityPicker::Construct(const FArguments& Args)
{
	Abilities = Args._Abilities;
	Slot = Args._Slot;
	CostText = Args._CostText;
	const UObject* Ctx = Args._World.Get();
	const FTSUIStyle& St = FTSUIStyle::Get();
	const FString CooldownWord = TSText::Get(Ctx, TEXT("cooldown"), TEXT("cooldown"));
	auto Cards = SNew(SHorizontalBox);
	for (int32 I = 0; I < Args._Cards; ++I)
	{
		auto Picked = [this, I]() { return Slot && Slot() == I; };
		Cards->AddSlot().AutoWidth().Padding(6, 0)
		[
			SNew(SBorder).BorderImage(TSUI::White()).Padding(3)
			.BorderBackgroundColor_Lambda([Picked]() { return FSlateColor(Picked() ? FTSUIStyle::Get().Accent : FLinearColor(1, 1, 1, 0.12f)); })
			[
				SNew(SBox).WidthOverride_Lambda([Picked]() { return FOptionalSize(Picked() ? 132.f : 112.f); })
				.HeightOverride_Lambda([Picked]() { return FOptionalSize(Picked() ? 96.f : 80.f); })
				[
					SNew(SBorder).BorderImage(TSUI::White()).BorderBackgroundColor(FLinearColor(0.05f, 0.06f, 0.08f, 0.92f)).Padding(6)
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Justification(ETextJustify::Center).AutoWrapText(true)
						.Font_Lambda([Picked]() { return TSUI::Font(Picked() ? 14 : 11, TEXT("Bold")); })
						.Text_Lambda([this, I]() {
							const UTSAbilityComponent* A = Abilities ? Abilities() : nullptr;
							const TSJson::FObj D = Def(I);
							if (!A || !D) return FText::GetEmpty();
							const FString Id = A->Ids[I];
							if (!A->Unlocked(Id)) return TSUI::Text(FString::Printf(TEXT("%d  %s\nLv %d"), I + 1, *TSJson::Str(D, TEXT("name")), int32(TSJson::Num(D, TEXT("unlockLevel"), 1))));
							const float Cd = A->Cooldowns.FindRef(Id);
							return TSUI::Text(FString::Printf(TEXT("%d  %s%s"), I + 1, *TSJson::Str(D, TEXT("name")), Cd > 0.f ? *FString::Printf(TEXT("\n%.1fs"), Cd) : TEXT(""))); })
						.ColorAndOpacity_Lambda([this, I]() {
							const UTSAbilityComponent* A = Abilities ? Abilities() : nullptr;
							const TSJson::FObj D = Def(I);
							if (!A || !D || !A->Unlocked(A->Ids[I])) return FSlateColor(FLinearColor(0.45f, 0.45f, 0.48f));
							return FSlateColor(TSJson::Color(TSJson::Str(D, TEXT("color")))); })
					]
				]
			]
		];
	}
	ChildSlot
	[
		SNew(SBorder).BorderImage(TSUI::White()).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)).Padding(FMargin(28, 18))
		.Visibility_Lambda([this]() { return Slot && Slot() >= 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)
			[ SNew(STextBlock).Font(TSUI::Font(12, TEXT("Bold"))).ColorAndOpacity(St.Muted).Text(TSUI::Text(TSText::Get(Ctx, TEXT("pickerTitle"), TEXT("CHOOSE AN ABILITY")))) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Cards ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 2)
			[
				SNew(STextBlock).Font(TSUI::Font(16, TEXT("Bold"))).ColorAndOpacity(St.Accent)
				.Text_Lambda([this, CooldownWord]() {
					const TSJson::FObj D = Slot ? Def(Slot()) : nullptr;
					if (!D) return FText::GetEmpty();
					const FString Cost = CostText ? CostText(D) : FString();
					return TSUI::Text(TSJson::Str(D, TEXT("name")) + (Cost.IsEmpty() ? FString() : TEXT("   ·   ") + Cost)
						+ FString::Printf(TEXT("   ·   %.0fs %s"), TSJson::Num(D, TEXT("cooldown"), 0), *CooldownWord)); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(560)
				[
					SNew(STextBlock).Font(TSUI::Font(12)).AutoWrapText(true).Justification(ETextJustify::Center).ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.88f))
					.Text_Lambda([this]() { const TSJson::FObj D = Slot ? Def(Slot()) : nullptr; return D ? TSUI::Text(TSJson::Str(D, TEXT("desc"))) : FText::GetEmpty(); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12, 0, 0)
			[ SNew(STextBlock).Font(TSUI::Font(10)).ColorAndOpacity(St.Muted).Text(TSUI::Text(TSText::Get(Ctx, TEXT("pickerHint"),
				TEXT("scroll to change   ·   release or click to cast   ·   right-click to cancel")))) ]
		]
	];
}
