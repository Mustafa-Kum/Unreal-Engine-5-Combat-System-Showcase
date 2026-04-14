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
	void InitializeStartingStats(const struct FCharacterStartingStats& Stats);
	[[nodiscard]] static FString GetFriendlyAttributeLabel(const FGameplayAttribute& Attribute);
	[[nodiscard]] static bool IsPercentageDisplayAttribute(const FGameplayAttribute& Attribute);

	// UAttributeSet Overrides
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostAttributeBaseChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) const override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

private:
	// SOLID Helpers: Each scaling rule extracted from PreAttributeChange (Orchestrator Pattern)
	void SetAttributeBaseValue(const FGameplayAttribute& Attribute, float NewValue) const;
	[[nodiscard]] float GetAttributeBaseValue(const FGameplayAttribute& Attribute) const;
	void ApplyAdditiveAttributeBaseDelta(const FGameplayAttribute& Attribute, float Delta) const;
	bool HandleClampedAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) const;
	void ClampPostEffectAttribute(const FGameplayAttribute& Attribute);
	void HandleDerivedAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue);
	void RecalculateFromStrength(float OldStrength, float NewStrength);
	void RecalculateFromAgility(float OldAgility, float NewAgility);
	void RecalculateFromIntellect(float OldIntellect, float NewIntellect);
	void RecalculateFromStamina(float OldStamina, float NewStamina);
	void ClampHealth(float& NewValue) const;
	void ClampMana(float& NewValue) const;
	void ClampArmor(float& NewValue) const;
	void ClampMagicResistance(float& NewValue) const;
	void ClampCastSpeed(float& NewValue) const;

	// DRY: Proportional current value adjustment when Max changes (Stamina/Intellect scaling)
	void AdjustAttributeProportionally(const FGameplayAttribute& CurrentAttr, float NewMax, float OldMax);
	bool bIsInitializingStartingStats = false;
	float BaseHealthFromScaling = 100.0f;
	float AttackDamagePerStrength = 2.0f;
	float ArmorPerAgility = 1.0f;
	float CastSpeedPercentPerAgility = 1.0f;
	float MaxCastSpeedPercent = 90.0f;
	float SpellDamagePerIntellect = 2.5f;
	float MaxManaPerIntellect = 15.0f;
	float MaxHealthPerStamina = 10.0f;
	float BaseHealthRegen = 1.0f;
	float HealthRegenPerStrength = 0.15f;
	float BaseManaRegen = 2.0f;
	float ManaRegenPerIntellect = 0.25f;

public:

	/*
	 * PRIMARY STATS
	 */

	// Increases Attack Damage
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Primary")
	FGameplayAttributeData Strength;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Strength)

	// Increases Armor and Cast Speed
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

	// Health regeneration applied per second by the owning character's resource loop.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, HealthRegen)

	// Mana regeneration applied per second by the owning character's resource loop.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData ManaRegen;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, ManaRegen)

	// Maximum Mana (Derived from Intellect)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vital")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxMana)

	// Physical Damage (Derived from Strength)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, AttackDamage)

	// Percent cooldown reduction applied to ability cooldowns
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData CastSpeed;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CastSpeed)

	// Magical resistance / mitigation source for spell damage.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData MagicResistance;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MagicResistance)

	// Magical Damage (Derived from Intellect)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData SpellDamage;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, SpellDamage)

	// Physical Resistance / Mitigation
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Derived")
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Armor)
};
