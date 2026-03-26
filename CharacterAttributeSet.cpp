// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "GameplayEffectExtension.h"

UCharacterAttributeSet::UCharacterAttributeSet()
{
	// AAA Rule: Stats should NEVER be hardcoded in the constructor (Violates SOLID OCP).
	// They must be driven by data (e.g. UCharacterDataAsset or Data Tables) passing through
	// a Gameplay Effect or an Initialization function during Character Setup.
}

void UCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (HandleDerivedAttributeChange(Attribute, NewValue))
	{
		return;
	}

	HandleClampedVitalChange(Attribute, NewValue);
}

// ==============================================================================
// SOLID Helpers — Each stat scaling rule is isolated and self-documenting
// ==============================================================================

void UCharacterAttributeSet::RecalculateFromStrength(float NewStrength)
{
	// 1 Strength = 2.0 Physical Attack Damage
	const float Diff = NewStrength - GetStrength();
	SetAttackDamage(GetAttackDamage() + (Diff * 2.0f));
}

void UCharacterAttributeSet::RecalculateFromAgility(float NewAgility)
{
	// 1 Agility = 0.01 Haste (1% faster attacks)
	// Model: FinalInterval = WeaponBaseInterval / (1.0 + (Agility * 0.01))
	SetCastSpeed(CalculateHastedInterval(GetWeaponBaseInterval(), NewAgility));
}

void UCharacterAttributeSet::RecalculateFromWeaponInterval(float NewInterval)
{
	// When equipping a new weapon, recalculate the final interval based on current Agility (Haste)
	SetCastSpeed(CalculateHastedInterval(NewInterval, GetAgility()));
}

void UCharacterAttributeSet::RecalculateFromIntellect(float NewIntellect)
{
	// 1 Intellect = 2.5 Spell/Magic Damage
	const float Diff = NewIntellect - GetIntellect();
	SetSpellDamage(GetSpellDamage() + (Diff * 2.5f));

	// 1 Intellect = 15 Max Mana (WoW-style caster scaling)
	const float OldMaxMana = GetMaxMana();
	const float NewMaxMana = OldMaxMana + (Diff * 15.0f);
	SetMaxMana(NewMaxMana);

	// Proportional Mana Adjustment (AAA Standard: Maintains current % during buff/debuff)
	AdjustAttributeProportionally(GetManaAttribute(), NewMaxMana, OldMaxMana);
}

void UCharacterAttributeSet::RecalculateFromStamina(float NewStamina)
{
	// 1 Stamina = 10 Max Health
	const float Diff = NewStamina - GetStamina();
	const float OldMaxHealth = GetMaxHealth();
	const float NewMaxHealth = OldMaxHealth + (Diff * 10.0f);
	SetMaxHealth(NewMaxHealth);

	// Proportional Health Adjustment
	AdjustAttributeProportionally(GetHealthAttribute(), NewMaxHealth, OldMaxHealth);
}

void UCharacterAttributeSet::ClampHealth(float& NewValue) const
{
	NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
}

void UCharacterAttributeSet::ClampMana(float& NewValue) const
{
	NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
}

void UCharacterAttributeSet::AdjustAttributeProportionally(const FGameplayAttribute& CurrentAttr, float NewMax, float OldMax)
{
	// Processing
	if (OldMax <= 0.0f || NewMax == OldMax) return;

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float CurrentValue = CurrentAttr.GetNumericValue(this);
	const float NewCurrentValue = CurrentValue * (NewMax / OldMax);

	// Execution
	// Internal implementation helper: SetNumericAttributeBase bypasses some validation but is efficient for relative adjustments
	ASC->SetNumericAttributeBase(CurrentAttr, NewCurrentValue);
}

float UCharacterAttributeSet::CalculateHastedInterval(float BaseInterval, float AgilityValue) const
{
	// DRY: Single formula shared between Agility-change and WeaponInterval-change paths
	const float Haste = AgilityValue * 0.01f;
	return BaseInterval / FMath::Max(1.0f + Haste, 0.1f);
}

void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	ClampPostEffectAttribute(Data.EvaluatedData.Attribute);
}

bool UCharacterAttributeSet::HandleDerivedAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetStrengthAttribute())
	{
		RecalculateFromStrength(NewValue);
		return true;
	}

	if (Attribute == GetAgilityAttribute())
	{
		RecalculateFromAgility(NewValue);
		return true;
	}

	if (Attribute == GetWeaponBaseIntervalAttribute())
	{
		RecalculateFromWeaponInterval(NewValue);
		return true;
	}

	if (Attribute == GetIntellectAttribute())
	{
		RecalculateFromIntellect(NewValue);
		return true;
	}

	if (Attribute == GetStaminaAttribute())
	{
		RecalculateFromStamina(NewValue);
		return true;
	}

	return false;
}

bool UCharacterAttributeSet::HandleClampedVitalChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		ClampHealth(NewValue);
		return true;
	}

	if (Attribute == GetManaAttribute())
	{
		ClampMana(NewValue);
		return true;
	}

	return false;
}

void UCharacterAttributeSet::ClampPostEffectAttribute(const FGameplayAttribute& Attribute)
{
	if (Attribute == GetHealthAttribute())
	{
		float Value = GetHealth();
		ClampHealth(Value);
		SetHealth(Value);
		return;
	}

	if (Attribute == GetManaAttribute())
	{
		float Value = GetMana();
		ClampMana(Value);
		SetMana(Value);
	}
}
