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
		CombatComp->BeginComboWindow(this, ComboWindowRequest);
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
		CombatComp->EndComboWindow(this);
	}
}

