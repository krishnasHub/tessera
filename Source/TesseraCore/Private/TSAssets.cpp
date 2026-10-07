#include "TSAssets.h"
#include "Tessera.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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
