#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_AttackMoveInterruptWindow.generated.h"

/**
 * Opens a window during the attack montage where movement input can interrupt root motion.
 */
UCLASS()
class WOWCLONE_API UANS_AttackMoveInterruptWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Interrupt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InterruptBlendOutTime = 0.25f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
