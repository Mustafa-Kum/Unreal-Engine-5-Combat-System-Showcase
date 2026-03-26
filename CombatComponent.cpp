#include "Components/CombatComponent.h"
#include "Characters/CharacterBase.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/PlayerController.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatSystem, Log, All);

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = false;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());

	if (UPrimitiveComponent* HitCollisionComp = OwnerCharacter ? OwnerCharacter->GetMeleeHitCollisionComponent() : nullptr)
	{
		HitCollisionComp->OnComponentBeginOverlap.AddDynamic(this, &UCombatComponent::HandleWeaponHitOverlap);
	}
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPrimitiveComponent* HitCollisionComp = OwnerCharacter ? OwnerCharacter->GetMeleeHitCollisionComponent() : nullptr)
	{
		HitCollisionComp->OnComponentBeginOverlap.RemoveDynamic(this, &UCombatComponent::HandleWeaponHitOverlap);
	}

	RestoreActiveHitStop();
	HitActorsInActiveMeleeWindow.Reset();
	ClearBufferedAttackInput();
	ResetReliableMeleeTraceState();
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::ProcessAttack()
{
	if (!CanPerformAttack())
	{
		return;
	}

	if (bCanAdvanceCombo)
	{
		HandleComboInput();
		return;
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		if (const UAnimMontage* AttackMontage = CurrentAttackMontage.Get(); AttackMontage && AnimInstance->Montage_IsPlaying(AttackMontage))
		{
			BufferAttackInput();
			return;
		}
	}

	HandleInitialInput();
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnableReliableMeleeTracing || ActiveMeleeHitWindows.IsEmpty())
	{
		return;
	}

	TraceReliableMeleeHits();
}

void UCombatComponent::SetCanAdvanceCombo(bool bInCanAdvance)
{
	bCanAdvanceCombo = bInCanAdvance;

	if (bCanAdvanceCombo)
	{
		ConsumeBufferedAttackInput();
	}
}

void UCombatComponent::BeginAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage, float InBlendOutTime)
{
	if (!WindowSource || !AttackMontage)
	{
		return;
	}

	FAttackInterruptWindowState& WindowState = ActiveAttackInterruptWindows.FindOrAdd(WindowSource);
	WindowState.Montage = AttackMontage;
	WindowState.BlendOutTime = FMath::Max(0.0f, InBlendOutTime);
	++WindowState.ActiveCount;
}

void UCombatComponent::EndAttackMoveInterruptWindow(const UAnimNotifyState* WindowSource)
{
	if (!WindowSource)
	{
		return;
	}

	if (FAttackInterruptWindowState* WindowState = ActiveAttackInterruptWindows.Find(WindowSource))
	{
		WindowState->ActiveCount = FMath::Max(0, WindowState->ActiveCount - 1);
		if (WindowState->ActiveCount == 0)
		{
			ActiveAttackInterruptWindows.Remove(WindowSource);
		}
	}
}

void UCombatComponent::BeginMeleeHitWindow(const UAnimNotifyState* WindowSource, const UAnimMontage* AttackMontage)
{
	if (!WindowSource || !AttackMontage)
	{
		return;
	}

	FMeleeHitWindowState& WindowState = ActiveMeleeHitWindows.FindOrAdd(WindowSource);
	WindowState.Montage = AttackMontage;
	++WindowState.ActiveCount;

	ResetReliableMeleeTraceState();
	RefreshWeaponHitCollisionState();
}

void UCombatComponent::EndMeleeHitWindow(const UAnimNotifyState* WindowSource)
{
	if (!WindowSource)
	{
		return;
	}

	if (FMeleeHitWindowState* WindowState = ActiveMeleeHitWindows.Find(WindowSource))
	{
		WindowState->ActiveCount = FMath::Max(0, WindowState->ActiveCount - 1);
		if (WindowState->ActiveCount == 0)
		{
			ActiveMeleeHitWindows.Remove(WindowSource);
		}
	}

	RefreshWeaponHitCollisionState();
}

bool UCombatComponent::TryInterruptAttackForMovement()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	const UAnimMontage* AttackMontage = ResolveInterruptibleAttackMontage(AnimInstance);
	if (!AttackMontage)
	{
		return false;
	}

	UE_LOG(LogCombatSystem, Log, TEXT("Attack montage interrupted by movement input."));

	AnimInstance->Montage_Stop(ResolveInterruptBlendOutTime(AttackMontage), AttackMontage);
	ClearAttackInterruptWindowsForMontage(AttackMontage);
	ClearMeleeHitWindowsForMontage(AttackMontage);
	CurrentAttackMontage.Reset();
	ResetCombo();
	ClearBufferedAttackInput();

	return true;
}

void UCombatComponent::HandleWeaponHitOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!CanRegisterMeleeHit(OtherActor))
	{
		return;
	}

	ApplyMeleeHitToActor(OtherActor);
}

bool UCombatComponent::CanPerformAttack() const
{
	if (!OwnerCharacter) return false;

	UInventoryComponent* Inv = GetInventoryComponent();
	if (!Inv || Inv->IsWeaponActionInProgress()) return false;

	// Requirement: Weapon must be in slot and physically drawn
	const bool bHasWeapon = Inv->HasItemEquippedAtSlot(EEquipmentSlot::MainHand);
	const bool bIsArmed = OwnerCharacter->HasWeaponEquipped();

	return bHasWeapon && bIsArmed;
}

void UCombatComponent::HandleComboInput()
{
	UE_LOG(LogCombatSystem, Log, TEXT("Advancing Combo. Index: %d"), CurrentComboIndex);
	ClearBufferedAttackInput();
	ExecuteNextComboStep();
}

void UCombatComponent::HandleInitialInput()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || AnimInstance->IsAnyMontagePlaying()) return;

	ClearBufferedAttackInput();
	ResetCombo();
	ExecuteNextComboStep();
}

void UCombatComponent::ExecuteNextComboStep()
{
	UWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	
	// Phase 1: Logic Validation
	if (!IsComboStateValid(WeaponData)) return;

	// Phase 2: State Processing
	AdvanceComboState(WeaponData);

	// Phase 3: Execution
	PlayComboAttack(WeaponData, CurrentComboIndex - 1);
}

bool UCombatComponent::IsComboStateValid(const UWeaponDataAsset* WeaponData) const
{
	if (!WeaponData) return false;

	return WeaponData->WeaponData.ComboData.ComboMontages.Num() > 0;
}

void UCombatComponent::AdvanceComboState(const UWeaponDataAsset* WeaponData)
{
	const int32 MaxCombos = WeaponData->WeaponData.ComboData.ComboMontages.Num();

	// Cycle logic: Wrap around if we exceed the defined combo length
	if (CurrentComboIndex >= MaxCombos)
	{
		ResetCombo();
	}

	CurrentComboIndex++;
	bCanAdvanceCombo = false; // Close the window until next ANS_ComboWindow NotifyBegin

	UE_LOG(LogCombatSystem, Log, TEXT("Combo State Advanced. Next Index: %d"), CurrentComboIndex);
}

void UCombatComponent::PlayComboAttack(UWeaponDataAsset* WeaponData, int32 Index)
{
	if (!WeaponData || !OwnerCharacter) return;

	const TArray<TSoftObjectPtr<UAnimMontage>>& Combos = WeaponData->WeaponData.ComboData.ComboMontages;
	
	if (Combos.IsValidIndex(Index))
	{
		const TSoftObjectPtr<UAnimMontage>& ComboMontage = Combos[Index];
		if (ComboMontage.IsPending())
		{
			RequestComboMontageLoad(WeaponData, Index);
		}
		else if (ComboMontage.IsValid())
		{
			ClearPendingComboRequest();
			RecordPlayedAttackMontage(ComboMontage.Get(), OwnerCharacter->PlayAnimMontage(ComboMontage.Get()));
		}
	}
}

void UCombatComponent::RequestComboMontageLoad(UWeaponDataAsset* WeaponData, int32 Index)
{
	if (!WeaponData) return;

	const TArray<TSoftObjectPtr<UAnimMontage>>& Combos = WeaponData->WeaponData.ComboData.ComboMontages;
	if (!Combos.IsValidIndex(Index))
	{
		return;
	}

	PendingComboWeaponData = WeaponData;
	PendingComboMontageIndex = Index;

	FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UCombatComponent::OnComboMontageLoaded, WeaponData, Index);
	UAssetManager::GetStreamableManager().RequestAsyncLoad(Combos[Index].ToSoftObjectPath(), Delegate);
}

void UCombatComponent::OnComboMontageLoaded(UWeaponDataAsset* WeaponData, int32 Index)
{
	if (!OwnerCharacter || PendingComboWeaponData.Get() != WeaponData || PendingComboMontageIndex != Index || GetEquippedWeaponData() != WeaponData)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		if (AnimInstance->IsAnyMontagePlaying())
		{
			return;
		}
	}

	const TArray<TSoftObjectPtr<UAnimMontage>>& Combos = WeaponData->WeaponData.ComboData.ComboMontages;
	if (Combos.IsValidIndex(Index) && Combos[Index].IsValid())
	{
		ClearPendingComboRequest();
		RecordPlayedAttackMontage(Combos[Index].Get(), OwnerCharacter->PlayAnimMontage(Combos[Index].Get()));
	}
}

UWeaponDataAsset* UCombatComponent::GetEquippedWeaponData() const
{
	UInventoryComponent* Inventory = GetInventoryComponent();
	if (Inventory)
	{
		return Cast<UWeaponDataAsset>(Inventory->GetEquippedItem(EEquipmentSlot::MainHand));
	}
	return nullptr;
}

UInventoryComponent* UCombatComponent::GetInventoryComponent() const
{
	if (!CachedInventory.IsValid() && OwnerCharacter)
	{
		CachedInventory = OwnerCharacter->FindComponentByClass<UInventoryComponent>();
	}
	return CachedInventory.Get();
}

UAnimInstance* UCombatComponent::GetAnimInstance() const
{
	if (!CachedAnimInstance.IsValid() && OwnerCharacter)
	{
		CachedAnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
	}
	return CachedAnimInstance.Get();
}

void UCombatComponent::ResetCombo()
{
	CurrentComboIndex = 0;
	bCanAdvanceCombo = false;
	ClearBufferedAttackInput();
	ClearPendingComboRequest();
}

void UCombatComponent::ClearPendingComboRequest()
{
	PendingComboWeaponData.Reset();
	PendingComboMontageIndex = INDEX_NONE;
}

void UCombatComponent::BufferAttackInput()
{
	if (AttackInputBufferDuration <= 0.0f)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		bBufferedAttackInput = true;
		BufferedAttackInputExpiryTime = World->GetTimeSeconds() + AttackInputBufferDuration;
	}
}

void UCombatComponent::ConsumeBufferedAttackInput()
{
	if (bCanAdvanceCombo && HasBufferedAttackInput())
	{
		HandleComboInput();
	}
}

void UCombatComponent::ClearBufferedAttackInput()
{
	bBufferedAttackInput = false;
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

void UCombatComponent::RecordPlayedAttackMontage(UAnimMontage* PlayedMontage, float PlayedDuration)
{
	if (PlayedMontage && PlayedDuration > 0.0f)
	{
		CurrentAttackMontage = PlayedMontage;

		if (UAnimInstance* AnimInstance = GetAnimInstance())
		{
			FOnMontageBlendingOutStarted BlendOutDelegate;
			BlendOutDelegate.BindUObject(this, &UCombatComponent::OnAttackMontageBlendingOut);
			AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, PlayedMontage);
		}
	}
}

void UCombatComponent::OnAttackMontageBlendingOut(UAnimMontage* AttackMontage, bool /*bInterrupted*/)
{
	if (!AttackMontage)
	{
		return;
	}

	ClearAttackInterruptWindowsForMontage(AttackMontage);
	ClearMeleeHitWindowsForMontage(AttackMontage);
	ClearBufferedAttackInput();

	if (CurrentAttackMontage.Get() == AttackMontage)
	{
		CurrentAttackMontage.Reset();
	}

	if (PendingComboWeaponData.IsValid() && PendingComboMontageIndex != INDEX_NONE)
	{
		OnComboMontageLoaded(PendingComboWeaponData.Get(), PendingComboMontageIndex);
	}
}

void UCombatComponent::ClearAttackInterruptWindowsForMontage(const UAnimMontage* AttackMontage)
{
	if (!AttackMontage)
	{
		return;
	}

	for (auto It = ActiveAttackInterruptWindows.CreateIterator(); It; ++It)
	{
		if (It.Value().Montage == AttackMontage)
		{
			It.RemoveCurrent();
		}
	}
}

void UCombatComponent::ClearMeleeHitWindowsForMontage(const UAnimMontage* AttackMontage)
{
	if (!AttackMontage)
	{
		return;
	}

	for (auto It = ActiveMeleeHitWindows.CreateIterator(); It; ++It)
	{
		if (It.Value().Montage == AttackMontage)
		{
			It.RemoveCurrent();
		}
	}

	RefreshWeaponHitCollisionState();
}

void UCombatComponent::RefreshWeaponHitCollisionState()
{
	if (!OwnerCharacter)
	{
		return;
	}

	bool bShouldEnableCollision = false;
	const UAnimMontage* AttackMontage = CurrentAttackMontage.Get();

	if (AttackMontage)
	{
		if (UAnimInstance* AnimInstance = GetAnimInstance(); AnimInstance && AnimInstance->Montage_IsPlaying(AttackMontage))
		{
			for (const TPair<const UAnimNotifyState*, FMeleeHitWindowState>& Entry : ActiveMeleeHitWindows)
			{
				const FMeleeHitWindowState& WindowState = Entry.Value;
				if (WindowState.ActiveCount > 0 && WindowState.Montage == AttackMontage)
				{
					bShouldEnableCollision = true;
					break;
				}
			}
		}
	}

	if (!bShouldEnableCollision)
	{
		HitActorsInActiveMeleeWindow.Reset();
		ResetReliableMeleeTraceState();
	}

	const bool bShouldRunReliableTracing = bShouldEnableCollision && bEnableReliableMeleeTracing;
	SetComponentTickEnabled(bShouldRunReliableTracing);

	if (bShouldRunReliableTracing)
	{
		if (!bHasReliableTraceOrigin)
		{
			PreviousReliableTraceOrigin = ResolveMeleeTraceOrigin();
			bHasReliableTraceOrigin = true;
		}
	}

	OwnerCharacter->SetMeleeHitCollisionEnabled(bShouldEnableCollision);
}

bool UCombatComponent::CanRegisterMeleeHit(AActor* OtherActor) const
{
	if (!OwnerCharacter || !IsValid(OtherActor) || OtherActor == OwnerCharacter || ActiveMeleeHitWindows.IsEmpty())
	{
		return false;
	}

	if (HitActorsInActiveMeleeWindow.Contains(TObjectKey<AActor>(OtherActor)))
	{
		return false;
	}

	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(OtherActor);
	return TargetCharacter && OtherActor->CanBeDamaged();
}

void UCombatComponent::TraceReliableMeleeHits()
{
	if (!OwnerCharacter)
	{
		return;
	}

	UPrimitiveComponent* HitCollisionComp = OwnerCharacter->GetMeleeHitCollisionComponent();
	if (!HitCollisionComp)
	{
		ResetReliableMeleeTraceState();
		return;
	}

	const FVector CurrentTraceOrigin = ResolveMeleeTraceOrigin();
	if (!bHasReliableTraceOrigin)
	{
		PreviousReliableTraceOrigin = CurrentTraceOrigin;
		bHasReliableTraceOrigin = true;
		return;
	}

	if (CurrentTraceOrigin.Equals(PreviousReliableTraceOrigin, 0.1f))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ReliableMeleeHitTrace), false, OwnerCharacter);
	QueryParams.AddIgnoredComponent(HitCollisionComp);

	World->SweepMultiByChannel(
		HitResults,
		PreviousReliableTraceOrigin,
		CurrentTraceOrigin,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(ResolveMeleeTraceRadius()),
		QueryParams);

	for (const FHitResult& HitResult : HitResults)
	{
		if (AActor* HitActor = HitResult.GetActor(); CanRegisterMeleeHit(HitActor))
		{
			ApplyMeleeHitToActor(HitActor);
		}
	}

	PreviousReliableTraceOrigin = CurrentTraceOrigin;
}

void UCombatComponent::ResetReliableMeleeTraceState()
{
	bHasReliableTraceOrigin = false;
	PreviousReliableTraceOrigin = FVector::ZeroVector;
}

float UCombatComponent::ResolveOutgoingMeleeDamage(bool& bOutIsCriticalHit) const
{
	bOutIsCriticalHit = false;

	if (!OwnerCharacter)
	{
		return 0.0f;
	}

	const UAbilitySystemComponent* SourceASC = OwnerCharacter->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		return 0.0f;
	}

	const float BaseDamage = FMath::Max(1.0f, SourceASC->GetNumericAttribute(UCharacterAttributeSet::GetAttackDamageAttribute()));
	bOutIsCriticalHit = RollCriticalHit();
	return bOutIsCriticalHit ? (BaseDamage * CriticalDamageMultiplier) : BaseDamage;
}

bool UCombatComponent::RollCriticalHit() const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	const UAbilitySystemComponent* SourceASC = OwnerCharacter->GetAbilitySystemComponent();
	if (!SourceASC)
	{
		return false;
	}

	const float CriticalStrikeChance = FMath::Clamp(SourceASC->GetNumericAttribute(UCharacterAttributeSet::GetCriticalStrikeChanceAttribute()), 0.0f, 100.0f);
	return CriticalStrikeChance > 0.0f && FMath::FRandRange(0.0f, 100.0f) <= CriticalStrikeChance;
}

FVector UCombatComponent::ResolveMeleeTraceOrigin() const
{
	if (OwnerCharacter)
	{
		if (const UPrimitiveComponent* HitCollisionComp = OwnerCharacter->GetMeleeHitCollisionComponent())
		{
			return HitCollisionComp->Bounds.Origin;
		}
	}

	return FVector::ZeroVector;
}

float UCombatComponent::ResolveMeleeTraceRadius() const
{
	if (OwnerCharacter)
	{
		if (const UPrimitiveComponent* HitCollisionComp = OwnerCharacter->GetMeleeHitCollisionComponent())
		{
			const float RawRadius = HitCollisionComp->Bounds.SphereRadius * ReliableMeleeTraceRadiusScale;
			return FMath::Clamp(RawRadius, ReliableMeleeTraceMinRadius, ReliableMeleeTraceMaxRadius);
		}
	}

	return ReliableMeleeTraceMinRadius;
}

void UCombatComponent::ApplyMeleeHitToActor(AActor* HitActor)
{
	if (!HitActor || !OwnerCharacter)
	{
		return;
	}

	bool bIsCriticalHit = false;
	const float Damage = ResolveOutgoingMeleeDamage(bIsCriticalHit);
	if (Damage <= 0.0f)
	{
		return;
	}

	FWoWCloneCombatDamageEvent DamageEvent(bIsCriticalHit);
	const float AppliedDamage = HitActor->TakeDamage(Damage, DamageEvent, OwnerCharacter->GetController(), OwnerCharacter);
	if (AppliedDamage <= 0.0f)
	{
		return;
	}

	HitActorsInActiveMeleeWindow.Add(TObjectKey<AActor>(HitActor));
	TryPlayHitCameraShake();
	TryApplyHitStop(HitActor);

	UE_LOG(LogCombatSystem, Log, TEXT("Melee hit registered. Target: %s Damage: %.2f Crit: %s"), *GetNameSafe(HitActor), AppliedDamage, bIsCriticalHit ? TEXT("True") : TEXT("False"));
}

void UCombatComponent::TryPlayHitCameraShake() const
{
	const UWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	if (!WeaponData || !OwnerCharacter)
	{
		return;
	}

	const FWeaponData& WeaponConfig = WeaponData->WeaponData;
	if (!WeaponConfig.HitCameraShakeClass || WeaponConfig.HitCameraShakeScale <= 0.0f)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(OwnerCharacter->GetController()))
	{
		PlayerController->ClientStartCameraShake(WeaponConfig.HitCameraShakeClass, WeaponConfig.HitCameraShakeScale);
	}
}

void UCombatComponent::TryApplyHitStop(AActor* HitActor)
{
	const UWeaponDataAsset* WeaponData = GetEquippedWeaponData();
	if (!WeaponData || !OwnerCharacter || !HitActor)
	{
		return;
	}

	const FWeaponData& WeaponConfig = WeaponData->WeaponData;
	if (!WeaponConfig.bEnableHitStop || WeaponConfig.HitStopDuration <= 0.0f || WeaponConfig.HitStopTimeDilation >= 1.0f)
	{
		return;
	}

	RestoreActiveHitStop();

	auto AddHitStopActor = [this, &WeaponConfig](AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		for (const FHitStopActorState& ExistingState : ActiveHitStopActors)
		{
			if (ExistingState.Actor.Get() == Actor)
			{
				return;
			}
		}

		FHitStopActorState& ActorState = ActiveHitStopActors.AddDefaulted_GetRef();
		ActorState.Actor = Actor;
		ActorState.PreviousCustomTimeDilation = Actor->CustomTimeDilation;
		Actor->CustomTimeDilation = WeaponConfig.HitStopTimeDilation;
	};

	AddHitStopActor(OwnerCharacter);
	AddHitStopActor(HitActor);

	if (ActiveHitStopActors.IsEmpty())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		++ActiveHitStopRequestId;

		FTimerDelegate RestoreDelegate;
		RestoreDelegate.BindUObject(this, &UCombatComponent::RestoreHitStop, ActiveHitStopRequestId);
		World->GetTimerManager().SetTimer(HitStopRestoreTimerHandle, RestoreDelegate, WeaponConfig.HitStopDuration, false);
	}
	else
	{
		RestoreActiveHitStop();
	}
}

void UCombatComponent::RestoreHitStop(int32 HitStopRequestId)
{
	if (HitStopRequestId != ActiveHitStopRequestId)
	{
		return;
	}

	RestoreActiveHitStop();
}

void UCombatComponent::RestoreActiveHitStop()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitStopRestoreTimerHandle);
	}

	for (const FHitStopActorState& ActorState : ActiveHitStopActors)
	{
		if (AActor* Actor = ActorState.Actor.Get())
		{
			Actor->CustomTimeDilation = ActorState.PreviousCustomTimeDilation;
		}
	}

	ActiveHitStopActors.Reset();
}

const UAnimMontage* UCombatComponent::ResolveInterruptibleAttackMontage(UAnimInstance* AnimInstance) const
{
	const UAnimMontage* AttackMontage = CurrentAttackMontage.Get();
	if (!AttackMontage || !AnimInstance->Montage_IsPlaying(AttackMontage))
	{
		return nullptr;
	}

	for (const TPair<const UAnimNotifyState*, FAttackInterruptWindowState>& Entry : ActiveAttackInterruptWindows)
	{
		const FAttackInterruptWindowState& WindowState = Entry.Value;
		if (WindowState.ActiveCount > 0 && WindowState.Montage == AttackMontage)
		{
			return AttackMontage;
		}
	}

	return nullptr;
}

float UCombatComponent::ResolveInterruptBlendOutTime(const UAnimMontage* AttackMontage) const
{
	float BlendOutTime = 0.25f;
	bool bFoundMatchingWindow = false;

	for (const TPair<const UAnimNotifyState*, FAttackInterruptWindowState>& Entry : ActiveAttackInterruptWindows)
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

