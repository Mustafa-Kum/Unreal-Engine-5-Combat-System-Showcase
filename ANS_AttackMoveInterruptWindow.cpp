#include "Animations/ANS_AttackMoveInterruptWindow.h"
#include "Animation/AnimMontage.h"
#include "Components/CombatComponent.h"
#include "GameFramework/Actor.h"

void UANS_AttackMoveInterruptWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	UCombatComponent* CombatComp = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!CombatComp)
	{
		return;
	}

	if (const UAnimMontage* AttackMontage = Cast<UAnimMontage>(Animation))
	{
		CombatComp->BeginAttackMoveInterruptWindow(this, AttackMontage, InterruptBlendOutTime);
	}
}

void UANS_AttackMoveInterruptWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (UCombatComponent* CombatComp = Owner ? Owner->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		CombatComp->EndAttackMoveInterruptWindow(this);
	}
}
