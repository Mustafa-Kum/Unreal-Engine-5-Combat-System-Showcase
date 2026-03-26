#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataAssets/WeaponDataAsset.h"
#include "Engine/DamageEvents.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "CombatComponent.generated.h"

class UAnimMontage;
class UAnimNotifyState;
class UPrimitiveComponent;

struct FWoWCloneCombatDamageEvent : public FDamageEvent
{
	static const int32 ClassID = 0x57A9D114;

	bool bIsCriticalHit = false;

	FWoWCloneCombatDamageEvent()
		: FDamageEvent(UDamageType::StaticClass())
	{
	}

	explicit FWoWCloneCombatDamageEvent(bool bInIsCriticalHit)
		: FDamageEvent(UDamageType::StaticClass())
		, bIsCriticalHit(bInIsCriticalHit)
	{
	}

	virtual int32 GetTypeID() const override
	{
		return FWoWCloneCombatDamageEvent::ClassID;
	}

	virtual bool IsOfType(int32 InID) const override
	{
		return InID == FWoWCloneCombatDamageEvent::ClassID || FDamageEvent::IsOfType(InID);
	}
};

/**
 * AAA Combat Component: Manages combo state and montage execution.
 * Optimized with component caching and orchestration for high performance.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WOWCLONE_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCombatComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	/** --- COMBAT INTERFACE --- */
	
	/** Main entry point for attack input - Orchestration Layer */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ProcessAttack();

	/** Called by AnimNotifies to open/close the combo window */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetCanAdvanceCombo(bool bInCanAdvance);

	/** Called by attack notify states to register the active movement interrupt window */
	void BeginAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime);

	/** Called by attack notify states to unregister the active movement interrupt window */
	void EndAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource);

	/** Called by attack notify states to register the active melee hit window */
	void BeginMeleeHitWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage);

	/** Called by attack notify states to unregister the active melee hit window */
	void EndMeleeHitWindow(const UAnimNotifyState* WindowSource);

	/** Resets the combo to the first attack */
	void ResetCombo();

	/** Stops the active attack montage if movement-cancel is currently allowed */
	bool TryInterruptAttackForMovement();

protected:
	/** --- STATE TRACKING --- */
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	int32 CurrentComboIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bCanAdvanceCombo = false;

	/** --- AAA ORCHESTRATION --- */
	
	[[nodiscard]] bool CanPerformAttack() const;
	void HandleComboInput();
	void HandleInitialInput();
	void ExecuteNextComboStep();
	void ClearPendingComboRequest();
	void BufferAttackInput();
	void ConsumeBufferedAttackInput();
	void ClearBufferedAttackInput();
	[[nodiscard]] bool HasBufferedAttackInput() const;

	[[nodiscard]] bool IsComboStateValid(const class UWeaponDataAsset* WeaponData) const;
	void AdvanceComboState(const class UWeaponDataAsset* WeaponData);

	void PlayComboAttack(class UWeaponDataAsset* WeaponData, int32 Index);
	void RequestComboMontageLoad(class UWeaponDataAsset* WeaponData, int32 Index);
	void OnComboMontageLoaded(class UWeaponDataAsset* WeaponData, int32 Index);
	
	/** --- PERFORMANCE UTILS & CACHING --- */
	[[nodiscard]] class UWeaponDataAsset* GetEquippedWeaponData() const;
	[[nodiscard]] class UInventoryComponent* GetInventoryComponent() const;
	[[nodiscard]] class UAnimInstance* GetAnimInstance() const;

private:
	UFUNCTION()
	void HandleWeaponHitOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	struct FAttackInterruptWindowState
	{
		const UAnimMontage* Montage = nullptr;
		float BlendOutTime = 0.25f;
		int32 ActiveCount = 0;
	};

	struct FMeleeHitWindowState
	{
		const UAnimMontage* Montage = nullptr;
		int32 ActiveCount = 0;
	};

	struct FHitStopActorState
	{
		TWeakObjectPtr<AActor> Actor;
		float PreviousCustomTimeDilation = 1.0f;
	};

	void RecordPlayedAttackMontage(UAnimMontage* PlayedMontage, float PlayedDuration);
	void OnAttackMontageBlendingOut(UAnimMontage* AttackMontage, bool bInterrupted);
	void ClearAttackInterruptWindowsForMontage(const UAnimMontage* AttackMontage);
	void ClearMeleeHitWindowsForMontage(const UAnimMontage* AttackMontage);
	void RefreshWeaponHitCollisionState();
	void TraceReliableMeleeHits();
	void ResetReliableMeleeTraceState();
	[[nodiscard]] bool CanRegisterMeleeHit(AActor* OtherActor) const;
	[[nodiscard]] float ResolveOutgoingMeleeDamage(bool& bOutIsCriticalHit) const;
	[[nodiscard]] bool RollCriticalHit() const;
	[[nodiscard]] FVector ResolveMeleeTraceOrigin() const;
	[[nodiscard]] float ResolveMeleeTraceRadius() const;
	void ApplyMeleeHitToActor(AActor* HitActor);
	void TryPlayHitCameraShake() const;
	void TryApplyHitStop(AActor* HitActor);
	void RestoreHitStop(int32 HitStopRequestId);
	void RestoreActiveHitStop();
	[[nodiscard]] const UAnimMontage* ResolveInterruptibleAttackMontage(UAnimInstance* AnimInstance) const;
	[[nodiscard]] float ResolveInterruptBlendOutTime(const UAnimMontage* AttackMontage) const;

	UPROPERTY(Transient)
	TObjectPtr<class ACharacterBase> OwnerCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<class UWeaponDataAsset> PendingComboWeaponData;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimMontage> CurrentAttackMontage;

	int32 PendingComboMontageIndex = INDEX_NONE;
	TMap<const UAnimNotifyState*, FAttackInterruptWindowState> ActiveAttackInterruptWindows;
	TMap<const UAnimNotifyState*, FMeleeHitWindowState> ActiveMeleeHitWindows;
	TSet<TObjectKey<AActor>> HitActorsInActiveMeleeWindow;
	TArray<FHitStopActorState> ActiveHitStopActors;
	FTimerHandle HitStopRestoreTimerHandle;
	int32 ActiveHitStopRequestId = 0;
	bool bBufferedAttackInput = false;
	float BufferedAttackInputExpiryTime = 0.0f;
	bool bHasReliableTraceOrigin = false;
	FVector PreviousReliableTraceOrigin = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Input", meta = (ClampMin = "0.0"))
	float AttackInputBufferDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Damage", meta = (ClampMin = "1.0"))
	float CriticalDamageMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Detection")
	bool bEnableReliableMeleeTracing = true;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Detection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReliableMeleeTraceRadiusScale = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Detection", meta = (ClampMin = "0.0"))
	float ReliableMeleeTraceMinRadius = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Hit Detection", meta = (ClampMin = "0.0"))
	float ReliableMeleeTraceMaxRadius = 28.0f;

	// AAA Performance: Cached references to avoid redundant FindComponentByClass lookups
	mutable TWeakObjectPtr<class UInventoryComponent> CachedInventory;
	mutable TWeakObjectPtr<class UAnimInstance> CachedAnimInstance;
};
