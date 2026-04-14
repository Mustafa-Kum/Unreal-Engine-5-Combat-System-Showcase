#include "Characters/CharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/CombatComponent.h"
#include "Components/CombatImpactComponent.h"
#include "Components/DamageReceiverComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/ResourceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataAssets/CharacterDataAsset.h"
#include "DataAssets/WeaponDataAsset.h"
#include "GameplayAbilitySpec.h"
#include "WoWCloneGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterSetup, Log, All);

ACharacterBase::ACharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("AttributeSet"));

	EquipmentComp = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
	CombatImpactComp = CreateDefaultSubobject<UCombatImpactComponent>(TEXT("CombatImpactComponent"));

	WeaponMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(GetMesh());

	CombatFeedbackComp = CreateDefaultSubobject<UCombatFeedbackComponent>(TEXT("CombatFeedbackComponent"));
	DamageReceiverComp = CreateDefaultSubobject<UDamageReceiverComponent>(TEXT("DamageReceiverComponent"));
	ResourceComp = CreateDefaultSubobject<UResourceComponent>(TEXT("ResourceComponent"));

	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComp->SetGenerateOverlapEvents(false);

	HelmMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HelmMeshComp"));
	HelmMeshComp->SetupAttachment(GetMesh());

	ChestMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMeshComp"));
	ChestMeshComp->SetupAttachment(GetMesh());

	GauntletsMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GauntletsMeshComp"));
	GauntletsMeshComp->SetupAttachment(GetMesh());

	LeggingsMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LeggingsMeshComp"));
	LeggingsMeshComp->SetupAttachment(GetMesh());

	BootsMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BootsMeshComp"));
	BootsMeshComp->SetupAttachment(GetMesh());
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (USkeletalMeshComponent* MainMesh = GetMesh())
	{
		HelmMeshComp->SetLeaderPoseComponent(MainMesh);
		ChestMeshComp->SetLeaderPoseComponent(MainMesh);
		GauntletsMeshComp->SetLeaderPoseComponent(MainMesh);
		LeggingsMeshComp->SetLeaderPoseComponent(MainMesh);
		BootsMeshComp->SetLeaderPoseComponent(MainMesh);
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	InitializeCharacterStats();
	GrantStartupAbilities();
}

void ACharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

float ACharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float AppliedDamage = DamageReceiverComp
		? DamageReceiverComp->ReceiveDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser)
		: 0.0f;

	if (CombatFeedbackComp)
	{
		CombatFeedbackComp->HandleReceivedDamage(AppliedDamage, DamageEvent);
	}

	if (AppliedDamage > 0.0f)
	{
		if (UCombatComponent* CombatComp = FindComponentByClass<UCombatComponent>())
		{
			CombatComp->HandleReceivedDamage(AppliedDamage);
		}
	}

	Super::TakeDamage(AppliedDamage, DamageEvent, EventInstigator, DamageCauser);
	return AppliedDamage;
}

UAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float ACharacterBase::DealDamageToActor(AActor* TargetActor, float DamageAmount, const FDamageEvent& DamageEvent)
{
	if (!TargetActor || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = TargetActor->TakeDamage(DamageAmount, DamageEvent, GetController(), this);
	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	if (UCombatComponent* CombatComp = FindComponentByClass<UCombatComponent>())
	{
		CombatComp->NotifyDamageDealt();
	}

	return AppliedDamage;
}

void ACharacterBase::InitializeCharacterStats()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	if (CharacterClassData)
	{
		ApplyStartingStats(CharacterClassData->StartingStats);
		return;
	}

	FCharacterStartingStats FallbackStats;
	FallbackStats.InitialStrength = 2.0f;
	FallbackStats.InitialAgility = 1.0f;
	FallbackStats.InitialIntellect = 3.0f;
	FallbackStats.InitialStamina = 5.0f;
	FallbackStats.BaseArmor = 10.0f;
	FallbackStats.BaseMagicResistance = 0.0f;
	FallbackStats.BaseCriticalStrikeChance = 0.0f;
	FallbackStats.BaseMovementSpeed = 350.0f;
	FallbackStats.BasePhysicalDamage = 10.0f;
	FallbackStats.BaseMagicDamage = 10.0f;
	FallbackStats.BaseMaxMana = 100.0f;
	ApplyStartingStats(FallbackStats);
}

void ACharacterBase::GrantStartupAbilities()
{
	if (!AbilitySystemComponent || !HasAuthority() || !CharacterClassData)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : CharacterClassData->GrantedAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

void ACharacterBase::ApplyStartingStats(const FCharacterStartingStats& Stats)
{
	if (!AttributeSet)
	{
		return;
	}

	AttributeSet->InitializeStartingStats(Stats);

	if (AbilitySystemComponent)
	{
		UE_LOG(
			LogCharacterSetup,
			Log,
			TEXT("ApplyStartingStats -> AttackDamage=%.2f SpellDamage=%.2f Strength=%.2f Intellect=%.2f Agility=%.2f"),
			AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetAttackDamageAttribute()),
			AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetSpellDamageAttribute()),
			AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetStrengthAttribute()),
			AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetIntellectAttribute()),
			AbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetAgilityAttribute()));
	}
}

void ACharacterBase::OnWeaponEquipNotify()
{
	if (EquipmentComp)
	{
		EquipmentComp->HandleWeaponEquipNotify();
	}
}

void ACharacterBase::OnWeaponUnequipNotify()
{
	if (EquipmentComp)
	{
		EquipmentComp->HandleWeaponUnequipNotify();
	}
}

void ACharacterBase::ClearWeaponMesh()
{
	if (EquipmentComp)
	{
		EquipmentComp->ClearWeaponMesh();
	}
}

void ACharacterBase::RequestAreaIndicatorPreview(UObject* RequestOwner, float Radius) const
{
	if (CombatFeedbackComp)
	{
		CombatFeedbackComp->RequestGroundAreaIndicator(RequestOwner, Radius);
	}
}

void ACharacterBase::ClearAreaIndicatorPreview(UObject* RequestOwner) const
{
	if (CombatFeedbackComp)
	{
		CombatFeedbackComp->ClearGroundAreaIndicatorRequest(RequestOwner);
	}
}

void ACharacterBase::ToggleGameplayTagPair(UAbilitySystemComponent* ASC, const FGameplayTag& TagToRemove, const FGameplayTag& TagToAdd)
{
	if (!ASC)
	{
		return;
	}

	ASC->RemoveLooseGameplayTag(TagToRemove);
	ASC->AddLooseGameplayTag(TagToAdd);
}

bool ACharacterBase::HasWeaponEquipped() const
{
	return EquipmentComp && EquipmentComp->HasWeaponEquipped();
}

EWeaponType ACharacterBase::GetEquippedWeaponType() const
{
	return EquipmentComp ? EquipmentComp->GetEquippedWeaponType() : EWeaponType::None;
}
