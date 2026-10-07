#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UObject;

/**
 * Visual style ("look") of the run: <world>.look in the data, or -<Prefix>Look= on the command line.
 *
 *   Mesh3D   3D meshes and the game's own materials
 *   HD2D     pixel-art sprite cards standing in the lit 3D world, pixel-art textures on the terrain and
 *            props, tree cards (Octopath Traveler style)
 *   Flat2D   orthographic straight-down camera over a baked pixel-art map; flat sprites, y-sorted
 *
 * Gameplay is identical in every look: only what is drawn changes.
 *
 * Data (<world>.looks2d): spriteUnits { hd2d, flat2d } (world units per sprite pixel), hd2dCamera.pitch, pixelSize,
 * pixelMaterials { <material name>: <texture> }, textureFolder; <world>.assets { sprite, pixelWorld } (materials).
 * Camera: <world>.camera.topdown { pitch, yaw }.
 */
namespace TSLook
{
	enum class EMode : uint8 { Mesh3D, HD2D, Flat2D };

	/** Read once from data / the command line (by the game mode, before the world is built). */
	TESSERACORE_API void Init(const UObject* WorldContext);
	TESSERACORE_API EMode Mode();
	inline bool IsSprite() { return Mode() != EMode::Mesh3D; }
	TESSERACORE_API const TCHAR* ModeName();

	/** World units per sprite pixel. */
	TESSERACORE_API float SpriteUnits();
	/** The fixed camera rotation of this look (cards are turned to face it). */
	TESSERACORE_API FRotator CameraRotation();
	/** Rotation for a sprite card: the engine Plane turned to face the camera (HD-2D) or lying flat with
	 *  the picture's top to the north (Flat 2D). Card height runs along the returned rotation's -Y axis. */
	TESSERACORE_API FRotator CardRotation();
	/** Flat 2D: card height above the ground for a world Y (south draws over north). */
	TESSERACORE_API float FlatSortZ(float WorldY);

	/** HD-2D: the pixel-art replacement for a named material (looks2d.pixelMaterials), or null to keep the original. */
	TESSERACORE_API UMaterialInterface* PixelMaterial(const FString& MaterialName);
	/** A pixel-art world material for a texture name ("grass", "rock", ...). */
	TESSERACORE_API UMaterialInterface* PixelTexture(const FString& TexName);
	/** A new sprite material instance showing <textureFolder>/<Texture> (Cols x Rows frames). */
	TESSERACORE_API UMaterialInstanceDynamic* SpriteMaterial(UObject* Outer, const FString& Texture, int32 Cols = 1, int32 Rows = 1);
	/** A shared single-frame sprite material for props (trees, houses...). */
	TESSERACORE_API UMaterialInterface* PropMaterial(const FString& Texture);
}
