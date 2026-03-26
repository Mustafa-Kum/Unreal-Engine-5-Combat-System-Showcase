#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "CombatFeedbackComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WOWCLONE_API UCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatFeedbackComponent();

	void HandleReceivedDamage(float AppliedDamage, const FDamageEvent& DamageEvent);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeOverheadHealthBarWidget();
	[[nodiscard]] class UWidgetComponent* ResolveOverheadHealthBarComponent() const;
	[[nodiscard]] bool IsCriticalDamageEvent(const FDamageEvent& DamageEvent) const;

	UPROPERTY(Transient)
	TObjectPtr<class ACharacterBase> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<class UOverheadHealthBarWidget> CachedOverheadHealthBarWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback")
	FName OverheadHealthBarComponentName = TEXT("OverheadHealthBarComponent");
};
