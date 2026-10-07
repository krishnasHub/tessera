#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TSAnimNotifies.generated.h"

/**
 * Implemented by characters that play the combat montages. The montage notifies below call into it.
 */
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class TESSERAGAMEPLAY_API UTSAttacker : public UInterface { GENERATED_BODY() };

class ITSAttacker
{
	GENERATED_BODY()
public:
	/** The swing's hit moment. */
	virtual void DoAttackTrace(FName SourceBone) = 0;
	/** Window where a buffered attack input continues the combo. */
	virtual void CheckCombo() = 0;
	virtual void CheckChargedAttack() {}
};

/*
 * The three notifies below keep the class and property names of Unreal's Third Person template, so montages
 * authored there call into Tessera: add a package redirect to the game's DefaultEngine.ini,
 *   [CoreRedirects] +PackageRedirects=(OldName="/Script/TP_ThirdPerson",NewName="/Script/TesseraGameplay")
 */

UCLASS()
class TESSERAGAMEPLAY_API UAnimNotify_DoAttackTrace : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Attack")
	FName AttackBoneName;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Do Attack Trace"); }
};

UCLASS()
class TESSERAGAMEPLAY_API UAnimNotify_CheckCombo : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Check Combo String"); }
};

UCLASS()
class TESSERAGAMEPLAY_API UAnimNotify_CheckChargedAttack : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Check Charged Attack"); }
};
