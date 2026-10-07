#include "TSData.h"
#include "Tessera.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UTSData::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	WorldSection = TSConfig::Get(TEXT("WorldSection"), TEXT("world"));
	const FString Path = FPaths::ProjectContentDir() / TSConfig::Get(TEXT("DataFile"), TEXT("Data/game.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTessera, Error, TEXT("Could not read the game data %s (DefaultGame.ini [Tessera] DataFile)"), *Path);
		return;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTessera, Error, TEXT("%s is not valid JSON: %s"), *Path, *Reader->GetErrorMessage());
		Root.Reset();
		return;
	}

	UnitsPerPx = float(TSJson::Num(World(), TEXT("unitsPerPx"), 1.0));
	TileSize = float(TSJson::Num(World(), TEXT("tileSize"), 100.0));
	ParseMap();
	UE_LOG(LogTessera, Display, TEXT("Game data loaded from %s: %d sections, map %dx%d (%d spawns)."),
		*FPaths::GetCleanFilename(Path), Root->Values.Num(), MapW, MapH, Spawns.Num());
}

UTSData& UTSData::Get(const UObject* WorldContext)
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext);
	check(GI);
	return *GI->GetSubsystem<UTSData>();
}

void UTSData::ParseMap()
{
	const TSJson::FObj Map = Section(TEXT("map"));
	const TSJson::FObj SpawnDefs = TSJson::Obj(Map, TEXT("spawns"));
	const FString Floor = TSJson::Str(Map, TEXT("floor"), TEXT("."));
	const FString Solid = TSJson::Str(Map, TEXT("solid"));
	const TCHAR FloorChar = Floor.IsEmpty() ? TEXT('.') : Floor[0];

	for (const TSharedPtr<FJsonValue>& V : TSJson::Arr(Map, TEXT("rows"))) Rows.Add(V->AsString());
	MapH = Rows.Num();
	for (const FString& R : Rows) MapW = FMath::Max(MapW, R.Len());

	for (int32 Y = 0; Y < MapH; ++Y)
	{
		FString& Row = Rows[Y];
		while (Row.Len() < MapW) Row.AppendChar(FloorChar);
		for (int32 X = 0; X < MapW; ++X)
		{
			FString Def;
			if (!SpawnDefs.IsValid() || !SpawnDefs->TryGetStringField(Row.Mid(X, 1), Def)) continue;

			FTSSpawn S;
			S.X = X; S.Y = Y;
			if (!Def.Split(TEXT(":"), &S.Kind, &S.Id)) S.Kind = Def;
			Spawns.Add(S);

			// The marker tile becomes whatever floor is to its left (unless that's solid).
			const TCHAR Left = X > 0 ? Row[X - 1] : FloorChar;
			int32 Ignored;
			Row[X] = Solid.FindChar(Left, Ignored) ? FloorChar : Left;
		}
	}
}

TCHAR UTSData::TileAt(int32 X, int32 Y, TCHAR Outside) const
{
	if (X < 0 || Y < 0 || Y >= MapH || X >= MapW) return Outside;
	return Rows[Y][X];
}

FString UTSData::RegionAt(float WorldY) const
{
	const int32 Row = FMath::FloorToInt(WorldY / TileSize);
	FString Last;
	for (const TSharedPtr<FJsonValue>& V : TSJson::Arr(Section(TEXT("map")), TEXT("regions")))
	{
		const TSJson::FObj R = V->AsObject();
		Last = TSJson::Str(R, TEXT("id"));
		if (Row <= int32(TSJson::Num(R, TEXT("maxRow"), 9999))) return Last;
	}
	return Last;
}
