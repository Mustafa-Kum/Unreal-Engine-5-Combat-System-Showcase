#include "Components/DamageReceiverComponent.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
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

float UDamageReceiverComponent::ReceiveDamage(float DamageAmount, const FDamageEvent& /*DamageEvent*/, AController* /*EventInstigator*/, AActor* /*DamageCauser*/)
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

	const float AppliedDamage = ResolveAppliedDamage(RequestedDamage);
	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	ApplyHealthDelta(AppliedDamage);
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

float UDamageReceiverComponent::ResolveAppliedDamage(float RequestedDamage) const
{
	const float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
	if (CurrentHealth <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Min(RequestedDamage, CurrentHealth);
}

void UDamageReceiverComponent::ApplyHealthDelta(float AppliedDamage) const
{
	AbilitySystemComponent->ApplyModToAttributeUnsafe(UCharacterAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -AppliedDamage);
}
