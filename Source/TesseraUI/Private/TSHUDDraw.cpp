#include "TSHUDDraw.h"
#include "TSCharacter.h"
#include "TSFeedback.h"
#include "TSPerception.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace TSHUDDraw
{
	void Text(UCanvas* Canvas, const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter, bool bShadow)
	{
		UFont* Font = GEngine->GetLargeFont();
		float W = 0, H = 0;
		Canvas->TextSize(Font, S, W, H, Scale, Scale);
		if (bCenter) X -= W * 0.5f;
		FCanvasTextItem Item(FVector2D(X, Y - H * 0.5f), FText::FromString(S), Font, C);
		Item.Scale = FVector2D(Scale, Scale);
		if (bShadow) Item.EnableShadow(FLinearColor(0, 0, 0, 0.85f), FVector2D(1.5f, 1.5f));
		Canvas->DrawItem(Item);
	}

	void Bar(UCanvas* Canvas, float X, float Y, float W, float H, float Frac, const FLinearColor& C)
	{
		FCanvasTileItem Back(FVector2D(X - 1, Y - 1), FVector2D(W + 2, H + 2), FLinearColor(0, 0, 0, 0.7f));
		Back.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Back);
		FCanvasTileItem Fill(FVector2D(X, Y), FVector2D(W * FMath::Clamp(Frac, 0.f, 1.f), H), C);
		Fill.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Fill);
	}

	void Floaters(UCanvas* Canvas, const UObject* WorldContext, float Scale)
	{
		const UTSFeedback* F = UTSFeedback::Get(WorldContext);
		if (!F) return;
		for (const FTSFloater& Fl : F->Floaters)
		{
			const FVector S = Canvas->Project(Fl.World, false);
			if (S.Z <= 0.f) continue;
			FLinearColor C = Fl.Color;
			C.A = FMath::Clamp(2.f * (1.f - Fl.Age / Fl.Life), 0.f, 1.f);
			Text(Canvas, Fl.Text, S.X, S.Y, C, 1.1f * Fl.Size * Scale);
		}
	}

	void GroundRing(UCanvas* Canvas, const FVector& At, float Radius, const FLinearColor& C, float Thickness)
	{
		FVector2D Prev;
		for (int32 I = 0; I <= 20; ++I)
		{
			const float A = I * UE_TWO_PI / 20.f;
			const FVector S = Canvas->Project(At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius + FVector(0, 0, 4), false);
			const FVector2D Cur(S.X, S.Y);
			if (I > 0 && S.Z > 0)
			{
				FCanvasLineItem L(Prev, Cur);
				L.SetColor(C);
				L.LineThickness = Thickness;
				Canvas->DrawItem(L);
			}
			Prev = Cur;
		}
	}

	void ThreatArrows(UCanvas* Canvas, const APlayerController* PC, const ATSCharacter* Hero, float Scale)
	{
		if (!PC || !Hero || !PC->PlayerCameraManager) return;
		const float Now = Hero->GetWorld()->GetRealTimeSeconds();
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
		for (const ATSCharacter* E : TSPerception::Hunters(Hero, TSPerception::ThreatRange(Hero)))
		{
			const FVector S = Canvas->Project(E->Chest(), false);
			const bool bOnScreen = S.Z > 0 && S.X > 0 && S.Y > 0 && S.X < Canvas->ClipX && S.Y < Canvas->ClipY;
			if (bOnScreen) continue;
			// Direction relative to the camera, mapped onto the screen.
			const FRotator CamRot = PC->PlayerCameraManager->GetCameraRotation();
			const FVector Local = FRotator(0, CamRot.Yaw, 0).UnrotateVector(E->GetActorLocation() - Hero->GetActorLocation());
			const FVector2D Dir = FVector2D(Local.Y, -Local.X).GetSafeNormal();
			const FVector2D At = Center + Dir * FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.42f;
			const bool bWinding = E->IsWindingUp();
			FLinearColor C = bWinding ? FLinearColor(1.f, 0.15f, 0.15f) : FLinearColor(1.f, 0.55f, 0.4f);
			C.A = bWinding ? 0.7f + 0.3f * FMath::Sin(Now * 20.f) : 0.6f;
			const FVector2D Perp(-Dir.Y, Dir.X);
			const float L = 26.f * Scale, Wd = 14.f * Scale;
			FCanvasTriangleItem Tri(At + Dir * L, At + Perp * Wd, At - Perp * Wd, GWhiteTexture);
			Tri.SetColor(C);
			Tri.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tri);
		}
	}
}
