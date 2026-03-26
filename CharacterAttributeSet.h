// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.generated.h"

// Macro that defines a set of helper functions for accessing and initializing attributes.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class WOWCLONE_API UCharacterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UCharacterAttributeSet();

	// UAttributeSet Overrides
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

private:
	// SOLID Helpers: Each scaling rule extracted from PreAttributeChange (Orchestrator Pattern)
	bool HandleDerivedAttributeChange(const FGameplayAttribute& Attribute, float& NewValue);
	bool HandleClampedVitalChange(const FGameplayAttribute& Attribute, float& NewValue) const;
	void ClampPostEffectAttribute(const FGameplayAttribute& Attribute);
	void RecalculateFromStrength(float NewStrength);
	void RecalculateFromAgility(float NewAgility);
	void RecalculateFromWeaponInterval(float NewInterval);
	void RecalculateFromIntellect(float NewIntellect);
	void RecalculateFromStamina(float NewStamina);
	void ClampHealth(float& NewValue) const;
	void ClampMana(float& NewValue) const;

	// DRY: Proportional current value adjustment when Max changes (Stamina/Intellect scaling)
	void AdjustAttributeProportionally(const FGameplayAttribute& CurrentAttr, float NewMax, float OldMax);

	// DRY: Pure formula for Agility -> Haste conversion
	[[nodiscard]] float CalculateHastedInterval(float BaseInterval, float AgilityValue) const;

public:

	/*
	 * PRIMARY STATS
	 */

	// Increases Attack Damage
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Primary")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Strength)

	// Increases Attack Speed
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Primary")
	FGameplayAttributeData Agility;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Agility)

	// Increases Spell Damage
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Primary")
	FGameplayAttributeData Intellect;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Intellect)

	// Increases Maximum Health
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Primary")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Stamina)

	/*
	 * SECONDARY STATS
	 */

	// Chance to deal higher damage or heal
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Secondary")
	FGameplayAttributeData CriticalStrikeChance;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CriticalStrikeChance)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Secondary")
	FGameplayAttributeData MovementSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MovementSpeed)

	/*
	 * DERIVED / BASE STATS
	 */

	// Current Health
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Health)

	// Maximum Health (Derived from Stamina)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxHealth)

	// Current Mana
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Mana)

	// Maximum Mana (Derived from Intellect)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxMana)

	// Physical Damage (Derived from Strength)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, AttackDamage)

	// Final Cast interval in seconds (Derived: WeaponBaseInterval / (1 + Haste))
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData CastSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CastSpeed)

	// The base interval of the current weapon (e.g., 3.64)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData WeaponBaseInterval;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, WeaponBaseInterval)

	// Magical Damage (Derived from Intellect)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData SpellDamage;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, SpellDamage)

	// Physical Resistance / Mitigation
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Armor)
};
