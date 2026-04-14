#include "Components/DamageReceiverComponent.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "CombatTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"

UDamageReceiverComponent::UDamageReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDamageReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	CacheDependencies();
}

float UDamageReceiverComponent::ReceiveDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	CacheDependencies();

	if (!CanProcessDamage())
	{
		return 0.0f;
	}

	const float RequestedDamage = SanitizeRequestedDamage(DamageAmount);
	if (RequestedDamage <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = ResolveAppliedDamage(RequestedDamage, DamageEvent, EventInstigator, DamageCauser);
	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	ApplyHealthDelta(AppliedDamage, EventInstigator, DamageCauser);
	if (bRestoreHealthToMaxOnDefeat)
	{
		RestoreHealthAfterDefeat();
	}
	return AppliedDamage;
}

void UDamageReceiverComponent::CacheDependencies()
{
	if (!OwnerActor)
	{
		OwnerActor = GetOwner();
	}

	if (!AbilitySystemComponent)
	{
		if (const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(OwnerActor))
		{
			AbilitySystemComponent = AbilitySystemOwner->GetAbilitySystemComponent();
		}
	}
}

bool UDamageReceiverComponent::CanProcessDamage() const
{
	return OwnerActor && OwnerActor->CanBeDamaged() && AbilitySystemComponent;
}

float UDamageReceiverComponent::SanitizeRequestedDamage(float DamageAmount) const
{
	return FMath::Max(0.0f, DamageAmount);
}

float UDamageReceiverComponent::ResolveAppliedDamage(float RequestedDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const
{
	float MitigatedDamage = ApplyMitigation(RequestedDamage, DamageEvent);
	MitigatedDamage = ApplySourceContextMultiplier(MitigatedDamage, EventInstigator, DamageCauser);
	if (MitigatedDamage <= 0.0f)
	{
		return 0.0f;
	}

	const float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
	if (CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Min(MitigatedDamage, CurrentHealth);
}

float UDamageReceiverComponent::ApplyMitigation(float RequestedDamage, const FDamageEvent& DamageEvent) const
{
	const FWoWCloneCombatDamageEvent* CombatDamageEvent = static_cast<const FWoWCloneCombatDamageEvent*>(nullptr);
	if (DamageEvent.IsOfType(FWoWCloneCombatDamageEvent::ClassID))
	{
		CombatDamageEvent = static_cast<const FWoWCloneCombatDamageEvent*>(&DamageEvent);
	}

	float MitigatedDamage = RequestedDamage;
	if (CombatDamageEvent)
	{
		MitigatedDamage = ApplyDamageTypeMitigation(MitigatedDamage, CombatDamageEvent->DamageType);
		if (CombatDamageEvent->bIsAreaDamage)
		{
			MitigatedDamage *= AreaDamageTakenMultiplier;
		}
	}

	return FMath::Max(0.0f, MitigatedDamage);
}

float UDamageReceiverComponent::ApplySourceContextMultiplier(float DamageAmount, AController* EventInstigator, AActor* DamageCauser) const
{
	const bool bSourceIsPlayer = IsPlayerControlledSource(EventInstigator, DamageCauser);
	const bool bTargetIsPlayer = IsPlayerControlledTarget();

	if (bSourceIsPlayer && bTargetIsPlayer)
	{
		return DamageAmount * PlayerVsPlayerDamageMultiplier;
	}

	if (bSourceIsPlayer)
	{
		return DamageAmount * PlayerVsEnvironmentDamageMultiplier;
	}

	if (bTargetIsPlayer)
	{
		return DamageAmount * EnvironmentVsPlayerDamageMultiplier;
	}

	return DamageAmount * EnvironmentVsEnvironmentDamageMultiplier;
}

float UDamageReceiverComponent::ApplyDamageTypeMitigation(float DamageAmount, EWoWCloneDamageType DamageType) const
{
	switch (DamageType)
	{
	case EWoWCloneDamageType::Physical:
		{
			const float Armor = FMath::Max(0.0f, AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetArmorAttribute()));
			const float MitigationScale = PhysicalMitigationScale > 0.0f ? PhysicalMitigationScale : 100.0f;
			return DamageAmount * (MitigationScale / (MitigationScale + Armor));
		}
	case EWoWCloneDamageType::Magical:
		{
			const float MagicResistance = FMath::Max(0.0f, AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetMagicResistanceAttribute()));
			const float MitigationScale = MagicalMitigationScale > 0.0f ? MagicalMitigationScale : 100.0f;
			return DamageAmount * (MitigationScale / (MitigationScale + MagicResistance));
		}
	case EWoWCloneDamageType::TrueDamage:
	default:
		return DamageAmount;
	}
}

bool UDamageReceiverComponent::IsPlayerControlledSource(AController* EventInstigator, AActor* DamageCauser) const
{
	if (EventInstigator)
	{
		return EventInstigator->IsPlayerController();
	}

	if (const APawn* SourcePawn = Cast<APawn>(DamageCauser))
	{
		return SourcePawn->IsPlayerControlled();
	}

	return false;
}

bool UDamageReceiverComponent::IsPlayerControlledTarget() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		return OwnerPawn->IsPlayerControlled();
	}

	return false;
}

void UDamageReceiverComponent::ApplyHealthDelta(float AppliedDamage, AController* EventInstigator, AActor* DamageCauser) const
{
	if (!AbilitySystemComponent || AppliedDamage <= 0.0f)
	{
		return;
	}

	UGameplayEffect* DamageEffect = GetOrCreateReusableDamageEffect();
	if (!DamageEffect)
	{
		return;
	}

	DamageEffect->Modifiers[0].ModifierMagnitude = FScalableFloat(-AppliedDamage);

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	AActor* InstigatorActor = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	EffectContext.AddInstigator(InstigatorActor, DamageCauser);
	UObject* SourceObject = DamageCauser ? static_cast<UObject*>(DamageCauser) : static_cast<UObject*>(OwnerActor.Get());
	EffectContext.AddSourceObject(SourceObject);

	FGameplayEffectSpec DamageSpec(DamageEffect, EffectContext, 1.0f);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(DamageSpec);
}

void UDamageReceiverComponent::RestoreHealthAfterDefeat() const
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	const float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
	if (CurrentHealth > 0.0f)
	{
		return;
	}

	const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth > 0.0f)
	{
		AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), MaxHealth);
	}
}

UGameplayEffect* UDamageReceiverComponent::GetOrCreateReusableDamageEffect() const
{
	if (!ReusableDamageEffect)
	{
		ReusableDamageEffect = NewObject<UGameplayEffect>(const_cast<UDamageReceiverComponent*>(this), NAME_None, RF_Transient);
		if (!ReusableDamageEffect)
		{
			return nullptr;
		}

		ReusableDamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo& HealthModifier = ReusableDamageEffect->Modifiers.AddDefaulted_GetRef();
		HealthModifier.Attribute = UCharacterAttributeSet::GetHealthAttribute();
		HealthModifier.ModifierOp = EGameplayModOp::Additive;
		HealthModifier.ModifierMagnitude = FScalableFloat(0.0f);
	}

	return ReusableDamageEffect;
}
