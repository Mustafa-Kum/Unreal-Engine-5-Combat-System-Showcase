#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "CombatComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimNotify;
class UAnimNotifyState;
class UGameplayAbility;
class UWeaponDataAsset;
struct FCombatComboData;
struct FGameplayTagContainer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityCastStateChanged, const FAbilityCastState&, CastState);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WOWCLONE_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	void InitializeCombatState();
	void NotifyDamageDealt();
	[[nodiscard]] bool IsInCombat() const { return bIsInCombat; }

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ProcessAttackInput(ECombatAttackType AttackType);

	void BeginComboWindow(const UAnimNotifyState* WindowSource, const FComboWindowRequest& ComboWindowRequest = FComboWindowRequest());
	void EndComboWindow(const UAnimNotifyState* WindowSource);

	void BeginAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime);
	void EndAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource);
	void BeginAbilityInterruptWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime);
	void EndAbilityInterruptWindow(const UAnimNotifyState* WindowSource);
	void BeginMeleeHitWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, const FMeleeHitWindowRequest& HitWindowRequest = FMeleeHitWindowRequest());
	void EndMeleeHitWindow(const UAnimNotifyState* WindowSource);

	void ResetCombo();
	bool TryInterruptAttackForMovement();
	bool TryBeginAbilityAreaImpact(const FAbilityAreaImpactConfig& AreaImpactConfig, UGameplayAbility* SourceAbility);
	bool CanActivateAbilityAreaImpact(const FAbilityAreaImpactConfig& AreaImpactConfig, FGameplayTagContainer* OptionalRelevantTags = nullptr) const;
	void EndAbilityAreaImpact(UGameplayAbility* SourceAbility);
	void TriggerAbilityAreaImpact(const UAnimMontage* SourceMontage);
	bool TryBeginAbilityCast(const FAbilityCastConfig& CastConfig, UGameplayAbility* SourceAbility, FGameplayTagContainer* OptionalRelevantTags = nullptr);
	void EndAbilityCast(UGameplayAbility* SourceAbility);
	bool NotifyAbilityCastCommit(const UAnimMontage* SourceMontage, TSubclassOf<UAnimNotify> NotifyClass);
	void HandleReceivedDamage(float AppliedDamage);

	UFUNCTION(BlueprintCallable, Category = "Combat|Cast")
	[[nodiscard]] FAbilityCastState GetAbilityCastState() const;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Cast")
	FOnAbilityCastStateChanged OnAbilityCastStateChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	int32 CurrentComboIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bCanAdvanceCombo = false;

	[[nodiscard]] bool CanPerformAttack() const;
	[[nodiscard]] bool CanAdvanceComboForAttackType(ECombatAttackType AttackType) const;
	void HandleComboInput(ECombatAttackType AttackType);
	void HandleInitialInput(ECombatAttackType AttackType);
	void ExecuteNextComboStep(ECombatAttackType AttackType);
	void ClearPendingComboRequest();
	void SchedulePendingComboRetry();
	void RetryPendingComboRequest();
	void BufferAttackInput(ECombatAttackType AttackType);
	void ConsumeBufferedAttackInput();
	void ClearBufferedAttackInput();
	[[nodiscard]] bool HasBufferedAttackInput() const;

	[[nodiscard]] bool IsComboStateValid(const FCombatComboData* ComboData) const;
	void AdvanceComboState(const FCombatComboData& ComboData, ECombatAttackType AttackType);
	[[nodiscard]] int32 GetComboIndexForAttackType(ECombatAttackType AttackType) const;
	int32& GetMutableComboIndexForAttackType(ECombatAttackType AttackType);
	void RefreshCurrentComboIndex(ECombatAttackType AttackType);
	void ResetComboState(ECombatAttackType AttackType);

	void PlayComboAttack(UWeaponDataAsset* WeaponData, ECombatAttackType AttackType, int32 Index);
	void RequestComboMontageLoad(UWeaponDataAsset* WeaponData, ECombatAttackType AttackType, int32 Index);
	void OnComboMontageLoaded(UWeaponDataAsset* WeaponData, ECombatAttackType AttackType, int32 Index);
	[[nodiscard]] FMeleeKnockbackConfig GetComboStepKnockbackConfig(const FCombatComboData* ComboData, int32 Index) const;

	[[nodiscard]] UWeaponDataAsset* GetEquippedWeaponData() const;
	[[nodiscard]] const FCombatComboData* GetComboDataForAttackType(const UWeaponDataAsset* WeaponData, ECombatAttackType AttackType) const;
	[[nodiscard]] class UEquipmentComponent* GetEquipmentComponent() const;
	[[nodiscard]] class UCombatImpactComponent* GetCombatImpactComponent() const;
	[[nodiscard]] class UWeaponActionComponent* GetWeaponActionComponent() const;
	[[nodiscard]] class UHeroLocomotionComponent* GetHeroLocomotionComponent() const;
	[[nodiscard]] UAnimInstance* GetAnimInstance() const;

private:
	struct FComboWindowRuntimeState
	{
		FComboWindowRequest Request;
		int32 ActiveCount = 0;
	};

	struct FAttackInterruptWindowState
	{
		const UAnimMontage* Montage = nullptr;
		float BlendOutTime = 0.25f;
		int32 ActiveCount = 0;
	};

	struct FAbilityCastRuntimeState
	{
		FAbilityCastConfig Config;
		TWeakObjectPtr<UGameplayAbility> SourceAbility;
		FText AbilityName;
		float StartWorldTime = 0.0f;
		bool bHasCommitted = false;

		[[nodiscard]] bool IsActive() const;
		void Reset();
	};

	bool TryPrepareAbilityAreaImpactActivation(UAnimInstance* AnimInstance);
	void BeginInterruptWindow(TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates, const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime);
	void EndInterruptWindow(TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates, const UAnimNotifyState* WindowSource);
	void InterruptAttackMontage(UAnimInstance* AnimInstance, const UAnimMontage* AttackMontage, float BlendOutTime);
	void RecordPlayedAttackMontage(UAnimMontage* PlayedMontage, float PlayedDuration, const FMeleeKnockbackConfig& KnockbackConfig);
	void OnAttackMontageBlendingOut(UAnimMontage* AttackMontage, bool bInterrupted);
	void RefreshComboWindowState();
	void ClearInterruptWindowsForMontage(TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates, const UAnimMontage* AttackMontage);
	[[nodiscard]] const UAnimMontage* ResolveInterruptibleMontage(UAnimInstance* AnimInstance, const TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates) const;
	[[nodiscard]] float ResolveInterruptBlendOutTime(const UAnimMontage* AttackMontage, const TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates) const;
	void EnterCombatState(class UAbilitySystemComponent* AbilitySystemComponent);
	void ExitCombatState(class UAbilitySystemComponent* AbilitySystemComponent);
	void HandleCombatTagChange(class UAbilitySystemComponent* AbilitySystemComponent, bool bEnteringCombat);
	void ApplyCombatStateMovementOverrides();
	void RevertCombatStateMovementOverrides();
	void RefreshCombatExitTimer();
	void HandleCombatExitTimeout();
	[[nodiscard]] bool HasInterruptibleAbilityCast() const;
	void BroadcastAbilityCastState();
	void ClearAbilityCastState(bool bBroadcastStateChanged);
	void SetAbilityCastingTag(bool bEnable) const;
	void InterruptAbilityCast(float BlendOutTime, const TCHAR* DebugReason);

	UPROPERTY(Transient)
	TObjectPtr<class ACharacterBase> OwnerCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWeaponDataAsset> PendingComboWeaponData;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimMontage> CurrentAttackMontage;

	int32 LightComboIndex = 0;
	int32 HeavyComboIndex = 0;
	ECombatAttackType ActiveAttackType = ECombatAttackType::Light;
	ECombatAttackType BufferedAttackType = ECombatAttackType::Light;
	ECombatAttackType PendingComboAttackType = ECombatAttackType::Light;
	int32 PendingComboMontageIndex = INDEX_NONE;
	TMap<const UAnimNotifyState*, FComboWindowRuntimeState> ActiveComboWindows;
	TMap<const UAnimNotifyState*, FAttackInterruptWindowState> ActiveAttackInterruptWindows;
	TMap<const UAnimNotifyState*, FAttackInterruptWindowState> ActiveAbilityInterruptWindows;
	FTimerHandle PendingComboRetryTimerHandle;
	FTimerHandle CombatExitTimerHandle;
	bool bBufferedAttackInput = false;
	bool bHasActiveAttackType = false;
	bool bHasPendingComboRequest = false;
	bool bCanAdvanceLightCombo = false;
	bool bCanAdvanceHeavyCombo = false;
	bool bIsInCombat = false;
	float BufferedAttackInputExpiryTime = 0.0f;
	FAbilityCastRuntimeState ActiveAbilityCast;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input", meta = (ClampMin = "0.0"))
	float AttackInputBufferDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|State", meta = (ClampMin = "0.1"))
	float CombatExitDelaySeconds = 5.0f;

	mutable TWeakObjectPtr<class UEquipmentComponent> CachedEquipment;
	mutable TWeakObjectPtr<class UCombatImpactComponent> CachedCombatImpact;
	mutable TWeakObjectPtr<class UWeaponActionComponent> CachedWeaponAction;
	mutable TWeakObjectPtr<class UHeroLocomotionComponent> CachedHeroLocomotion;
	mutable TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
};
