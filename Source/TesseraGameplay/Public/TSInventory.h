#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TSJson.h"
#include "TSInventory.generated.h"

/** One item instance, rolled from data ("items", "rarities", "affixes"). */
struct FTSItem
{
	int32 Uid = 0;
	FString Id, Name, Type, Slot, Rarity = TEXT("common");
	TMap<FName, float> Mods;
	int32 Qty = 1;
	bool bStackable = false;
};

DECLARE_MULTICAST_DELEGATE(FTSInventoryChanged);

/**
 * Bag, equipment slots and currency. Equipping pushes the item's mods into the owner's stats.
 *
 * Data: items { <id>: { name, type: "equipment" | "consumable" | "quest" | ..., slot, mods, stackable, heal,
 * minAffixes, rarity } }, rarities { <id>: { affixes, color, salvage } }, affixes [ { stat, min, max, label } ].
 */
UCLASS(ClassGroup = (Tessera))
class TESSERAGAMEPLAY_API UTSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	TArray<FTSItem> Items;
	TMap<FString, FTSItem> Equipment;   // slot -> item
	int32 Currency = 0;
	int32 Capacity = 16;

	FTSInventoryChanged OnChanged;

	/** Rolls an item: rarity from weights (or common), random affixes per rarity. */
	static FTSItem MakeItem(const UObject* WorldContext, const FString& Id, const TSJson::FObj& RarityWeights = nullptr);
	static FLinearColor RarityColor(const UObject* WorldContext, const FString& Rarity);

	bool Add(const FTSItem& Item);
	int32 Count(const FString& Id) const;
	void Remove(const FString& Id, int32 Qty);
	FTSItem* Find(int32 Uid);
	void Equip(int32 Uid);
	void Unequip(const FString& Slot);
	/** Equipment -> equip, potion -> drink. Returns false if it could not be used. */
	bool Use(int32 Uid);
	void Salvage(int32 Uid);

private:
	void Changed() { OnChanged.Broadcast(); }
};
