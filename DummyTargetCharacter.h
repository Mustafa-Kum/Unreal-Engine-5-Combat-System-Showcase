#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Characters/CharacterBase.h"
#include "DummyTargetCharacter.generated.h"

/**
 * Lightweight melee test target that reuses the project's core character/attribute stack.
 */
UCLASS()
class WOWCLONE_API ADummyTargetCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ADummyTargetCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UWidgetComponent> OverheadHealthBarComponent;
};
