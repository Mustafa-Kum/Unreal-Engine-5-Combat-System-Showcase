#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AN_ResetCombo.generated.h"

/**
 * Resets the combo state to the first attack.
 */
UCLASS()
class WOWCLONE_API UAN_ResetCombo : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
