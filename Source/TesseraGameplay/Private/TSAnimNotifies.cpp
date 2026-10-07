#include "TSAnimNotifies.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_DoAttackTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ITSAttacker* A = Cast<ITSAttacker>(MeshComp ? MeshComp->GetOwner() : nullptr)) A->DoAttackTrace(AttackBoneName);
}

void UAnimNotify_CheckCombo::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ITSAttacker* A = Cast<ITSAttacker>(MeshComp ? MeshComp->GetOwner() : nullptr)) A->CheckCombo();
}

void UAnimNotify_CheckChargedAttack::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ITSAttacker* A = Cast<ITSAttacker>(MeshComp ? MeshComp->GetOwner() : nullptr)) A->CheckChargedAttack();
}
