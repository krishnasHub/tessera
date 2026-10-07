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

	// One grid of rows: pad them, pull out the spawn markers (world tiles: Origin + local).
	auto Parse = [&](TArray<FString>& Grid, int32& W, int32& H, const FIntPoint& Origin, const FString& Area)
	{
		H = Grid.Num();
		for (const FString& R : Grid) W = FMath::Max(W, R.Len());
		for (int32 Y = 0; Y < H; ++Y)
		{
			FString& Row = Grid[Y];
			while (Row.Len() < W) Row.AppendChar(FloorChar);
			for (int32 X = 0; X < W; ++X)
			{
				FString Def;
				if (!SpawnDefs.IsValid() || !SpawnDefs->TryGetStringField(Row.Mid(X, 1), Def)) continue;

				FTSSpawn S;
				S.X = Origin.X + X; S.Y = Origin.Y + Y;
				S.Area = Area;
				if (!Def.Split(TEXT(":"), &S.Kind, &S.Id)) S.Kind = Def;
				Spawns.Add(S);

				// The marker tile becomes whatever floor is to its left (unless that's solid).
				const TCHAR Left = X > 0 ? Row[X - 1] : FloorChar;
				int32 Ignored;
				Row[X] = Solid.FindChar(Left, Ignored) ? FloorChar : Left;
			}
		}
	};
	for (const TSharedPtr<FJsonValue>& V : TSJson::Arr(Map, TEXT("rows"))) Rows.Add(V->AsString());
	Parse(Rows, MapW, MapH, FIntPoint::ZeroValue, FString());

	if (const TSJson::FObj AreaDefs = TSJson::Obj(Map, TEXT("areas")))
		for (const auto& KV : AreaDefs->Values)
		{
			const TSJson::FObj A = TSJson::Obj(AreaDefs, FString(*KV.Key));   // (skips "_doc" strings)
			if (!A) continue;
			FTSArea Area;
			Area.Id = FString(*KV.Key);
			Area.Def = A;
			Area.Name = TSJson::Str(A, TEXT("name"), Area.Id);
			const TArray<TSharedPtr<FJsonValue>> At = TSJson::Arr(A, TEXT("at"));
			if (At.Num() == 2) Area.Origin = FIntPoint(int32(At[0]->AsNumber()), int32(At[1]->AsNumber()));
			for (const TSharedPtr<FJsonValue>& V : TSJson::Arr(A, TEXT("rows"))) Area.Rows.Add(V->AsString());
			Parse(Area.Rows, Area.W, Area.H, Area.Origin, Area.Id);
			Areas.Add(MoveTemp(Area));
		}
}

const FTSArea* UTSData::AreaAt(const FVector& P) const
{
	const FIntPoint T = TileOf(P);
	for (const FTSArea& A : Areas) if (A.Contains(T.X, T.Y)) return &A;
	return nullptr;
}

const FTSArea* UTSData::FindArea(const FString& Id) const
{
	for (const FTSArea& A : Areas) if (A.Id == Id) return &A;
	return nullptr;
}

TCHAR UTSData::TileAt(int32 X, int32 Y, TCHAR Outside) const
{
	if (X < 0 || Y < 0 || Y >= MapH || X >= MapW)
	{
		for (const FTSArea& A : Areas) if (A.Contains(X, Y)) return A.Rows[Y - A.Origin.Y][X - A.Origin.X];
		return Outside;
	}
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

double UTSData::Value(const TSJson::FObj& O, const FString& Key, double Default) const
{
	const TSharedPtr<FJsonValue> V = O.IsValid() ? O->TryGetField(Key) : nullptr;
	if (!V) return Default;
	if (V->Type == EJson::String) return Tuning(V->AsString(), Default);
	return V->Type == EJson::Number ? V->AsNumber() : Default;
}

FString TSText::Get(const UObject* WorldContext, const FString& Key, const FString& Default, const TMap<FString, FString>& Args)
{
	FString Out = TSJson::Str(TSJson::Obj(UTSData::Get(WorldContext).World(), TEXT("text")), Key, Default);
	for (const auto& KV : Args) Out = Out.Replace(*(TEXT("{") + KV.Key + TEXT("}")), *KV.Value);
	return Out;
}
