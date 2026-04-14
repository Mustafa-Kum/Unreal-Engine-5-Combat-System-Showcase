// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "DataAssets/CharacterDataAsset.h"
#include "GameplayEffectExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterAttributeSet, Log, All);

UCharacterAttributeSet::UCharacterAttributeSet()
{
	// AAA Rule: Stats should NEVER be hardcoded in the constructor (Violates SOLID OCP).
	// They must be driven by data (e.g. UCharacterDataAsset or Data Tables) passing through
	// a Gameplay Effect or an Initialization function during Character Setup.
}

FString UCharacterAttributeSet::GetFriendlyAttributeLabel(const FGameplayAttribute& Attribute)
{
	if (Attribute == GetStrengthAttribute()) return TEXT("Strength");
	if (Attribute == GetAgilityAttribute()) return TEXT("Agility");
	if (Attribute == GetIntellectAttribute()) return TEXT("Intellect");
	if (Attribute == GetStaminaAttribute()) return TEXT("Stamina");
	if (Attribute == GetCriticalStrikeChanceAttribute()) return TEXT("Critical Strike Chance");
	if (Attribute == GetMovementSpeedAttribute()) return TEXT("Movement Speed");
	if (Attribute == GetAttackDamageAttribute()) return TEXT("Attack Damage");
	if (Attribute == GetSpellDamageAttribute()) return TEXT("Spell Damage");
	if (Attribute == GetCastSpeedAttribute()) return TEXT("Cast Speed");
	if (Attribute == GetArmorAttribute()) return TEXT("Armor");
	if (Attribute == GetMagicResistanceAttribute()) return TEXT("Magic Resistance");
	if (Attribute == GetManaAttribute()) return TEXT("Mana");
	if (Attribute == GetManaRegenAttribute()) return TEXT("Mana Regen");
	if (Attribute == GetHealthAttribute()) return TEXT("Health");
	if (Attribute == GetHealthRegenAttribute()) return TEXT("Health Regen");
	return Attribute.GetName();
}

bool UCharacterAttributeSet::IsPercentageDisplayAttribute(const FGameplayAttribute& Attribute)
{
	return Attribute == GetCriticalStrikeChanceAttribute()
		|| Attribute == GetCastSpeedAttribute();
}

void UCharacterAttributeSet::InitializeStartingStats(const FCharacterStartingStats& Stats)
{
	bIsInitializingStartingStats = true;

	BaseHealthFromScaling = Stats.DerivedScaling.BaseHealth;
	AttackDamagePerStrength = Stats.DerivedScaling.AttackDamagePerStrength;
	ArmorPerAgility = Stats.DerivedScaling.ArmorPerAgility;
	CastSpeedPercentPerAgility = Stats.DerivedScaling.CastSpeedPercentPerAgility;
	MaxCastSpeedPercent = Stats.DerivedScaling.MaxCastSpeedPercent;
	SpellDamagePerIntellect = Stats.DerivedScaling.SpellDamagePerIntellect;
	MaxManaPerIntellect = Stats.DerivedScaling.MaxManaPerIntellect;
	MaxHealthPerStamina = Stats.DerivedScaling.MaxHealthPerStamina;
	BaseHealthRegen = Stats.DerivedScaling.BaseHealthRegen;
	HealthRegenPerStrength = Stats.DerivedScaling.HealthRegenPerStrength;
	BaseManaRegen = Stats.DerivedScaling.BaseManaRegen;
	ManaRegenPerIntellect = Stats.DerivedScaling.ManaRegenPerIntellect;

	const float InitialArmor = Stats.BaseArmor + (Stats.InitialAgility * ArmorPerAgility);
	const float InitialCastSpeed = Stats.InitialAgility * CastSpeedPercentPerAgility;
	const float InitialAttackDamage = Stats.BasePhysicalDamage + (Stats.InitialStrength * AttackDamagePerStrength);
	const float InitialSpellDamage = Stats.BaseMagicDamage + (Stats.InitialIntellect * SpellDamagePerIntellect);
	const float FinalMaxHealth = BaseHealthFromScaling + (Stats.InitialStamina * MaxHealthPerStamina);
	const float FinalMaxMana = Stats.BaseMaxMana + (Stats.InitialIntellect * MaxManaPerIntellect);
	const float InitialHealthRegen = BaseHealthRegen + (Stats.InitialStrength * HealthRegenPerStrength);
	const float InitialManaRegen = BaseManaRegen + (Stats.InitialIntellect * ManaRegenPerIntellect);

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(GetStrengthAttribute(), Stats.InitialStrength);
		ASC->SetNumericAttributeBase(GetAgilityAttribute(), Stats.InitialAgility);
		ASC->SetNumericAttributeBase(GetIntellectAttribute(), Stats.InitialIntellect);
		ASC->SetNumericAttributeBase(GetStaminaAttribute(), Stats.InitialStamina);
		ASC->SetNumericAttributeBase(GetCriticalStrikeChanceAttribute(), Stats.BaseCriticalStrikeChance);
		ASC->SetNumericAttributeBase(GetMovementSpeedAttribute(), Stats.BaseMovementSpeed);
		ASC->SetNumericAttributeBase(GetArmorAttribute(), InitialArmor);
		ASC->SetNumericAttributeBase(GetMagicResistanceAttribute(), Stats.BaseMagicResistance);
		ASC->SetNumericAttributeBase(GetCastSpeedAttribute(), InitialCastSpeed);
		ASC->SetNumericAttributeBase(GetAttackDamageAttribute(), InitialAttackDamage);
		ASC->SetNumericAttributeBase(GetSpellDamageAttribute(), InitialSpellDamage);
		ASC->SetNumericAttributeBase(GetMaxHealthAttribute(), FinalMaxHealth);
		ASC->SetNumericAttributeBase(GetHealthAttribute(), FinalMaxHealth);
		ASC->SetNumericAttributeBase(GetMaxManaAttribute(), FinalMaxMana);
		ASC->SetNumericAttributeBase(GetManaAttribute(), FinalMaxMana);
		ASC->SetNumericAttributeBase(GetHealthRegenAttribute(), InitialHealthRegen);
		ASC->SetNumericAttributeBase(GetManaRegenAttribute(), InitialManaRegen);
		UE_LOG(
			LogCharacterAttributeSet,
			Log,
			TEXT("InitializeStartingStats -> Strength=%.2f Agility=%.2f Intellect=%.2f Stamina=%.2f AttackDamage=%.2f SpellDamage=%.2f Armor=%.2f CastSpeed=%.2f HealthRegen=%.2f ManaRegen=%.2f"),
			ASC->GetNumericAttribute(GetStrengthAttribute()),
			ASC->GetNumericAttribute(GetAgilityAttribute()),
			ASC->GetNumericAttribute(GetIntellectAttribute()),
			ASC->GetNumericAttribute(GetStaminaAttribute()),
			ASC->GetNumericAttribute(GetAttackDamageAttribute()),
			ASC->GetNumericAttribute(GetSpellDamageAttribute()),
			ASC->GetNumericAttribute(GetArmorAttribute()),
			ASC->GetNumericAttribute(GetCastSpeedAttribute()),
			ASC->GetNumericAttribute(GetHealthRegenAttribute()),
			ASC->GetNumericAttribute(GetManaRegenAttribute()));
		bIsInitializingStartingStats = false;
		return;
	}

	InitStrength(Stats.InitialStrength);
	InitAgility(Stats.InitialAgility);
	InitIntellect(Stats.InitialIntellect);
	InitStamina(Stats.InitialStamina);

	InitCriticalStrikeChance(Stats.BaseCriticalStrikeChance);
	InitMovementSpeed(Stats.BaseMovementSpeed);
	InitArmor(InitialArmor);
	InitMagicResistance(Stats.BaseMagicResistance);
	InitCastSpeed(InitialCastSpeed);
	InitAttackDamage(InitialAttackDamage);
	InitSpellDamage(InitialSpellDamage);
	InitMaxHealth(FinalMaxHealth);
	InitHealth(FinalMaxHealth);
	InitMaxMana(FinalMaxMana);
	InitMana(FinalMaxMana);
	InitHealthRegen(InitialHealthRegen);
	InitManaRegen(InitialManaRegen);

	bIsInitializingStartingStats = false;
}

void UCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	HandleClampedAttributeChange(Attribute, NewValue);
}

void UCharacterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	HandleClampedAttributeChange(Attribute, NewValue);
}

void UCharacterAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (bIsInitializingStartingStats)
	{
		return;
	}

	if (Attribute == GetStrengthAttribute()
		|| Attribute == GetAgilityAttribute()
		|| Attribute == GetIntellectAttribute()
		|| Attribute == GetStaminaAttribute())
	{
		UE_LOG(
			LogCharacterAttributeSet,
			Log,
			TEXT("PostAttributeChange -> %s Old=%.2f New=%.2f"),
			*Attribute.GetName(),
			OldValue,
			NewValue);

		HandleDerivedAttributeChange(Attribute, OldValue, NewValue);
	}
}

void UCharacterAttributeSet::PostAttributeBaseChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) const
{
	Super::PostAttributeBaseChange(Attribute, OldValue, NewValue);

	if (bIsInitializingStartingStats)
	{
		return;
	}

	if (Attribute == GetStrengthAttribute()
		|| Attribute == GetAttackDamageAttribute()
		|| Attribute == GetSpellDamageAttribute()
		|| Attribute == GetCastSpeedAttribute())
	{
		UE_LOG(
			LogCharacterAttributeSet,
			Log,
			TEXT("PostAttributeBaseChange -> %s Old=%.2f New=%.2f CurrentAttackDamage=%.2f CurrentSpellDamage=%.2f CurrentCastSpeed=%.2f"),
			*Attribute.GetName(),
			OldValue,
			NewValue,
			GetOwningAbilitySystemComponent() ? GetOwningAbilitySystemComponent()->GetNumericAttribute(GetAttackDamageAttribute()) : GetAttackDamage(),
			GetOwningAbilitySystemComponent() ? GetOwningAbilitySystemComponent()->GetNumericAttribute(GetSpellDamageAttribute()) : GetSpellDamage(),
			GetOwningAbilitySystemComponent() ? GetOwningAbilitySystemComponent()->GetNumericAttribute(GetCastSpeedAttribute()) : GetCastSpeed());
	}
}

// ==============================================================================
// SOLID Helpers — Each stat scaling rule is isolated and self-documenting
// ==============================================================================

void UCharacterAttributeSet::RecalculateFromStrength(float OldStrength, float NewStrength)
{
	const float Diff = NewStrength - OldStrength;
	ApplyAdditiveAttributeBaseDelta(GetAttackDamageAttribute(), Diff * AttackDamagePerStrength);
	ApplyAdditiveAttributeBaseDelta(GetHealthRegenAttribute(), Diff * HealthRegenPerStrength);
	UE_LOG(
		LogCharacterAttributeSet,
		Log,
		TEXT("RecalculateFromStrength -> Old=%.2f New=%.2f Diff=%.2f AttackDamageBaseNow=%.2f HealthRegenBaseNow=%.2f"),
		OldStrength,
		NewStrength,
		Diff,
		GetAttributeBaseValue(GetAttackDamageAttribute()),
		GetAttributeBaseValue(GetHealthRegenAttribute()));
}

void UCharacterAttributeSet::RecalculateFromAgility(float OldAgility, float NewAgility)
{
	const float Diff = NewAgility - OldAgility;
	ApplyAdditiveAttributeBaseDelta(GetArmorAttribute(), Diff * ArmorPerAgility);
	ApplyAdditiveAttributeBaseDelta(GetCastSpeedAttribute(), Diff * CastSpeedPercentPerAgility);
}

void UCharacterAttributeSet::RecalculateFromIntellect(float OldIntellect, float NewIntellect)
{
	const float Diff = NewIntellect - OldIntellect;
	ApplyAdditiveAttributeBaseDelta(GetSpellDamageAttribute(), Diff * SpellDamagePerIntellect);
	ApplyAdditiveAttributeBaseDelta(GetManaRegenAttribute(), Diff * ManaRegenPerIntellect);

	const float OldMaxMana = GetAttributeBaseValue(GetMaxManaAttribute());
	const float NewMaxMana = OldMaxMana + (Diff * MaxManaPerIntellect);
	SetAttributeBaseValue(GetMaxManaAttribute(), NewMaxMana);

	AdjustAttributeProportionally(GetManaAttribute(), NewMaxMana, OldMaxMana);
}

void UCharacterAttributeSet::RecalculateFromStamina(float OldStamina, float NewStamina)
{
	const float Diff = NewStamina - OldStamina;
	const float OldMaxHealth = GetAttributeBaseValue(GetMaxHealthAttribute());
	const float NewMaxHealth = OldMaxHealth + (Diff * MaxHealthPerStamina);
	SetAttributeBaseValue(GetMaxHealthAttribute(), NewMaxHealth);

	AdjustAttributeProportionally(GetHealthAttribute(), NewMaxHealth, OldMaxHealth);
}

void UCharacterAttributeSet::SetAttributeBaseValue(const FGameplayAttribute& Attribute, float NewValue) const
{
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(Attribute, NewValue);
		return;
	}

	if (FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(const_cast<UCharacterAttributeSet*>(this)))
	{
		AttributeData->SetBaseValue(NewValue);
		AttributeData->SetCurrentValue(NewValue);
	}
}

float UCharacterAttributeSet::GetAttributeBaseValue(const FGameplayAttribute& Attribute) const
{
	if (const FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(this))
	{
		return AttributeData->GetBaseValue();
	}

	return Attribute.GetNumericValue(this);
}

void UCharacterAttributeSet::ApplyAdditiveAttributeBaseDelta(const FGameplayAttribute& Attribute, float Delta) const
{
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}

	SetAttributeBaseValue(Attribute, GetAttributeBaseValue(Attribute) + Delta);
}

void UCharacterAttributeSet::ClampHealth(float& NewValue) const
{
	NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
}

void UCharacterAttributeSet::ClampMana(float& NewValue) const
{
	NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
}

void UCharacterAttributeSet::ClampArmor(float& NewValue) const
{
	NewValue = FMath::Max(NewValue, 0.0f);
}

void UCharacterAttributeSet::ClampMagicResistance(float& NewValue) const
{
	NewValue = FMath::Max(NewValue, 0.0f);
}

void UCharacterAttributeSet::ClampCastSpeed(float& NewValue) const
{
	NewValue = FMath::Clamp(NewValue, 0.0f, MaxCastSpeedPercent);
}

void UCharacterAttributeSet::AdjustAttributeProportionally(const FGameplayAttribute& CurrentAttr, float NewMax, float OldMax)
{
	if (OldMax <= 0.0f || NewMax == OldMax)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float CurrentValue = CurrentAttr.GetNumericValue(this);
	const float NewCurrentValue = CurrentValue * (NewMax / OldMax);

	ASC->SetNumericAttributeBase(CurrentAttr, NewCurrentValue);
}

void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	ClampPostEffectAttribute(Data.EvaluatedData.Attribute);
}

void UCharacterAttributeSet::HandleDerivedAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	if (Attribute == GetStrengthAttribute())
	{
		RecalculateFromStrength(OldValue, NewValue);
		return;
	}

	if (Attribute == GetAgilityAttribute())
	{
		RecalculateFromAgility(OldValue, NewValue);
		return;
	}

	if (Attribute == GetIntellectAttribute())
	{
		RecalculateFromIntellect(OldValue, NewValue);
		return;
	}

	if (Attribute == GetStaminaAttribute())
	{
		RecalculateFromStamina(OldValue, NewValue);
		return;
	}
}

bool UCharacterAttributeSet::HandleClampedAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) const
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

	if (Attribute == GetArmorAttribute())
	{
		ClampArmor(NewValue);
		return true;
	}

	if (Attribute == GetMagicResistanceAttribute())
	{
		ClampMagicResistance(NewValue);
		return true;
	}

	if (Attribute == GetCastSpeedAttribute())
	{
		ClampCastSpeed(NewValue);
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
		return;
	}

	if (Attribute == GetArmorAttribute())
	{
		float Value = GetArmor();
		ClampArmor(Value);
		SetArmor(Value);
		return;
	}

	if (Attribute == GetMagicResistanceAttribute())
	{
		float Value = GetMagicResistance();
		ClampMagicResistance(Value);
		SetMagicResistance(Value);
		return;
	}

	if (Attribute == GetCastSpeedAttribute())
	{
		float Value = GetCastSpeed();
		ClampCastSpeed(Value);
		SetCastSpeed(Value);
	}
}
