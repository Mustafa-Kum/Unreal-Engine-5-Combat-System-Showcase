#include "Characters/HeroLocomotionComponent.h"
#include "Characters/CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "GameplayEffectTypes.h"
#include "WoWCloneGameplayTags.h"

UHeroLocomotionComponent::UHeroLocomotionComponent()
{
	// Locomotion owns its own state updates so the character does not need a proxy tick.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bWasMoving = false;
	bIsWalking = false;
	bWasWalking = false;
}

void UHeroLocomotionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindMovementSpeedAttribute();
	Super::EndPlay(EndPlayReason);
}

void UHeroLocomotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateLocomotionState(DeltaTime);
}

void UHeroLocomotionComponent::InitializeLocomotion(ACharacterBase* InOwnerCharacter)
{
	if (!InOwnerCharacter) return;

	UnbindMovementSpeedAttribute();
	OwnerCharacter = InOwnerCharacter;
	ASC = OwnerCharacter->GetAbilitySystemComponent();

	SetupMovementComponent();
	BindMovementSpeedAttribute();
	SetComponentTickEnabled(true);
	UpdateLocomotionTags();
}

void UHeroLocomotionComponent::SetupMovementComponent()
{
	if (!OwnerCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->RotationRate = FRotator(0.0f, BaseRotationRateYaw, 0.0f);
	}

	RefreshRunSpeedFromAttributes();
	ApplyCurrentMaxWalkSpeed();
}

void UHeroLocomotionComponent::BindMovementSpeedAttribute()
{
	if (!ASC || MovementSpeedAttributeChangedHandle.IsValid())
	{
		return;
	}

	MovementSpeedAttributeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMovementSpeedAttribute())
		.AddUObject(this, &UHeroLocomotionComponent::HandleMovementSpeedChanged);
}

void UHeroLocomotionComponent::UnbindMovementSpeedAttribute()
{
	if (ASC && MovementSpeedAttributeChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMovementSpeedAttribute())
			.Remove(MovementSpeedAttributeChangedHandle);
	}

	MovementSpeedAttributeChangedHandle.Reset();
}

void UHeroLocomotionComponent::HandleMovementSpeedChanged(const FOnAttributeChangeData& ChangeData)
{
	const float NewRunSpeed = ChangeData.NewValue > 0.0f ? ChangeData.NewValue : DefaultMaxWalkSpeed;
	RunSpeed = NewRunSpeed;
	ApplyCurrentMaxWalkSpeed();
}

void UHeroLocomotionComponent::RefreshRunSpeedFromAttributes()
{
	if (!ASC)
	{
		return;
	}

	const float AttributeRunSpeed = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMovementSpeedAttribute());
	if (AttributeRunSpeed > 0.0f)
	{
		RunSpeed = AttributeRunSpeed;
	}
}

void UHeroLocomotionComponent::ApplyCurrentMaxWalkSpeed()
{
	if (!OwnerCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = bIsWalking ? WalkSpeed : RunSpeed;
	}
}

void UHeroLocomotionComponent::UpdateLocomotionState(float DeltaTime)
{
	UpdateLocomotionTags();
}

void UHeroLocomotionComponent::ToggleWalkSpeed()
{
	if (!OwnerCharacter) return;
	
	SetWalkingState(!bIsWalking);
}

void UHeroLocomotionComponent::ApplyCombatStateOverrides()
{
	if (!OwnerCharacter) return;

	if (bIsWalking)
	{
		SetWalkingState(false);
	}
}

void UHeroLocomotionComponent::RevertCombatStateOverrides()
{
	// AAA Symmetry: Mirror of ApplyCombatStateOverrides — restores movement to default run state on combat exit
	if (!OwnerCharacter) return;

	SetWalkingState(false);
}

void UHeroLocomotionComponent::SetWalkingState(bool bEnabled)
{
	bIsWalking = bEnabled;
	ApplyCurrentMaxWalkSpeed();
}

FVector2D UHeroLocomotionComponent::GetNormalizedAndScaledMovementInput(FVector2D RawInput) const
{
	FVector2D InputVector = NormalizeInputVector(RawInput);
	return ApplyBackwardMovementPenalty(InputVector);
}

FVector2D UHeroLocomotionComponent::NormalizeInputVector(FVector2D RawInput) const
{
	return RawInput.SizeSquared() > 1.0f ? RawInput.GetSafeNormal() : RawInput;
}

FVector2D UHeroLocomotionComponent::ApplyBackwardMovementPenalty(FVector2D NormalizedInput) const
{
	// AAA: Encapsulate comparison against static epsilon or zero
	if (NormalizedInput.X < 0.0f)
	{
		return NormalizedInput * CalculateBackwardMovementScale(NormalizedInput.X);
	}

	return NormalizedInput;
}

float UHeroLocomotionComponent::CalculateBackwardMovementScale(float ForwardInputValue) const
{
	const float BackwardPower = FMath::Abs(ForwardInputValue); 
	return FMath::Lerp(1.0f, BackwardMovementSpeedMultiplier, BackwardPower);
}

// ==============================================================================
// Tags & State Cache Handlers
// ==============================================================================

void UHeroLocomotionComponent::UpdateLocomotionTags()
{
	if (!ASC || !OwnerCharacter) return;

	const bool bIsMovingState = IsMoving();

	if (HasLocomotionStateChanged(bIsMovingState))
	{
		ClearLocomotionTags();
		ApplyCurrentLocomotionTag(bIsMovingState);
		UpdateLocomotionStateCache(bIsMovingState);
	}
}

bool UHeroLocomotionComponent::IsMoving() const
{
	if (!OwnerCharacter || !OwnerCharacter->GetCharacterMovement()) return false;
	
	const float SpeedSq = OwnerCharacter->GetCharacterMovement()->Velocity.SizeSquared2D();
	const float ThresholdSq = FMath::Square(LocomotionVelocityThreshold);
	
	return SpeedSq > ThresholdSq;
}

bool UHeroLocomotionComponent::HasLocomotionStateChanged(bool bIsMovingState) const
{
	return bIsMovingState != bWasMoving || (bIsMovingState && bIsWalking != bWasWalking);
}

void UHeroLocomotionComponent::ClearLocomotionTags()
{
	if (!ASC) return;
	ASC->RemoveLooseGameplayTag(WoWCloneTags::State_Idle);
	ASC->RemoveLooseGameplayTag(WoWCloneTags::State_Walking);
	ASC->RemoveLooseGameplayTag(WoWCloneTags::State_Running);
}

void UHeroLocomotionComponent::ApplyCurrentLocomotionTag(bool bIsMovingState)
{
	if (!ASC) return;

	// AAA Clean Code: Dictionary/Ternary mapping instead of deep if/else blocks (DRY Principle)
	const FGameplayTag& TagToApply = !bIsMovingState ? WoWCloneTags::State_Idle 
								   : (bIsWalking ? WoWCloneTags::State_Walking : WoWCloneTags::State_Running);

	ASC->AddLooseGameplayTag(TagToApply);
}

void UHeroLocomotionComponent::UpdateLocomotionStateCache(bool bIsMovingState)
{
	bWasMoving = bIsMovingState;
	bWasWalking = bIsWalking;
}
