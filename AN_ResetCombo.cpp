#include "Animations/AN_ResetCombo.h"
#include "Components/CombatComponent.h"
#include "GameFramework/Actor.h"

void UAN_ResetCombo::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Animation*/)
{
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (UCombatComponent* CombatComp = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		CombatComp->ResetCombo();
	}
}

