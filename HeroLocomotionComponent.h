#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeroLocomotionComponent.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

UCLASS( ClassGroup=(Movement), meta=(BlueprintSpawnableComponent) )
class WOWCLONE_API UHeroLocomotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHeroLocomotionComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** --- COMPONENT API --- */
	void InitializeLocomotion(ACharacterBase* InOwnerCharacter);
	void UpdateLocomotionState(float DeltaTime);
	
	// Input processing
	FVector2D GetNormalizedAndScaledMovementInput(FVector2D RawInput) const;

	// State Handlers
	void ToggleWalkSpeed();
	void ApplyCombatStateOverrides();
	void RevertCombatStateOverrides();

	// Getters for Character/AnimInstance
	[[nodiscard]] float GetBackwardSpeedMultiplier() const { return BackwardMovementSpeedMultiplier; }
	[[nodiscard]] bool IsWalking() const { return bIsWalking; }

	/** --- CONFIGURABLE PROPERTIES --- */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement Settings", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float LocomotionVelocityThreshold = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement Settings", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
	float BaseRotationRateYaw = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement Settings", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float BackwardMovementSpeedMultiplier = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Settings", meta = (ClampMin = "50.0", ClampMax = "500.0"))
	float WalkSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Settings", meta = (ClampMin = "300.0", ClampMax = "1500.0"))
	float RunSpeed = 350.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacterBase> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	// Optimization Bitfields
	uint8 bWasMoving : 1;
	uint8 bIsWalking : 1;
	uint8 bWasWalking : 1;

	UPROPERTY(EditDefaultsOnly, Category = "Movement Settings")
	float DefaultMaxWalkSpeed = 600.0f;

	FDelegateHandle MovementSpeedAttributeChangedHandle;

	/** --- SOLID HELPERS --- */
	void SetupMovementComponent();
	void BindMovementSpeedAttribute();
	void UnbindMovementSpeedAttribute();
	void HandleMovementSpeedChanged(const FOnAttributeChangeData& ChangeData);
	void RefreshRunSpeedFromAttributes();
	void ApplyCurrentMaxWalkSpeed();

	void UpdateLocomotionTags();
	[[nodiscard]] bool IsMoving() const;
	[[nodiscard]] bool HasLocomotionStateChanged(bool bIsMoving) const;
	void ClearLocomotionTags();
	void ApplyCurrentLocomotionTag(bool bIsMoving);
	void UpdateLocomotionStateCache(bool bIsMoving);

	void SetWalkingState(bool bEnabled);
	[[nodiscard]] float CalculateBackwardMovementScale(float ForwardInputValue) const;
	[[nodiscard]] FVector2D NormalizeInputVector(FVector2D RawInput) const;
	[[nodiscard]] FVector2D ApplyBackwardMovementPenalty(FVector2D NormalizedInput) const;
};
