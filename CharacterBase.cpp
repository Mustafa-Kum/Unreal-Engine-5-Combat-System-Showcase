#include "Characters/CharacterBase.h"
#include "Components/StaticMeshComponent.h"
#include "DataAssets/WeaponDataAsset.h"
#include "DataAssets/CharacterDataAsset.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "WoWCloneGameplayTags.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/DamageReceiverComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

ACharacterBase::ACharacterBase()
{
	// Performance optimization: disabling Tick function if not strictly needed.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Ability System Component Kurulumu (GAS)
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	
	// Character Attribute Set Kurulumu (Stats)
	AttributeSet = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("AttributeSet"));

	// Weapon Component Kurulumu (AAA: Varsayılan olarak çarpışmaları kapalı tutuyoruz)
	WeaponMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComp"));
	WeaponMeshComp->SetupAttachment(GetMesh()); // Varsayılan olarak kemik belirsiz, dinamik bağlanacak

	CombatFeedbackComp = CreateDefaultSubobject<UCombatFeedbackComponent>(TEXT("CombatFeedbackComponent"));
	DamageReceiverComp = CreateDefaultSubobject<UDamageReceiverComponent>(TEXT("DamageReceiverComponent"));
	
	// Çarpışmaları tamamen kapatıyoruz ki mermi sektirmesin, kamerayı sarsmasın veya karakteri itmesin
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComp->SetGenerateOverlapEvents(false); // Performans için gereksiz tetiklenmeleri önleriz

	// Modular Character Meshes
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

	bWantsWeaponArmed = 0;
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Setup Modular Meshes to follow the main mesh animation strictly
	if (USkeletalMeshComponent* MainMesh = GetMesh())
	{
		HelmMeshComp->SetLeaderPoseComponent(MainMesh);
		ChestMeshComp->SetLeaderPoseComponent(MainMesh);
		GauntletsMeshComp->SetLeaderPoseComponent(MainMesh);
		LeggingsMeshComp->SetLeaderPoseComponent(MainMesh);
		BootsMeshComp->SetLeaderPoseComponent(MainMesh);
	}

	// Load setup from Data Asset
	InitializeCharacterStats();

	// GAS: Başlangıçta karakterin elinde silah olmadığını (Unarmed) sisteme bildiriyoruz.
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AddLooseGameplayTag(WoWCloneTags::State_Unarmed);
		AbilitySystemComponent->AddLooseGameplayTag(WoWCloneTags::State_Idle);
	}
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
	Super::TakeDamage(AppliedDamage, DamageEvent, EventInstigator, DamageCauser);
	return AppliedDamage;
}

UAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
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
	}
	else
	{
		// AAA: Safe Fallback — construct default stats if Data Asset is missing
		FCharacterStartingStats FallbackStats;
		FallbackStats.InitialStrength = 2.0f;
		FallbackStats.InitialAgility = 1.0f;
		FallbackStats.InitialIntellect = 3.0f;
		FallbackStats.InitialStamina = 5.0f;
		FallbackStats.BaseArmor = 10.0f;
		FallbackStats.BaseCriticalStrikeChance = 0.0f;
		FallbackStats.BaseMovementSpeed = 350.0f;
		FallbackStats.BasePhysicalDamage = 10.0f;
		FallbackStats.BaseMagicDamage = 10.0f;
		FallbackStats.BaseMaxMana = 100.0f;
		ApplyStartingStats(FallbackStats);
	}
}

UPrimitiveComponent* ACharacterBase::GetMeleeHitCollisionComponent() const
{
	return WeaponMeshComp;
}

void ACharacterBase::ApplyStartingStats(const FCharacterStartingStats& Stats)
{
	if (!AttributeSet) return;

	// 1. Initialize Base Derived Values and Non-Scaling Attributes (Quiet Init)
	AttributeSet->InitHealth(100.0f);
	AttributeSet->InitMaxHealth(100.0f);
	AttributeSet->InitMana(Stats.BaseMaxMana);
	AttributeSet->InitMaxMana(Stats.BaseMaxMana);
	
	AttributeSet->InitCriticalStrikeChance(Stats.BaseCriticalStrikeChance);
	AttributeSet->InitMovementSpeed(Stats.BaseMovementSpeed);
	AttributeSet->InitAttackDamage(Stats.BasePhysicalDamage);
	AttributeSet->InitSpellDamage(Stats.BaseMagicDamage);
	AttributeSet->InitArmor(Stats.BaseArmor);
	
	// Ensure Weapon Interval is set so Agility scaling has a base to work from
	AttributeSet->InitWeaponBaseInterval(0.0f); 

	// 2. Set Primary Stats (Active Setters trigger PreAttributeChange Scaling logic)
	// This ensures logic like "1 Stamina = 10 MaxHealth" is applied on top of the base 100.0.
	AttributeSet->SetStrength(Stats.InitialStrength);
	AttributeSet->SetAgility(Stats.InitialAgility);
	AttributeSet->SetIntellect(Stats.InitialIntellect);
	AttributeSet->SetStamina(Stats.InitialStamina);
}

// ==============================================================================
// AAA DRY: Shared GE Application — Single Source of Truth
// ==============================================================================

FActiveGameplayEffectHandle ACharacterBase::ApplyGameplayEffectFromItem(UItemDataAsset* ItemData)
{
	if (!AbilitySystemComponent || !ItemData || !ItemData->ItemData.EquippedStatEffect)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(ItemData->ItemData.EquippedStatEffect, 1.0f, EffectContext);
	if (SpecHandle.IsValid())
	{
		return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return FActiveGameplayEffectHandle();
}

void ACharacterBase::ApplyItemStats(UItemDataAsset* ItemData)
{
	if (!AbilitySystemComponent || !ItemData) return;

	if (UWeaponDataAsset* WeaponData = Cast<UWeaponDataAsset>(ItemData))
	{
		AbilitySystemComponent->ApplyModToAttributeUnsafe(UCharacterAttributeSet::GetWeaponBaseIntervalAttribute(), EGameplayModOp::Override, WeaponData->WeaponData.WeaponCastSpeed);
		AbilitySystemComponent->ApplyModToAttributeUnsafe(UCharacterAttributeSet::GetAttackDamageAttribute(), EGameplayModOp::Additive, WeaponData->WeaponData.BaseDamage);
	}

	UpdateEquipmentMesh(ItemData->ItemData.ValidEquipmentSlot, ItemData->ItemData.EquipmentMesh);

	FActiveGameplayEffectHandle Handle = ApplyGameplayEffectFromItem(ItemData);
	if (Handle.IsValid())
	{
		ActiveEquipmentEffects.Add(ItemData->ItemData.ValidEquipmentSlot, Handle);
	}
}

void ACharacterBase::RemoveItemStats(UItemDataAsset* ItemData)
{
	if (!AbilitySystemComponent || !ItemData) return;

	if (UWeaponDataAsset* WeaponData = Cast<UWeaponDataAsset>(ItemData))
	{
		AbilitySystemComponent->ApplyModToAttributeUnsafe(UCharacterAttributeSet::GetWeaponBaseIntervalAttribute(), EGameplayModOp::Override, 0.0f);
		AbilitySystemComponent->ApplyModToAttributeUnsafe(UCharacterAttributeSet::GetAttackDamageAttribute(), EGameplayModOp::Additive, -WeaponData->WeaponData.BaseDamage);
	}

	ClearEquipmentMesh(ItemData->ItemData.ValidEquipmentSlot);

	if (FActiveGameplayEffectHandle* HandlePtr = ActiveEquipmentEffects.Find(ItemData->ItemData.ValidEquipmentSlot))
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(*HandlePtr);
		ActiveEquipmentEffects.Remove(ItemData->ItemData.ValidEquipmentSlot);
	}
}

void ACharacterBase::ApplyConsumableEffect(UItemDataAsset* ItemData)
{
	// Consumable effects are instant/duration — no need to store the handle
	ApplyGameplayEffectFromItem(ItemData);
}

void ACharacterBase::SetPendingWeapon(UWeaponDataAsset* InitialWeaponData)
{
	if (!InitialWeaponData) return;

	CurrentWeaponData = InitialWeaponData;
	
	// AAA Standard: Senkron yerine asenkron (arka planda) model yüklemesi
	if (CurrentWeaponData->WeaponData.WeaponMesh.IsPending())
	{
		FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &ACharacterBase::OnWeaponMeshLoaded, CurrentWeaponData.Get());
		UAssetManager::GetStreamableManager().RequestAsyncLoad(CurrentWeaponData->WeaponData.WeaponMesh.ToSoftObjectPath(), Delegate);
	}
	else if (CurrentWeaponData->WeaponData.WeaponMesh.IsValid())
	{
		OnWeaponMeshLoaded(CurrentWeaponData);
	}
}

void ACharacterBase::OnWeaponMeshLoaded(UWeaponDataAsset* LoadedWeaponData)
{
	if (IsWeaponLoadValid(LoadedWeaponData))
	{
		ApplyWeaponMesh(LoadedWeaponData);
		AttachLoadedWeaponToDesiredSocket(LoadedWeaponData);
	}
}

bool ACharacterBase::IsWeaponLoadValid(UWeaponDataAsset* LoadedWeaponData) const
{
	// Eğer yükleme bitene kadar karakter başka bir silah seçtiyse (veya nil olduysa) iptal et
	return LoadedWeaponData && CurrentWeaponData == LoadedWeaponData;
}

void ACharacterBase::ApplyWeaponMesh(UWeaponDataAsset* LoadedWeaponData)
{
	if (WeaponMeshComp && LoadedWeaponData->WeaponData.WeaponMesh.IsValid())
	{
		WeaponMeshComp->SetStaticMesh(LoadedWeaponData->WeaponData.WeaponMesh.Get());
	}
}

void ACharacterBase::AttachLoadedWeaponToDesiredSocket(UWeaponDataAsset* LoadedWeaponData)
{
	AttachWeaponToSocket(GetDesiredWeaponSocketName(LoadedWeaponData));
}

void ACharacterBase::OnWeaponEquipNotify()
{
	if (CanProcessWeaponNotify())
	{
		SetMeleeHitCollisionEnabled(false);
		bWantsWeaponArmed = 1;
		AttachWeaponToSocket(GetDesiredWeaponSocketName(CurrentWeaponData));
		SetArmedState(true);
	}

	OnWeaponActionFinished.Broadcast();
}

void ACharacterBase::OnWeaponUnequipNotify()
{
	// Animasyon o frame'e geldiğinde (Kılıç sırta girdiği an) çalışır
	if (CanProcessWeaponNotify())
	{
		SetMeleeHitCollisionEnabled(false);
		bWantsWeaponArmed = 0;
		AttachWeaponToSocket(GetDesiredWeaponSocketName(CurrentWeaponData));
		SetArmedState(false);
	}

	OnWeaponActionFinished.Broadcast();
}

void ACharacterBase::ClearWeaponMesh()
{
	if (WeaponMeshComp)
	{
		SetMeleeHitCollisionEnabled(false);
		WeaponMeshComp->SetStaticMesh(nullptr);
	}
	CurrentWeaponData = nullptr;
	bWantsWeaponArmed = 0;
}

// ==============================================================================
// DRY Helpers — Single Source of Truth (AAA Clean Code)
// ==============================================================================

bool ACharacterBase::CanProcessWeaponNotify() const
{
	if (!WeaponMeshComp)
	{
		return false;
	}

	return CurrentWeaponData != nullptr;
}

void ACharacterBase::SetMeleeHitCollisionEnabled(bool bEnabled)
{
	if (!WeaponMeshComp)
	{
		return;
	}

	const bool bShouldEnable = bEnabled
		&& CurrentWeaponData != nullptr
		&& WeaponMeshComp->GetStaticMesh() != nullptr
		&& HasWeaponEquipped();

	WeaponMeshComp->SetCollisionEnabled(bShouldEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	WeaponMeshComp->SetGenerateOverlapEvents(bShouldEnable);
}

void ACharacterBase::AttachWeaponToSocket(FName SocketName)
{
	WeaponMeshComp->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
}

FName ACharacterBase::GetDesiredWeaponSocketName(const UWeaponDataAsset* WeaponData) const
{
	if (!WeaponData)
	{
		return NAME_None;
	}

	return bWantsWeaponArmed ? WeaponData->WeaponData.EquipSocketName : WeaponData->WeaponData.HolsterSocketName;
}

void ACharacterBase::ToggleGameplayTagPair(UAbilitySystemComponent* ASC, const FGameplayTag& TagToRemove, const FGameplayTag& TagToAdd)
{
	if (!ASC) return;

	ASC->RemoveLooseGameplayTag(TagToRemove);
	ASC->AddLooseGameplayTag(TagToAdd);
}

void ACharacterBase::SetArmedState(bool bIsArmed)
{
	const FGameplayTag& TagToRemove = bIsArmed ? WoWCloneTags::State_Unarmed : WoWCloneTags::State_Armed;
	const FGameplayTag& TagToAdd = bIsArmed ? WoWCloneTags::State_Armed : WoWCloneTags::State_Unarmed;

	ToggleGameplayTagPair(AbilitySystemComponent, TagToRemove, TagToAdd);
}

bool ACharacterBase::HasWeaponEquipped() const
{
	if (!WeaponMeshComp || !CurrentWeaponData) return false;

	// Kılıcın karakterin mesh üzerindeki hangi sokete bağlı olduğuna bakıyoruz.
	// Bu sayede ekstra bir "bIsEquipped" boolean tutmadan fiziksel gerçeği sorgulamış oluyoruz (Single Source of Truth).
	return WeaponMeshComp->GetAttachSocketName() == CurrentWeaponData->WeaponData.EquipSocketName;
}

EWeaponType ACharacterBase::GetEquippedWeaponType() const
{
	if (CurrentWeaponData && HasWeaponEquipped())
	{
		return CurrentWeaponData->WeaponData.WeaponType;
	}
	return EWeaponType::None;
}

void ACharacterBase::UpdateEquipmentMesh(EEquipmentSlot Slot, TSoftObjectPtr<USkeletalMesh> MeshAsset)
{
	USkeletalMeshComponent* TargetComp = GetMeshComponentForSlot(Slot);
	if (!TargetComp) return;

	if (MeshAsset.IsNull())
	{
		PendingEquipmentMeshes.Remove(Slot);
		TargetComp->SetSkeletalMesh(nullptr);
		return;
	}

	PendingEquipmentMeshes.Add(Slot, MeshAsset);

	if (MeshAsset.IsPending())
	{
		FStreamableDelegate Delegate = FStreamableDelegate::CreateWeakLambda(this, [this, Slot, MeshAsset]()
		{
			if (PendingEquipmentMeshes.FindRef(Slot) != MeshAsset || !MeshAsset.IsValid())
			{
				return;
			}

			if (USkeletalMeshComponent* LoadedTargetComp = GetMeshComponentForSlot(Slot))
			{
				LoadedTargetComp->SetSkeletalMesh(MeshAsset.Get());
			}
		});

		UAssetManager::GetStreamableManager().RequestAsyncLoad(MeshAsset.ToSoftObjectPath(), Delegate);
		return;
	}

	if (MeshAsset.IsValid())
	{
		TargetComp->SetSkeletalMesh(MeshAsset.Get());
	}
}

void ACharacterBase::ClearEquipmentMesh(EEquipmentSlot Slot)
{
	if (USkeletalMeshComponent* TargetComp = GetMeshComponentForSlot(Slot))
	{
		PendingEquipmentMeshes.Remove(Slot);
		TargetComp->SetSkeletalMesh(nullptr);
	}
}

USkeletalMeshComponent* ACharacterBase::GetMeshComponentForSlot(EEquipmentSlot Slot) const
{
	switch (Slot)
	{
		case EEquipmentSlot::Helm: return HelmMeshComp;
		case EEquipmentSlot::Chest: return ChestMeshComp;
		case EEquipmentSlot::Gauntlets: return GauntletsMeshComp;
		case EEquipmentSlot::Leggings: return LeggingsMeshComp;
		case EEquipmentSlot::Boots: return BootsMeshComp;
		default: return nullptr;
	}
}
