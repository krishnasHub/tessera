#include "TSAssets.h"
#include "Tessera.h"
#include "TSData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	TFunction<UMaterialInterface*(const FString&)>& Resolver()
	{
		static TFunction<UMaterialInterface*(const FString&)> R;
		return R;
	}
}

void TSAssets::LogMissing(const FString& Path)
{
	UE_LOG(LogTessera, Warning, TEXT("Missing asset: %s"), Path.IsEmpty() ? TEXT("(no path set)") : *Path);
}

UMaterialInstanceDynamic* TSAssets::Color(UObject* Outer, const FLinearColor& InColor)
{
	UMaterialInterface* Base = Load<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
	MID->SetVectorParameterValue(TEXT("Color"), InColor);
	return MID;
}

FString TSAssets::Path(const UObject* WorldContext, const FString& Key)
{
	return TSJson::Str(TSJson::Obj(UTSData::Get(WorldContext).World(), TEXT("assets")), Key);
}

void TSAssets::SetMaterialResolver(TFunction<UMaterialInterface*(const FString&)> InResolver) { Resolver() = MoveTemp(InResolver); }

UMaterialInterface* TSAssets::ResolveMaterial(const FString& NameOrPath)
{
	if (NameOrPath.StartsWith(TEXT("/")) || !Resolver()) return Load<UMaterialInterface>(NameOrPath);
	return Resolver()(NameOrPath);
}

UMaterialInterface* TSAssets::Material(const UObject* WorldContext, const FString& Key)
{
	return ResolveMaterial(Path(WorldContext, Key));
}
