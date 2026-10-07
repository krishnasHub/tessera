#include "TSLoot.h"
#include "TSData.h"
#include "TSAssets.h"
#include "TSCharacter.h"
#include "TSFeedback.h"
#include "TSWorldBuilder.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

ATSPickup::ATSPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	Gem = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gem"));
	Gem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Gem;
	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Gem);
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	Beam->SetVisibility(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Gem);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(12.f);
	Light->SetAttenuationRadius(300.f);
}

void ATSPickup::InitCurrency(int32 InAmount)
{
	bCurrency = true;
	Amount = InAmount;
	Gem->SetStaticMesh(TSAssets::Shape(TEXT("Cylinder")));
	Gem->SetMaterial(0, TSAssets::Material(this, TEXT("coin")));
	Gem->SetWorldScale3D(FVector(0.22f, 0.22f, 0.06f));
	Light->SetLightColor(FLinearColor(1.f, 0.8f, 0.3f));
	Light->SetIntensity(4.f);
	BaseZ = GetActorLocation().Z;
}

void ATSPickup::InitItem(const FTSItem& InItem)
{
	Item = InItem;
	const FLinearColor C = UTSInventoryComponent::RarityColor(this, Item.Rarity);
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(TSAssets::Material(this, TEXT("glow")), this);
	M->SetVectorParameterValue(TEXT("Color"), C);
	M->SetScalarParameterValue(TEXT("Intensity"), 4.f);
	Gem->SetStaticMesh(TSAssets::Shape(TEXT("Cone")));
	Gem->SetMaterial(0, M);
	Gem->SetWorldScale3D(FVector(0.18f, 0.18f, 0.28f));
	Light->SetLightColor(C);
	if (Item.Rarity == TEXT("rare") || Item.Rarity == TEXT("quest"))
	{
		UMaterialInstanceDynamic* B = UMaterialInstanceDynamic::Create(TSAssets::Material(this, TEXT("telegraph")), this);
		B->SetVectorParameterValue(TEXT("Color"), C);
		B->SetScalarParameterValue(TEXT("Opacity"), 0.25f);
		Beam->SetStaticMesh(TSAssets::Shape(TEXT("Cylinder")));
		Beam->SetMaterial(0, B);
		Beam->SetWorldScale3D(FVector(0.08f, 0.08f, 4.f));
		Beam->SetRelativeLocation(FVector(0, 0, 900.f));
		Beam->SetVisibility(true);
	}
	BaseZ = GetActorLocation().Z;
}

void ATSPickup::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	Warned -= Dt;
	FVector L = GetActorLocation();
	L.Z = BaseZ + 12.f + FMath::Sin(Age * 3.f) * 6.f;
	SetActorLocation(L);
	AddActorWorldRotation(FRotator(0, 90.f * Dt, 0));

	ATSCharacter* P = Cast<ATSCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UTSInventoryComponent* Bag = P ? P->FindComponentByClass<UTSInventoryComponent>() : nullptr;
	UTSFeedback* Fb = UTSFeedback::Get(this);
	if (!Bag || P->IsDead() || Age < 0.4f) return;
	const float D = FVector::Dist2D(P->GetActorLocation(), L);
	if (bCurrency && D < 260.f)
	{
		const FVector To = (P->GetActorLocation() - L).GetSafeNormal2D();
		SetActorLocation(L + To * 900.f * Dt);
	}
	if (D > P->Radius() + 45.f) return;

	if (bCurrency)
	{
		Bag->Currency += Amount;
		Bag->OnChanged.Broadcast();
		Fb->Float(P->Head(), TSText::Get(this, TEXT("currencyGained"), TEXT("+{n}"), { { TEXT("n"), FString::FromInt(Amount) } }), FLinearColor(1.f, 0.83f, 0.3f), 0.9f);
		Destroy();
	}
	else if (Bag->Add(Item))
	{
		Fb->Toast(TSText::Get(this, TEXT("pickedUp"), TEXT("Picked up {item}"), { { TEXT("item"), Item.Name } }), UTSInventoryComponent::RarityColor(this, Item.Rarity));
		Destroy();
	}
	else if (Warned <= 0.f)
	{
		Fb->Toast(TSText::Get(this, TEXT("bagFull"), TEXT("Bag is full")));
		Warned = 3.f;
	}
}

// ---------------------------------------------------------------------------------------------

void TSLoot::Spawn(UWorld* World, const FVector& At, const FTSItem* Item, int32 Currency)
{
	const float A = FMath::FRandRange(0.f, UE_TWO_PI), R = FMath::FRandRange(20.f, 80.f);
	FVector P = At + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
	for (TActorIterator<ATSWorldBuilder> It(World); It; ++It) { P.Z = It->GroundZ(P.X, P.Y); break; }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATSPickup* Pk = World->SpawnActor<ATSPickup>(P, FRotator::ZeroRotator, SP);
	if (Item) Pk->InitItem(*Item); else Pk->InitCurrency(Currency);
}

void TSLoot::DropTable(UWorld* World, const FVector& At, const FString& TableId)
{
	const UTSData& D = UTSData::Get(World);
	const TSJson::FObj Table = D.Entry(TEXT("lootTables"), TableId);
	if (!Table) return;
	// Coins: [min, max] under the game's currency key (<world>.currencyKey, default "currency").
	const TArray<TSharedPtr<FJsonValue>> Coins = TSJson::Arr(Table, TSJson::Str(D.World(), TEXT("currencyKey"), TEXT("currency")));
	if (Coins.Num() == 2) Spawn(World, At, nullptr, FMath::RandRange(int32(Coins[0]->AsNumber()), int32(Coins[1]->AsNumber())));

	const TArray<TSharedPtr<FJsonValue>> Entries = TSJson::Arr(Table, TEXT("entries"));
	const int32 Rolls = int32(TSJson::Num(Table, TEXT("rolls"), 1));
	for (int32 I = 0; I < Rolls && Entries.Num(); ++I)
	{
		if (FMath::FRand() >= TSJson::Num(Table, TEXT("dropChance"), 0.4)) continue;
		double Total = 0;
		for (const auto& E : Entries) Total += TSJson::Num(E->AsObject(), TEXT("weight"), 1);
		double R = FMath::FRand() * Total;
		FString Pick;
		for (const auto& E : Entries) { R -= TSJson::Num(E->AsObject(), TEXT("weight"), 1); if (R <= 0) { Pick = TSJson::Str(E->AsObject(), TEXT("item")); break; } }
		if (Pick.IsEmpty()) Pick = TSJson::Str(Entries.Last()->AsObject(), TEXT("item"));
		const FTSItem It = UTSInventoryComponent::MakeItem(World, Pick, TSJson::Obj(Table, TEXT("rarity")));
		Spawn(World, At, &It, 0);
	}
}
