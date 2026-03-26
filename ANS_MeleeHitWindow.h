#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_MeleeHitWindow.generated.h"

/**
 * Opens and closes the melee hit window for the active attack montage.
 * This notify only signals intent; gameplay state stays in UCombatComponent.
 */
UCLASS()
class WOWCLONE_API UANS_MeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
