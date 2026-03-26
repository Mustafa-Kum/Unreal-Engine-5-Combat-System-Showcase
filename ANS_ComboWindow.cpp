#include "Animations/ANS_ComboWindow.h"
#include "Components/CombatComponent.h"
#include "GameFramework/Actor.h"

void UANS_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Animation*/, float /*TotalDuration*/)
{
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (UCombatComponent* CombatComp = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		CombatComp->SetCanAdvanceCombo(true);
	}
}

void UANS_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Animation*/)
{
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (UCombatComponent* CombatComp = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		CombatComp->SetCanAdvanceCombo(false);
	}
}

