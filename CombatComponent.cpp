#include "Components/CombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "Abilities/BaseGameplayAbility.h"
#include "Characters/CharacterBase.h"
#include "Characters/HeroLocomotionComponent.h"
#include "CombatTypes.h"
#include "Components/CombatImpactComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/WeaponActionComponent.h"
#include "DataAssets/WeaponDataAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "WoWCloneGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatSystem, Log, All);

bool UCombatComponent::FAbilityCastRuntimeState::IsActive() const
{
	return SourceAbility.IsValid() && Config.IsValid();
}

void UCombatComponent::FAbilityCastRuntimeState::Reset()
{
	Config = FAbilityCastConfig();
	SourceAbility.Reset();
	AbilityName = FText::GetEmpty();
	StartWorldTime = 0.0f;
	bHasCommitted = false;
}

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());

	if (!OwnerCharacter)
	{
		UE_LOG(LogCombatSystem, Error, TEXT("CombatComponent must be attached to CharacterBase."));
		return;
	}

	if (!GetEquipmentComponent() || !GetCombatImpactComponent() || !GetWeaponActionComponent())
	{
		UE_LOG(LogCombatSystem, Error, TEXT("CombatComponent is missing required sibling components on %s."), *GetNameSafe(OwnerCharacter));
	}

	InitializeCombatState();
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingComboRetryTimerHandle);
		World->GetTimerManager().ClearTimer(CombatExitTimerHandle);
	}

	ClearAbilityCastState(false);

	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->ClearCurrentAttackMontage(CurrentAttackMontage.Get());
	}

	ActiveComboWindows.Reset();
	RefreshComboWindowState();
	ClearBufferedAttackInput();
	CachedWeaponAction.Reset();
	CachedCombatImpact.Reset();
	CachedEquipment.Reset();
	CachedHeroLocomotion.Reset();
	CachedAnimInstance.Reset();
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::InitializeCombatState()
{
	bIsInCombat = false;
	ClearAbilityCastState(false);

	if (UAbilitySystemComponent* AbilitySystemComponent = OwnerCharacter ? OwnerCharacter->GetAbilitySystemComponent() : nullptr)
	{
		HandleCombatTagChange(AbilitySystemComponent, false);
	}

	RevertCombatStateMovementOverrides();
}

void UCombatComponent::NotifyDamageDealt()
{
	UAbilitySystemComponent* AbilitySystemComponent = OwnerCharacter ? OwnerCharacter->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!bIsInCombat)
	{
		bIsInCombat = true;
		EnterCombatState(AbilitySystemComponent);
	}

	RefreshCombatExitTimer();
}

void UCombatComponent::ProcessAttackInput(ECombatAttackType AttackType)
{
	if (!CanPerformAttack())
	{
		return;
	}

	if (CanAdvanceComboForAttackType(AttackType))
	{
		HandleComboInput(AttackType);
		return;
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		if (const UAnimMontage* AttackMontage = CurrentAttackMontage.Get(); AttackMontage && AnimInstance->Montage_IsPlaying(AttackMontage))
		{
			BufferAttackInput(AttackType);
			return;
		}
	}

	HandleInitialInput(AttackType);
}

void UCombatComponent::BeginComboWindow(const UAnimNotifyState* WindowSource, const FComboWindowRequest& ComboWindowRequest)
{
	if (!WindowSource)
	{
		return;
	}

	FComboWindowRuntimeState& WindowState = ActiveComboWindows.FindOrAdd(WindowSource);
	WindowState.Request = ComboWindowRequest;
	++WindowState.ActiveCount;
	RefreshComboWindowState();
	ConsumeBufferedAttackInput();
}

void UCombatComponent::EndComboWindow(const UAnimNotifyState* WindowSource)
{
	if (!WindowSource)
	{
		return;
	}

	if (FComboWindowRuntimeState* WindowState = ActiveComboWindows.Find(WindowSource))
	{
		WindowState->ActiveCount = FMath::Max(0, WindowState->ActiveCount - 1);
		if (WindowState->ActiveCount == 0)
		{
			ActiveComboWindows.Remove(WindowSource);
		}
	}

	RefreshComboWindowState();
}

void UCombatComponent::BeginAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime)
{
	BeginInterruptWindow(ActiveAttackInterruptWindows, WindowSource, AttackMontage, InBlendOutTime);
}

void UCombatComponent::EndAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource)
{
	EndInterruptWindow(ActiveAttackInterruptWindows, WindowSource);
}

void UCombatComponent::BeginAbilityInterruptWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime)
{
	BeginInterruptWindow(ActiveAbilityInterruptWindows, WindowSource, AttackMontage, InBlendOutTime);
}

void UCombatComponent::EndAbilityInterruptWindow(const UAnimNotifyState* WindowSource)
{
	EndInterruptWindow(ActiveAbilityInterruptWindows, WindowSource);
}

void UCombatComponent::BeginMeleeHitWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, const FMeleeHitWindowRequest& HitWindowRequest)
{
	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->BeginMeleeHitWindow(WindowSource, AttackMontage, HitWindowRequest);
	}
}

void UCombatComponent::EndMeleeHitWindow(const UAnimNotifyState* WindowSource)
{
	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->EndMeleeHitWindow(WindowSource);
	}
}

bool UCombatComponent::TryInterruptAttackForMovement()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	const UAnimMontage* AttackMontage = ResolveInterruptibleMontage(AnimInstance, ActiveAttackInterruptWindows);
	if (!AttackMontage)
	{
		return false;
	}

	UE_LOG(LogCombatSystem, Log, TEXT("Attack montage interrupted by movement input."));
	InterruptAttackMontage(AnimInstance, AttackMontage, ResolveInterruptBlendOutTime(AttackMontage, ActiveAttackInterruptWindows));
	return true;
}

bool UCombatComponent::TryBeginAbilityAreaImpact(const FAbilityAreaImpactConfig& AreaImpactConfig, UGameplayAbility* SourceAbility)
{
	if (!CanActivateAbilityAreaImpact(AreaImpactConfig) || !IsValid(SourceAbility))
	{
		return false;
	}

	UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent();
	if (!CombatImpactComp)
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!TryPrepareAbilityAreaImpactActivation(AnimInstance))
	{
		return false;
	}

	CombatImpactComp->BeginAbilityAreaImpact(AreaImpactConfig, SourceAbility);
	return true;
}

bool UCombatComponent::CanActivateAbilityAreaImpact(const FAbilityAreaImpactConfig& AreaImpactConfig, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!OwnerCharacter || !AreaImpactConfig.IsValid())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(WoWCloneTags::AbilityFail_Blocked);
		}

		return false;
	}

	if (HasInterruptibleAbilityCast())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(WoWCloneTags::AbilityFail_Blocked);
		}

		return false;
	}

	const UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent();
	if (!CombatImpactComp || CombatImpactComp->HasActiveAbilityAreaImpact())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(WoWCloneTags::AbilityFail_Blocked);
		}

		return false;
	}

	const UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent();
	if (WeaponActionComp && WeaponActionComp->IsWeaponActionInProgress())
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(WoWCloneTags::AbilityFail_Blocked);
		}

		return false;
	}

	if (!AnimInstance->IsAnyMontagePlaying())
	{
		return true;
	}

	if (!ResolveInterruptibleMontage(AnimInstance, ActiveAbilityInterruptWindows))
	{
		return false;
	}

	return true;
}

void UCombatComponent::EndAbilityAreaImpact(UGameplayAbility* SourceAbility)
{
	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->EndAbilityAreaImpact(SourceAbility);
	}
}

void UCombatComponent::TriggerAbilityAreaImpact(const UAnimMontage* SourceMontage)
{
	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->TriggerAbilityAreaImpact(SourceMontage);
	}
}

bool UCombatComponent::TryBeginAbilityCast(const FAbilityCastConfig& CastConfig, UGameplayAbility* SourceAbility, FGameplayTagContainer* OptionalRelevantTags)
{
	if (!OwnerCharacter || !IsValid(SourceAbility) || !CastConfig.IsValid())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(WoWCloneTags::AbilityFail_Blocked);
		}

		return false;
	}

	if (HasInterruptibleAbilityCast())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(WoWCloneTags::AbilityFail_Blocked);
		}

		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(WoWCloneTags::AbilityFail_Blocked);
		}

		return false;
	}

	ActiveAbilityCast.Reset();
	ActiveAbilityCast.Config = CastConfig;
	ActiveAbilityCast.SourceAbility = SourceAbility;
	ActiveAbilityCast.StartWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (const UBaseGameplayAbility* BaseAbility = Cast<UBaseGameplayAbility>(SourceAbility))
	{
		ActiveAbilityCast.AbilityName = BaseAbility->AbilityName.IsEmpty()
			? FText::FromString(SourceAbility->GetClass()->GetName())
			: BaseAbility->AbilityName;
	}
	else
	{
		ActiveAbilityCast.AbilityName = FText::FromString(SourceAbility->GetClass()->GetName());
	}

	SetAbilityCastingTag(true);
	BroadcastAbilityCastState();
	return true;
}

void UCombatComponent::EndAbilityCast(UGameplayAbility* SourceAbility)
{
	if (!ActiveAbilityCast.IsActive())
	{
		return;
	}

	if (SourceAbility && ActiveAbilityCast.SourceAbility.Get() != SourceAbility)
	{
		return;
	}

	ClearAbilityCastState(true);
}

bool UCombatComponent::NotifyAbilityCastCommit(const UAnimMontage* SourceMontage, TSubclassOf<UAnimNotify> NotifyClass)
{
	if (!HasInterruptibleAbilityCast())
	{
		return false;
	}

	if (ActiveAbilityCast.Config.CastMontage != SourceMontage
		|| ActiveAbilityCast.Config.CommitNotifyClass != NotifyClass)
	{
		return false;
	}

	UBaseGameplayAbility* BaseAbility = Cast<UBaseGameplayAbility>(ActiveAbilityCast.SourceAbility.Get());
	if (BaseAbility && !BaseAbility->CommitAbilityFromAnimation())
	{
		InterruptAbilityCast(ActiveAbilityCast.Config.InterruptBlendOutTime, TEXT("commit failure"));
		return false;
	}

	if (BaseAbility)
	{
		BaseAbility->HandleAbilityAnimationCommit(SourceMontage);
	}

	ActiveAbilityCast.bHasCommitted = true;
	SetAbilityCastingTag(false);
	BroadcastAbilityCastState();
	return true;
}

void UCombatComponent::HandleReceivedDamage(float AppliedDamage)
{
	if (AppliedDamage <= 0.0f)
	{
		return;
	}
}

FAbilityCastState UCombatComponent::GetAbilityCastState() const
{
	FAbilityCastState CastState;
	if (!HasInterruptibleAbilityCast())
	{
		return CastState;
	}

	CastState.bIsCasting = true;
	CastState.AbilityName = ActiveAbilityCast.AbilityName;
	CastState.TotalDuration = ActiveAbilityCast.Config.CastDuration;

	if (const UWorld* World = GetWorld())
	{
		CastState.ElapsedTime = FMath::Clamp(World->GetTimeSeconds() - ActiveAbilityCast.StartWorldTime, 0.0f, CastState.TotalDuration);
	}

	CastState.RemainingTime = FMath::Max(0.0f, CastState.TotalDuration - CastState.ElapsedTime);
	return CastState;
}

bool UCombatComponent::CanPerformAttack() const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	if (HasInterruptibleAbilityCast())
	{
		return false;
	}

	const UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent();
	const UEquipmentComponent* EquipmentComp = GetEquipmentComponent();
	if (!WeaponActionComp || !EquipmentComp || WeaponActionComp->IsWeaponActionInProgress())
	{
		return false;
	}

	const bool bHasWeapon = EquipmentComp->HasItemEquippedAtSlot(EEquipmentSlot::MainHand);
	const bool bIsArmed = EquipmentComp->HasWeaponEquipped();
	return bHasWeapon && bIsArmed;
}

bool UCombatComponent::TryPrepareAbilityAreaImpactActivation(UAnimInstance* AnimInstance)
{
	if (!AnimInstance)
	{
		return false;
	}

	if (!AnimInstance->IsAnyMontagePlaying())
	{
		return true;
	}

	const UAnimMontage* AttackMontage = ResolveInterruptibleMontage(AnimInstance, ActiveAbilityInterruptWindows);
	if (!AttackMontage)
	{
		return false;
	}

	UE_LOG(LogCombatSystem, Log, TEXT("Attack montage interrupted by ability area impact activation."));
	InterruptAttackMontage(AnimInstance, AttackMontage, ResolveInterruptBlendOutTime(AttackMontage, ActiveAbilityInterruptWindows));
	return true;
}

void UCombatComponent::BeginInterruptWindow(TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates, const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime)
{
	if (!WindowSource || !AttackMontage)
	{
		return;
	}

	FAttackInterruptWindowState& WindowState = WindowStates.FindOrAdd(WindowSource);
	WindowState.Montage = AttackMontage;
	WindowState.BlendOutTime = FMath::Max(0.0f, InBlendOutTime);
	++WindowState.ActiveCount;
}

void UCombatComponent::EndInterruptWindow(TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates, const UAnimNotifyState* WindowSource)
{
	if (!WindowSource)
	{
		return;
	}

	if (FAttackInterruptWindowState* WindowState = WindowStates.Find(WindowSource))
	{
		WindowState->ActiveCount = FMath::Max(0, WindowState->ActiveCount - 1);
		if (WindowState->ActiveCount == 0)
		{
			WindowStates.Remove(WindowSource);
		}
	}
}

void UCombatComponent::InterruptAttackMontage(UAnimInstance* AnimInstance, const UAnimMontage* AttackMontage, float BlendOutTime)
{
	if (!AnimInstance || !AttackMontage)
	{
		return;
	}

	AnimInstance->Montage_Stop(BlendOutTime, AttackMontage);
	ClearInterruptWindowsForMontage(ActiveAttackInterruptWindows, AttackMontage);
	ClearInterruptWindowsForMontage(ActiveAbilityInterruptWindows, AttackMontage);

	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->ClearMeleeHitWindowsForMontage(AttackMontage);
		CombatImpactComp->ClearCurrentAttackMontage(AttackMontage);
	}

	CurrentAttackMontage.Reset();
	ResetCombo();
	ClearBufferedAttackInput();
}

bool UCombatComponent::CanAdvanceComboForAttackType(ECombatAttackType AttackType) const
{
	if (!bCanAdvanceCombo)
	{
		return false;
	}

	return AttackType == ECombatAttackType::Heavy ? bCanAdvanceHeavyCombo : bCanAdvanceLightCombo;
}

void UCombatComponent::HandleComboInput(ECombatAttackType AttackType)
{
	UE_LOG(LogCombatSystem, Log, TEXT("Advancing %s combo. Index: %d"), AttackType == ECombatAttackType::Heavy ? TEXT("Heavy Attack") : TEXT("Light Attack"), CurrentComboIndex);
	ClearBufferedAttackInput();
	ExecuteNextComboStep(AttackType);
}

void UCombatComponent::HandleInitialInput(ECombatAttackType AttackType)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || AnimInstance->IsAnyMontagePlaying())
	{
		return;
	}

	ClearBufferedAttackInput();
	RefreshCurrentComboIndex(AttackType);
	ExecuteNextComboStep(AttackType);
}

void UCombatComponent::ExecuteNextComboStep(ECombatAttackType AttackType)
{
	UWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	const FCombatComboData* ComboData = GetComboDataForAttackType(WeaponData, AttackType);
	if (!IsComboStateValid(ComboData))
	{
		return;
	}

	AdvanceComboState(*ComboData, AttackType);
	PlayComboAttack(WeaponData, AttackType, GetComboIndexForAttackType(AttackType) - 1);
}

bool UCombatComponent::IsComboStateValid(const FCombatComboData* ComboData) const
{
	return ComboData && ComboData->ComboMontages.Num() > 0;
}

void UCombatComponent::AdvanceComboState(const FCombatComboData& ComboData, ECombatAttackType AttackType)
{
	int32& AttackTypeComboIndex = GetMutableComboIndexForAttackType(AttackType);
	const int32 MaxCombos = ComboData.ComboMontages.Num();
	if (AttackTypeComboIndex >= MaxCombos)
	{
		AttackTypeComboIndex = 0;
	}

	++AttackTypeComboIndex;
	RefreshCurrentComboIndex(AttackType);
	bCanAdvanceCombo = false;
	ActiveAttackType = AttackType;
	bHasActiveAttackType = true;

	UE_LOG(LogCombatSystem, Log, TEXT("%s combo state advanced. Next Index: %d"), AttackType == ECombatAttackType::Heavy ? TEXT("Heavy Attack") : TEXT("Light Attack"), GetComboIndexForAttackType(AttackType));
}

void UCombatComponent::PlayComboAttack(UWeaponDataAsset* WeaponData, ECombatAttackType AttackType, int32 Index)
{
	if (!WeaponData || !OwnerCharacter)
	{
		return;
	}

	const FCombatComboData* ComboData = GetComboDataForAttackType(WeaponData, AttackType);
	if (!ComboData)
	{
		return;
	}

	const TArray<TSoftObjectPtr<UAnimMontage>>& Combos = ComboData->ComboMontages;
	if (!Combos.IsValidIndex(Index))
	{
		return;
	}

	const TSoftObjectPtr<UAnimMontage>& ComboMontage = Combos[Index];
	if (ComboMontage.IsPending())
	{
		RequestComboMontageLoad(WeaponData, AttackType, Index);
		return;
	}

	if (ComboMontage.IsValid())
	{
		ClearPendingComboRequest();
		RecordPlayedAttackMontage(ComboMontage.Get(), OwnerCharacter->PlayAnimMontage(ComboMontage.Get()), GetComboStepKnockbackConfig(ComboData, Index));
	}
}

void UCombatComponent::RequestComboMontageLoad(UWeaponDataAsset* WeaponData, ECombatAttackType AttackType, int32 Index)
{
	if (!WeaponData)
	{
		return;
	}

	const FCombatComboData* ComboData = GetComboDataForAttackType(WeaponData, AttackType);
	if (!ComboData)
	{
		return;
	}

	const TArray<TSoftObjectPtr<UAnimMontage>>& Combos = ComboData->ComboMontages;
	if (!Combos.IsValidIndex(Index))
	{
		return;
	}

	PendingComboWeaponData = WeaponData;
	PendingComboAttackType = AttackType;
	PendingComboMontageIndex = Index;
	bHasPendingComboRequest = true;

	FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UCombatComponent::OnComboMontageLoaded, WeaponData, AttackType, Index);
	UAssetManager::GetStreamableManager().RequestAsyncLoad(Combos[Index].ToSoftObjectPath(), Delegate);
}

void UCombatComponent::OnComboMontageLoaded(UWeaponDataAsset* WeaponData, ECombatAttackType AttackType, int32 Index)
{
	if (!bHasPendingComboRequest
		|| PendingComboWeaponData.Get() != WeaponData
		|| PendingComboAttackType != AttackType
		|| PendingComboMontageIndex != Index)
	{
		return;
	}

	if (!OwnerCharacter || GetEquippedWeaponData() != WeaponData)
	{
		ClearPendingComboRequest();
		return;
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		if (AnimInstance->IsAnyMontagePlaying())
		{
			const UAnimMontage* ActiveAttackMontage = CurrentAttackMontage.Get();
			if (ActiveAttackMontage && AnimInstance->Montage_IsPlaying(ActiveAttackMontage))
			{
				return;
			}

			ClearPendingComboRequest();
			return;
		}
	}

	const FCombatComboData* ComboData = GetComboDataForAttackType(WeaponData, AttackType);
	if (!ComboData)
	{
		ClearPendingComboRequest();
		return;
	}

	const TArray<TSoftObjectPtr<UAnimMontage>>& Combos = ComboData->ComboMontages;
	if (Combos.IsValidIndex(Index) && Combos[Index].IsValid())
	{
		ClearPendingComboRequest();
		RecordPlayedAttackMontage(Combos[Index].Get(), OwnerCharacter->PlayAnimMontage(Combos[Index].Get()), GetComboStepKnockbackConfig(ComboData, Index));
		return;
	}

	ClearPendingComboRequest();
}

UWeaponDataAsset* UCombatComponent::GetEquippedWeaponData() const
{
	if (const UEquipmentComponent* EquipmentComp = GetEquipmentComponent())
	{
		return Cast<UWeaponDataAsset>(EquipmentComp->GetEquippedItem(EEquipmentSlot::MainHand));
	}

	return nullptr;
}

const FCombatComboData* UCombatComponent::GetComboDataForAttackType(const UWeaponDataAsset* WeaponData, ECombatAttackType AttackType) const
{
	if (!WeaponData)
	{
		return nullptr;
	}

	return AttackType == ECombatAttackType::Heavy
		? &WeaponData->WeaponData.HeavyAttackComboData
		: &WeaponData->WeaponData.ComboData;
}

FMeleeKnockbackConfig UCombatComponent::GetComboStepKnockbackConfig(const FCombatComboData* ComboData, int32 Index) const
{
	if (!ComboData || !ComboData->ComboStepKnockbackConfigs.IsValidIndex(Index))
	{
		return FMeleeKnockbackConfig();
	}

	return ComboData->ComboStepKnockbackConfigs[Index];
}

int32 UCombatComponent::GetComboIndexForAttackType(ECombatAttackType AttackType) const
{
	return AttackType == ECombatAttackType::Heavy ? HeavyComboIndex : LightComboIndex;
}

int32& UCombatComponent::GetMutableComboIndexForAttackType(ECombatAttackType AttackType)
{
	return AttackType == ECombatAttackType::Heavy ? HeavyComboIndex : LightComboIndex;
}

void UCombatComponent::RefreshCurrentComboIndex(ECombatAttackType AttackType)
{
	CurrentComboIndex = GetComboIndexForAttackType(AttackType);
}

void UCombatComponent::ResetComboState(ECombatAttackType AttackType)
{
	GetMutableComboIndexForAttackType(AttackType) = 0;

	if (bHasActiveAttackType && ActiveAttackType == AttackType)
	{
		RefreshCurrentComboIndex(AttackType);
	}
}

UEquipmentComponent* UCombatComponent::GetEquipmentComponent() const
{
	if (!CachedEquipment.IsValid() && OwnerCharacter)
	{
		CachedEquipment = OwnerCharacter->FindComponentByClass<UEquipmentComponent>();
	}

	return CachedEquipment.Get();
}

UCombatImpactComponent* UCombatComponent::GetCombatImpactComponent() const
{
	if (!CachedCombatImpact.IsValid() && OwnerCharacter)
	{
		CachedCombatImpact = OwnerCharacter->FindComponentByClass<UCombatImpactComponent>();
	}

	return CachedCombatImpact.Get();
}

UWeaponActionComponent* UCombatComponent::GetWeaponActionComponent() const
{
	if (!CachedWeaponAction.IsValid() && OwnerCharacter)
	{
		CachedWeaponAction = OwnerCharacter->FindComponentByClass<UWeaponActionComponent>();
	}

	return CachedWeaponAction.Get();
}

UHeroLocomotionComponent* UCombatComponent::GetHeroLocomotionComponent() const
{
	if (!CachedHeroLocomotion.IsValid() && OwnerCharacter)
	{
		CachedHeroLocomotion = OwnerCharacter->FindComponentByClass<UHeroLocomotionComponent>();
	}

	return CachedHeroLocomotion.Get();
}

UAnimInstance* UCombatComponent::GetAnimInstance() const
{
	if (!CachedAnimInstance.IsValid() && OwnerCharacter && OwnerCharacter->GetMesh())
	{
		CachedAnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	}

	return CachedAnimInstance.Get();
}

void UCombatComponent::ResetCombo()
{
	CurrentComboIndex = 0;
	ResetComboState(ECombatAttackType::Light);
	ResetComboState(ECombatAttackType::Heavy);
	ActiveComboWindows.Reset();
	RefreshComboWindowState();
	bCanAdvanceCombo = false;
	bHasActiveAttackType = false;
	ActiveAttackType = ECombatAttackType::Light;
	ClearBufferedAttackInput();
	ClearPendingComboRequest();
}

void UCombatComponent::ClearPendingComboRequest()
{
	PendingComboWeaponData.Reset();
	PendingComboAttackType = ECombatAttackType::Light;
	PendingComboMontageIndex = INDEX_NONE;
	bHasPendingComboRequest = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingComboRetryTimerHandle);
	}
}

void UCombatComponent::SchedulePendingComboRetry()
{
	UWorld* World = GetWorld();
	if (!World || !PendingComboWeaponData.IsValid() || PendingComboMontageIndex == INDEX_NONE)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(PendingComboRetryTimerHandle))
	{
		return;
	}

	FTimerDelegate RetryDelegate;
	RetryDelegate.BindUObject(this, &UCombatComponent::RetryPendingComboRequest);
	World->GetTimerManager().SetTimer(PendingComboRetryTimerHandle, RetryDelegate, KINDA_SMALL_NUMBER, false);
}

void UCombatComponent::RetryPendingComboRequest()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingComboRetryTimerHandle);
	}

	UWeaponDataAsset* PendingWeaponData = PendingComboWeaponData.Get();
	if (!PendingWeaponData || PendingComboMontageIndex == INDEX_NONE || !bHasPendingComboRequest)
	{
		return;
	}

	OnComboMontageLoaded(PendingWeaponData, PendingComboAttackType, PendingComboMontageIndex);
}

void UCombatComponent::BufferAttackInput(ECombatAttackType AttackType)
{
	if (AttackInputBufferDuration <= 0.0f)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		bBufferedAttackInput = true;
		BufferedAttackType = AttackType;
		BufferedAttackInputExpiryTime = World->GetTimeSeconds() + AttackInputBufferDuration;
	}
}

void UCombatComponent::ConsumeBufferedAttackInput()
{
	if (CanAdvanceComboForAttackType(BufferedAttackType) && HasBufferedAttackInput())
	{
		HandleComboInput(BufferedAttackType);
	}
}

void UCombatComponent::ClearBufferedAttackInput()
{
	bBufferedAttackInput = false;
	BufferedAttackType = ECombatAttackType::Light;
	BufferedAttackInputExpiryTime = 0.0f;
}

bool UCombatComponent::HasBufferedAttackInput() const
{
	if (!bBufferedAttackInput)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() <= BufferedAttackInputExpiryTime;
}

void UCombatComponent::RecordPlayedAttackMontage(UAnimMontage* PlayedMontage, float PlayedDuration, const FMeleeKnockbackConfig& KnockbackConfig)
{
	if (!PlayedMontage || PlayedDuration <= 0.0f)
	{
		return;
	}

	CurrentAttackMontage = PlayedMontage;

	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->SetCurrentAttackMontage(PlayedMontage, KnockbackConfig);
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		FOnMontageBlendingOutStarted BlendOutDelegate;
		BlendOutDelegate.BindUObject(this, &UCombatComponent::OnAttackMontageBlendingOut);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, PlayedMontage);
	}
}

void UCombatComponent::OnAttackMontageBlendingOut(UAnimMontage* AttackMontage, bool bInterrupted)
{
	(void)bInterrupted;

	if (!AttackMontage)
	{
		return;
	}

	ActiveComboWindows.Reset();
	RefreshComboWindowState();
	ClearInterruptWindowsForMontage(ActiveAttackInterruptWindows, AttackMontage);
	ClearInterruptWindowsForMontage(ActiveAbilityInterruptWindows, AttackMontage);

	if (UCombatImpactComponent* CombatImpactComp = GetCombatImpactComponent())
	{
		CombatImpactComp->ClearMeleeHitWindowsForMontage(AttackMontage);
		CombatImpactComp->ClearCurrentAttackMontage(AttackMontage);
	}

	ClearBufferedAttackInput();

	if (CurrentAttackMontage.Get() == AttackMontage)
	{
		CurrentAttackMontage.Reset();
	}

	if (bHasActiveAttackType)
	{
		RefreshCurrentComboIndex(ActiveAttackType);
	}

	if (PendingComboWeaponData.IsValid() && PendingComboMontageIndex != INDEX_NONE)
	{
		SchedulePendingComboRetry();
	}
}

void UCombatComponent::RefreshComboWindowState()
{
	bCanAdvanceCombo = false;
	bCanAdvanceLightCombo = false;
	bCanAdvanceHeavyCombo = false;

	for (const TPair<const UAnimNotifyState*, FComboWindowRuntimeState>& Entry : ActiveComboWindows)
	{
		if (Entry.Value.ActiveCount <= 0)
		{
			continue;
		}

		bCanAdvanceLightCombo |= Entry.Value.Request.bAllowLightAttack;
		bCanAdvanceHeavyCombo |= Entry.Value.Request.bAllowHeavyAttack;
	}

	bCanAdvanceCombo = bCanAdvanceLightCombo || bCanAdvanceHeavyCombo;
}

void UCombatComponent::ClearInterruptWindowsForMontage(TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates, const UAnimMontage* AttackMontage)
{
	if (!AttackMontage)
	{
		return;
	}

	for (auto It = WindowStates.CreateIterator(); It; ++It)
	{
		if (It.Value().Montage == AttackMontage)
		{
			It.RemoveCurrent();
		}
	}
}

const UAnimMontage* UCombatComponent::ResolveInterruptibleMontage(UAnimInstance* AnimInstance, const TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates) const
{
	const UAnimMontage* AttackMontage = CurrentAttackMontage.Get();
	if (!AttackMontage || !AnimInstance->Montage_IsPlaying(AttackMontage))
	{
		return nullptr;
	}

	for (const TPair<const UAnimNotifyState*, FAttackInterruptWindowState>& Entry : WindowStates)
	{
		const FAttackInterruptWindowState& WindowState = Entry.Value;
		if (WindowState.ActiveCount > 0 && WindowState.Montage == AttackMontage)
		{
			return AttackMontage;
		}
	}

	return nullptr;
}

float UCombatComponent::ResolveInterruptBlendOutTime(const UAnimMontage* AttackMontage, const TMap<const UAnimNotifyState*, FAttackInterruptWindowState>& WindowStates) const
{
	float BlendOutTime = 0.25f;
	bool bFoundMatchingWindow = false;

	for (const TPair<const UAnimNotifyState*, FAttackInterruptWindowState>& Entry : WindowStates)
	{
		const FAttackInterruptWindowState& WindowState = Entry.Value;
		if (WindowState.ActiveCount > 0 && WindowState.Montage == AttackMontage)
		{
			BlendOutTime = bFoundMatchingWindow ? FMath::Max(BlendOutTime, WindowState.BlendOutTime) : WindowState.BlendOutTime;
			bFoundMatchingWindow = true;
		}
	}

	return BlendOutTime;
}

void UCombatComponent::EnterCombatState(UAbilitySystemComponent* AbilitySystemComponent)
{
	HandleCombatTagChange(AbilitySystemComponent, true);
	ApplyCombatStateMovementOverrides();
}

void UCombatComponent::ExitCombatState(UAbilitySystemComponent* AbilitySystemComponent)
{
	HandleCombatTagChange(AbilitySystemComponent, false);
	RevertCombatStateMovementOverrides();
}

void UCombatComponent::HandleCombatTagChange(UAbilitySystemComponent* AbilitySystemComponent, bool bEnteringCombat)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag& TagToRemove = bEnteringCombat ? WoWCloneTags::State_Uncombat : WoWCloneTags::State_Combat;
	const FGameplayTag& TagToAdd = bEnteringCombat ? WoWCloneTags::State_Combat : WoWCloneTags::State_Uncombat;
	AbilitySystemComponent->RemoveLooseGameplayTag(TagToRemove);
	AbilitySystemComponent->AddLooseGameplayTag(TagToAdd);
}

void UCombatComponent::ApplyCombatStateMovementOverrides()
{
	if (UHeroLocomotionComponent* HeroLocomotionComponent = GetHeroLocomotionComponent())
	{
		HeroLocomotionComponent->ApplyCombatStateOverrides();
	}
}

void UCombatComponent::RevertCombatStateMovementOverrides()
{
	if (UHeroLocomotionComponent* HeroLocomotionComponent = GetHeroLocomotionComponent())
	{
		HeroLocomotionComponent->RevertCombatStateOverrides();
	}
}

void UCombatComponent::RefreshCombatExitTimer()
{
	UWorld* World = GetWorld();
	if (!World || CombatExitDelaySeconds <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CombatExitTimerHandle,
		this,
		&UCombatComponent::HandleCombatExitTimeout,
		CombatExitDelaySeconds,
		false);
}

void UCombatComponent::HandleCombatExitTimeout()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CombatExitTimerHandle);
	}

	if (!bIsInCombat)
	{
		return;
	}

	bIsInCombat = false;

	if (UAbilitySystemComponent* AbilitySystemComponent = OwnerCharacter ? OwnerCharacter->GetAbilitySystemComponent() : nullptr)
	{
		ExitCombatState(AbilitySystemComponent);
	}
}

bool UCombatComponent::HasInterruptibleAbilityCast() const
{
	return ActiveAbilityCast.IsActive() && !ActiveAbilityCast.bHasCommitted;
}

void UCombatComponent::BroadcastAbilityCastState()
{
	OnAbilityCastStateChanged.Broadcast(GetAbilityCastState());
}

void UCombatComponent::ClearAbilityCastState(bool bBroadcastStateChanged)
{
	if (!ActiveAbilityCast.IsActive() && !ActiveAbilityCast.bHasCommitted)
	{
		return;
	}

	SetAbilityCastingTag(false);
	ActiveAbilityCast.Reset();

	if (bBroadcastStateChanged)
	{
		BroadcastAbilityCastState();
	}
}

void UCombatComponent::SetAbilityCastingTag(bool bEnable) const
{
	UAbilitySystemComponent* AbilitySystemComponent = OwnerCharacter ? OwnerCharacter->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (bEnable)
	{
		AbilitySystemComponent->AddLooseGameplayTag(WoWCloneTags::State_Casting);
		return;
	}

	AbilitySystemComponent->RemoveLooseGameplayTag(WoWCloneTags::State_Casting);
}

void UCombatComponent::InterruptAbilityCast(float BlendOutTime, const TCHAR* DebugReason)
{
	if (!ActiveAbilityCast.IsActive())
	{
		return;
	}

	UE_LOG(LogCombatSystem, Log, TEXT("Ability cast interrupted by %s."), DebugReason ? DebugReason : TEXT("unknown reason"));

	if (UBaseGameplayAbility* BaseAbility = Cast<UBaseGameplayAbility>(ActiveAbilityCast.SourceAbility.Get()))
	{
		BaseAbility->HandlePreCommitCancellation();
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		AnimInstance->Montage_Stop(FMath::Max(0.0f, BlendOutTime), ActiveAbilityCast.Config.CastMontage);
	}

	ClearAbilityCastState(true);
}
