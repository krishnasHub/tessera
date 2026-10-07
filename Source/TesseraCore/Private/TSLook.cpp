#include "TSLook.h"
#include "TSData.h"
#include "TSAssets.h"
#include "Tessera.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	TSLook::EMode GMode = TSLook::EMode::Mesh3D;
	TSJson::FObj GCfg;            // <world>.looks2d
	TSJson::FObj GTopDown;        // <world>.camera.topdown
	TMap<FString, TStrongObjectPtr<UMaterialInstanceDynamic>> GCache;

	FString MatPath(const TCHAR* Which) { return TSJson::Str(TSJson::Obj(GCfg, TEXT("materials")), Which); }
	UTexture2D* PixelTex(const FString& Name) { return TSAssets::Load<UTexture2D>(TSAssets::ObjPath(TSJson::Str(GCfg, TEXT("textureFolder")), Name)); }
}

void TSLook::Init(const UObject* WorldContext)
{
	const UTSData& D = UTSData::Get(WorldContext);
	GCfg = TSJson::Obj(D.World(), TEXT("looks2d"));
	GTopDown = TSJson::Obj(TSJson::Obj(D.World(), TEXT("camera")), TEXT("topdown"));
	FString Name = TSJson::Str(D.World(), TEXT("look"), TEXT("mesh3d"));
	TSCmd::Value(TEXT("Look"), Name);
	GMode = Name == TEXT("hd2d") ? EMode::HD2D : Name == TEXT("flat2d") ? EMode::Flat2D : EMode::Mesh3D;
	GCache.Reset();
	UE_LOG(LogTessera, Display, TEXT("Look: %s"), ModeName());
}

TSLook::EMode TSLook::Mode() { return GMode; }
const TCHAR* TSLook::ModeName() { return GMode == EMode::HD2D ? TEXT("hd2d") : GMode == EMode::Flat2D ? TEXT("flat2d") : TEXT("mesh3d"); }

float TSLook::SpriteUnits()
{
	return float(TSJson::Num(TSJson::Obj(GCfg, TEXT("spriteUnits")), ModeName(), 7.0));
}

FRotator TSLook::CameraRotation()
{
	const float Yaw = float(TSJson::Num(GTopDown, TEXT("yaw"), -90));
	if (GMode == EMode::Flat2D) return FRotator(-90.f, Yaw, 0.f);
	if (GMode == EMode::HD2D) return FRotator(float(TSJson::Num(TSJson::Obj(GCfg, TEXT("hd2dCamera")), TEXT("pitch"), -40)), Yaw, 0.f);
	return FRotator(float(TSJson::Num(GTopDown, TEXT("pitch"), -58)), Yaw, 0.f);
}

FRotator TSLook::CardRotation()
{
	// The engine Plane lies in its local XY (normal +Z, U along +X, V along +Y). Lay U along screen-right,
	// V down the screen, and the normal toward the camera.
	const FRotationMatrix Cam(CameraRotation());
	const FVector Right = Cam.GetUnitAxis(EAxis::Y);
	FVector Up = Cam.GetUnitAxis(EAxis::Z);
	if (GMode == EMode::Flat2D) Up = FRotationMatrix(FRotator(0, CameraRotation().Yaw, 0)).GetUnitAxis(EAxis::X);   // the picture's top points north
	return FRotationMatrix::MakeFromXY(Right, -Up).Rotator();
}

float TSLook::FlatSortZ(float WorldY)
{
	return 30.f + WorldY * 0.02f;
}

UMaterialInterface* TSLook::PixelTexture(const FString& TexName)
{
	const FString Key = TEXT("world:") + TexName;
	if (const TStrongObjectPtr<UMaterialInstanceDynamic>* Hit = GCache.Find(Key)) return Hit->Get();
	UMaterialInterface* Base = TSAssets::Load<UMaterialInterface>(MatPath(TEXT("pixelWorld")));
	UTexture2D* Tex = PixelTex(TEXT("TX_") + TexName);
	if (!Base || !Tex) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
	MID->SetTextureParameterValue(TEXT("Tex"), Tex);
	MID->SetScalarParameterValue(TEXT("Size"), float(TSJson::Num(GCfg, TEXT("pixelSize"), 200)));
	GCache.Add(Key, TStrongObjectPtr<UMaterialInstanceDynamic>(MID));
	return MID;
}

UMaterialInterface* TSLook::PixelMaterial(const FString& MaterialName)
{
	if (GMode != EMode::HD2D) return nullptr;
	const FString Tex = TSJson::Str(TSJson::Obj(GCfg, TEXT("pixelMaterials")), MaterialName);
	return Tex.IsEmpty() ? nullptr : PixelTexture(Tex);
}

UMaterialInstanceDynamic* TSLook::SpriteMaterial(UObject* Outer, const FString& Texture, int32 Cols, int32 Rows)
{
	UMaterialInterface* Base = TSAssets::Load<UMaterialInterface>(MatPath(TEXT("sprite")));
	UTexture2D* Tex = PixelTex(Texture);
	if (!Base || !Tex) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	MID->SetTextureParameterValue(TEXT("Tex"), Tex);
	MID->SetScalarParameterValue(TEXT("Cols"), float(Cols));
	MID->SetScalarParameterValue(TEXT("Rows"), float(Rows));
	return MID;
}

UMaterialInterface* TSLook::PropMaterial(const FString& Texture)
{
	const FString Key = TEXT("prop:") + Texture;
	if (const TStrongObjectPtr<UMaterialInstanceDynamic>* Hit = GCache.Find(Key)) return Hit->Get();
	UMaterialInstanceDynamic* MID = SpriteMaterial(GetTransientPackage(), Texture);
	if (MID) GCache.Add(Key, TStrongObjectPtr<UMaterialInstanceDynamic>(MID));
	return MID;
}
