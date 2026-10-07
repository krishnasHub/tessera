#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TSInventory.h"
#include "TSLoot.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Loot on the ground: coins (pulled toward the hero when close; <world>.assets.coin) or an item gem coloured by
 * rarity, with a light beam for rare and quest items. The hero (the player pawn, with a UTSInventoryComponent)
 * walks over it to pick it up. Words: <world>.text currencyGained ("+{n}"), pickedUp, bagFull.
 */
UCLASS()
class TESSERAGAMEPLAY_API ATSPickup : public AActor
{
	GENERATED_BODY()

public:
	ATSPickup();
	void InitCurrency(int32 Amount);
	void InitItem(const FTSItem& Item);
	virtual void Tick(float DeltaSeconds) override;

	bool bCurrency = false;
	int32 Amount = 0;
	FTSItem Item;

private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Gem;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	float Age = 0.f, BaseZ = 0.f, Warned = 0.f;
};

/** Loot tables: lootTables { <id>: { <currencyKey>: [min, max], rolls, dropChance, entries: [ { item, weight } ], rarity: weights } }. */
namespace TSLoot
{
	TESSERAGAMEPLAY_API void DropTable(UWorld* World, const FVector& At, const FString& TableId);
	/** An item (or, with none, Currency coins) on the ground near At. */
	TESSERAGAMEPLAY_API void Spawn(UWorld* World, const FVector& At, const FTSItem* Item, int32 Currency);
}
