#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CombatTypes.h"
#include "ANS_ComboWindow.generated.h"

/**
 * Opens a window during the montage where the player can queue the next attack.
 */
UCLASS()
class WOWCLONE_API UANS_ComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo Window")
	FComboWindowRequest ComboWindowRequest;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
