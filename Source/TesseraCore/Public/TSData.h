#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TSJson.h"
#include "TSData.generated.h"

/** On-screen words Tessera shows, each overridable in <world>.text: TSText::Get(Ctx, "block", "BLOCK").
 *  {name} placeholders are filled from Args. */
namespace TSText
{
	TESSERACORE_API FString Get(const UObject* WorldContext, const FString& Key, const FString& Default, const TMap<FString, FString>& Args = {});
}

/** One spawn marker from the map, resolved through map.spawns ("P": "player", "g": "enemy:guard"). */
struct FTSSpawn
{
	FString Kind;   // "player", "npc", "enemy", ... (whatever the game's data says)
	FString Id;     // e.g. "guard"
	int32 X = 0;
	int32 Y = 0;
	FString Area;   // "" = the main map; else the area it's in (X, Y are world tiles either way)
};

/** A separate small map (a cave, a cellar...) built off to the side of the main one, reached through doors. */
struct FTSArea
{
	FString Id, Name;
	TArray<FString> Rows;   // spawn markers already replaced by floor
	int32 W = 0, H = 0;
	FIntPoint Origin = FIntPoint::ZeroValue;   // its top-left, in world tiles
	TSJson::FObj Def;
	bool Contains(int32 TX, int32 TY) const { return TX >= Origin.X && TY >= Origin.Y && TX < Origin.X + W && TY < Origin.Y + H; }
};

/**
 * The game's data, loaded once from a JSON file in Content (plain file IO, so stage the folder as loose files when
 * packaging). The game picks the file and names in DefaultGame.ini:
 *
 *   [Tessera]
 *   DataFile=Data/game.json        ; relative to Content
 *   WorldSection=world             ; the section holding the world / look / sky settings below
 *   CommandPrefix=TS               ; command-line switches: -TSLook=, -TSHour=, ...
 *
 * What Tessera itself reads from the data:
 *   <world>.unitsPerPx, <world>.tileSize   gameplay numbers are in "pixels"; Px() converts them to Unreal units
 *   tuning                                 free-form numbers for the game (Tuning(key))
 *   map.rows                               tile rows, one character per tile
 *   map.spawns                             marker character -> "kind:id"; the marker tile becomes the floor to its left
 *   map.floor, map.solid                   the floor character, and the characters a marker can't take over (walls...)
 *   map.regions                            [ { "id", "maxRow" } ], top to bottom
 */
UCLASS()
class TESSERACORE_API UTSData : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UTSData& Get(const UObject* WorldContext);

	TSJson::FObj Root;
	bool IsLoaded() const { return Root.IsValid(); }

	TSJson::FObj Section(const FString& Name) const { return TSJson::Obj(Root, Name); }
	/** e.g. Entry("enemies", "guard") */
	TSJson::FObj Entry(const FString& SectionName, const FString& Id) const { return TSJson::Obj(Section(SectionName), Id); }
	double Tuning(const FString& Key, double Default = 0.0) const { return TSJson::Num(Section(TEXT("tuning")), Key, Default); }
	/** O[Key] as a number, or as the name of a tuning number ("staminaRegen" -> tuning.staminaRegen). */
	double Value(const TSJson::FObj& O, const FString& Key, double Default = 0.0) const;
	/** The world / look / sky settings ([Tessera] WorldSection). */
	TSJson::FObj World() const { return Section(WorldSection); }

	/** Data pixels -> Unreal units. */
	float Px(double Pixels) const { return float(Pixels) * UnitsPerPx; }
	float UnitsPerPx = 1.f;
	float TileSize = 100.f;

	// ---- map ----
	TArray<FString> Rows;          // spawn markers already replaced by floor
	int32 MapW = 0, MapH = 0;
	TArray<FTSSpawn> Spawns;
	/** map.areas { id: { "name", "at": [tileX, tileY] (top-left, world tiles; keep it clear of the main map), "rows": [...] } }:
	 *  the same legend and spawn markers as the main map. */
	TArray<FTSArea> Areas;
	/** The area a world position is in (null: the main map). */
	const FTSArea* AreaAt(const FVector& P) const;
	const FTSArea* FindArea(const FString& Id) const;

	/** The tile character at world tile X, Y (the main map or an area; Outside beyond them). */
	TCHAR TileAt(int32 X, int32 Y, TCHAR Outside = TEXT(' ')) const;
	/** World position of a tile's centre on the ground plane. */
	FVector TileCenter(int32 X, int32 Y) const { return FVector((X + 0.5f) * TileSize, (Y + 0.5f) * TileSize, 0.f); }
	FIntPoint TileOf(const FVector& P) const { return FIntPoint(FMath::FloorToInt(P.X / TileSize), FMath::FloorToInt(P.Y / TileSize)); }
	/** The map region (map.regions) for a world Y: the first whose maxRow reaches it ("" without regions). */
	FString RegionAt(float WorldY) const;

private:
	void ParseMap();
	FString WorldSection;
};
