#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"

class UStaticMesh;
class UMaterialInstanceDynamic;

/** Loading assets by path (worlds are built at runtime, so nothing holds hard references) and engine basics. */
namespace TSAssets
{
	TESSERACORE_API void LogMissing(const FString& Path);

	template <class T>
	T* Load(const FString& Path)
	{
		T* Obj = Path.IsEmpty() ? nullptr : LoadObject<T>(nullptr, *Path);
		if (!Obj) LogMissing(Path);
		return Obj;
	}

	/** "/Game/Folder" + "Name" -> "/Game/Folder/Name.Name" */
	inline FString ObjPath(const FString& Folder, const FString& Name) { return Folder / Name + TEXT(".") + Name; }

	/** /Engine/BasicShapes: Cube, Sphere, Cylinder, Cone, Plane (100uu). */
	inline UStaticMesh* Shape(const FString& Name) { return Load<UStaticMesh>(ObjPath(TEXT("/Engine/BasicShapes"), Name)); }

	/** A plain coloured surface (the engine's BasicShapeMaterial, "Color" parameter). */
	TESSERACORE_API UMaterialInstanceDynamic* Color(UObject* Outer, const FLinearColor& Color);
}
