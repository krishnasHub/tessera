#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Loading assets by path (worlds are built at runtime, so nothing holds hard references) and engine basics.
 *
 * Named assets: the game lists the assets Tessera uses in <world>.assets, e.g.
 *   "assets": { "glow": "/Game/X/M_Glow.M_Glow", "hitEffect": "/Game/X/NS_Hit.NS_Hit", "coin": "M_Metal_Gold" }
 * A value starting with "/" is an object path; anything else is a material name handed to the game's resolver
 * (SetMaterialResolver), which is also how weapon-kit parts name their materials.
 */
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

	/** <world>.assets[Key] as written in the data ("" if absent). */
	TESSERACORE_API FString Path(const UObject* WorldContext, const FString& Key);
	/** A named asset (<world>.assets[Key]), loaded. */
	template <class T>
	T* Get(const UObject* WorldContext, const FString& Key) { return Load<T>(Path(WorldContext, Key)); }
	/** A named material: an object path, or a name for the game's resolver. */
	TESSERACORE_API UMaterialInterface* Material(const UObject* WorldContext, const FString& Key);

	/** How the game turns a material name ("M_Metal_Steel") into a material. Default: the name is an object path. */
	TESSERACORE_API void SetMaterialResolver(TFunction<UMaterialInterface*(const FString& Name)> Resolver);
	TESSERACORE_API UMaterialInterface* ResolveMaterial(const FString& NameOrPath);
}
