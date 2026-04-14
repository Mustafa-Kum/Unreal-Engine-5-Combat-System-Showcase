#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "CombatFeedbackComponent.generated.h"

class UDecalComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WOWCLONE_API UCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatFeedbackComponent();

	void HandleReceivedDamage(float AppliedDamage, const FDamageEvent& DamageEvent);
	void RequestGroundAreaIndicator(UObject* RequestOwner, float Radius);
	void ClearGroundAreaIndicatorRequest(UObject* RequestOwner);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FGroundAreaIndicatorRequest
	{
		TWeakObjectPtr<UObject> RequestOwner;
		float Radius = 0.0f;
	};

	void InitializeOverheadHealthBarWidget();
	void InitializeAreaIndicator();
	void RefreshGroundAreaIndicator();
	void HandleGroundAreaIndicatorRefreshTimer();
	void ShowGroundAreaIndicator(float Radius);
	void HideGroundAreaIndicator();
	void UpdateGroundAreaIndicatorRefreshTimer(bool bShouldRun);
	[[nodiscard]] class UWidgetComponent* ResolveOverheadHealthBarComponent() const;
	[[nodiscard]] bool IsCriticalDamageEvent(const FDamageEvent& DamageEvent) const;
	[[nodiscard]] bool ShouldDisplayLocalAreaIndicator() const;

	UPROPERTY(Transient)
	TObjectPtr<class ACharacterBase> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<class UOverheadHealthBarWidget> CachedOverheadHealthBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> AreaIndicatorDecalComponent;

	TArray<FGroundAreaIndicatorRequest> ActiveGroundAreaIndicatorRequests;
	FTimerHandle GroundAreaIndicatorRefreshTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback")
	FName OverheadHealthBarComponentName = TEXT("OverheadHealthBarComponent");

	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Area Indicator")
	TObjectPtr<class UMaterialInterface> AreaIndicatorMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Area Indicator", meta = (ClampMin = "1.0"))
	float AreaIndicatorProjectionDepth = 128.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Area Indicator", meta = (ClampMin = "0.0"))
	float AreaIndicatorGroundOffset = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Area Indicator", meta = (ClampMin = "0.05"))
	float AreaIndicatorRefreshInterval = 0.2f;
};
