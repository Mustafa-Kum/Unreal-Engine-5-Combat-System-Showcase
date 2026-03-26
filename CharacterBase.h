#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "ActiveGameplayEffectHandle.h"
// AAA: Forward declare enum to minimize header coupling (avoid pulling entire DataAsset header into every Character include)
enum class EWeaponType : uint8;
enum class EEquipmentSlot : uint8;
class UCombatComponent;
class UCombatFeedbackComponent;
class UDamageReceiverComponent;
#include "CharacterBase.generated.h"

// AAA: Delegate for weapon state machine — CharacterBase broadcasts, InventoryComponent listens (DIP)
DECLARE_MULTICAST_DELEGATE(FOnWeaponActionFinished);

UCLASS(Abstract, Blueprintable)
class WOWCLONE_API ACharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACharacterBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// IAbilitySystemInterface
	[[nodiscard]] virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// SOLID: Virtual getter for AnimInstance to read multiplier safely without casting to child classes.
	// This makes CharacterBase open for extension but closed for modification.
	[[nodiscard]] virtual float GetBackwardMovementSpeedMultiplier() const { return 1.0f; }

	// AnimInstance'ın combat durumunu okuması için (SOLID: Open/Closed Principle)
	[[nodiscard]] virtual bool IsInCombat() const { return false; }

	// Şu an equipped silahın tipini döner (AnimGraph: Blend Poses by Enum)
	[[nodiscard]] virtual EWeaponType GetEquippedWeaponType() const;

	// Animasyon sisteminin karakterin elinde silah olup olmadığını anlaması için (AAA: Encapsulation)
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	[[nodiscard]] bool HasWeaponEquipped() const;

public:
	// --- WEAPON SYSTEM ---

	// Blueprint'ten veya Animation Notify'dan çağrılır: Silahı ele geçir
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void OnWeaponEquipNotify();

	// Blueprint'ten veya Animation Notify'dan çağrılır: Silahı sırta as
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void OnWeaponUnequipNotify();

	// Silahı karakterden tamamen kaldırır (Mesh'i siler ve referansı sıfırlar)
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ClearWeaponMesh();

	// --- ITEM STAT MODIFIERS ---
	// Eşya takıldığında veya çıkarıldığında statları güncellemek için (AAA Standard)
	virtual void ApplyItemStats(class UItemDataAsset* ItemData);
	virtual void RemoveItemStats(class UItemDataAsset* ItemData);

	// Tüketilebilir eşyalar (Consumables) için tek seferlik etki (AAA Standard)
	virtual void ApplyConsumableEffect(class UItemDataAsset* ItemData);

	// --- VIRTUAL INPUT HOOKS (AAA: DIP - Decoupling Components from Hero class) ---
	virtual void Move(const FInputActionValue& Value) {}
	virtual void Look(const FInputActionValue& Value) {}
	virtual void RightClickStarted() {}
	virtual void RightClickCompleted() {}
	virtual void LeftClickStarted() {}
	virtual void LeftClickCompleted() {}
	virtual void Zoom(const FInputActionValue& Value) {}
	virtual void ToggleWeapon() {}
	virtual void ToggleWalk() {}
	virtual void ToggleCombat() {}
	virtual void ToggleInventory() {}
	virtual void TestVitals() {}

	// InventoryComponent tetikler, montaj süresince bekleyeceğimiz hedef silahı kaydeder
	virtual void SetPendingWeapon(class UWeaponDataAsset* InitialWeaponData);

protected:
	// Async Asset Loading Callbacks (AAA Standard)
	virtual void OnWeaponMeshLoaded(class UWeaponDataAsset* LoadedWeaponData);

	[[nodiscard]] bool IsWeaponLoadValid(class UWeaponDataAsset* LoadedWeaponData) const;
	void ApplyWeaponMesh(class UWeaponDataAsset* LoadedWeaponData);
	void AttachLoadedWeaponToDesiredSocket(class UWeaponDataAsset* LoadedWeaponData);

	// AAA DRY: Shared tag-swap utility used by SetArmedState and subclass combat tag toggles
	void ToggleGameplayTagPair(class UAbilitySystemComponent* ASC, const FGameplayTag& TagToRemove, const FGameplayTag& TagToAdd);

	// AAA DRY: Shared GE application helper — single source of truth for applying item effects
	FActiveGameplayEffectHandle ApplyGameplayEffectFromItem(class UItemDataAsset* ItemData);

private:
	friend class UCombatComponent;

	// DRY Helpers: Eliminate duplication between Equip/Unequip notify methods
	[[nodiscard]] bool CanProcessWeaponNotify() const;
	[[nodiscard]] class UPrimitiveComponent* GetMeleeHitCollisionComponent() const;
	void SetMeleeHitCollisionEnabled(bool bEnabled);
	void AttachWeaponToSocket(FName SocketName);
	[[nodiscard]] FName GetDesiredWeaponSocketName(const class UWeaponDataAsset* WeaponData) const;
	void SetArmedState(bool bIsArmed);

	// DRY: Unified stat initialization from a stats struct
	void ApplyStartingStats(const struct FCharacterStartingStats& Stats);

	// Applies base stats from the assigned CharacterDataAsset (called from BeginPlay)
	void InitializeCharacterStats();

	// Ability System Component (GAS)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UAbilitySystemComponent> AbilitySystemComponent;

	// Character Attribute Set (Primary, Secondary, Derived Stats)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCharacterAttributeSet> AttributeSet;

	// AAA Data-Driven Setup
	// Sınıfın (Mage, Warrior vb.) başlangıç değerlerini ve yeteneklerini tutan Data Asset
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Setup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCharacterDataAsset> CharacterClassData;

	// --- MODULAR CHARACTER MESHES (AAA Standard) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkeletalMeshComponent> HelmMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkeletalMeshComponent> ChestMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkeletalMeshComponent> GauntletsMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkeletalMeshComponent> LeggingsMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkeletalMeshComponent> BootsMeshComp;

	void UpdateEquipmentMesh(EEquipmentSlot Slot, TSoftObjectPtr<class USkeletalMesh> MeshAsset);
	void ClearEquipmentMesh(EEquipmentSlot Slot);
	[[nodiscard]] class USkeletalMeshComponent* GetMeshComponentForSlot(EEquipmentSlot Slot) const;

	// Silahı karakterin modelinde tutacağımız Component (AAA Standartı: Varsayılan olarak collision kapalı)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStaticMeshComponent> WeaponMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatFeedbackComponent> CombatFeedbackComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDamageReceiverComponent> DamageReceiverComp;

	// Karakterin şu an hedeflediği veya tuttuğu silahın referansı
	UPROPERTY(Transient)
	TObjectPtr<class UWeaponDataAsset> CurrentWeaponData;

	uint8 bWantsWeaponArmed : 1;

	// AAA: Takılan ekipmanların buff/efekt Handle'larını (kimliklerini) tutarak temiz bir çıkarma sağlar
	TMap<EEquipmentSlot, FActiveGameplayEffectHandle> ActiveEquipmentEffects;
	TMap<EEquipmentSlot, TSoftObjectPtr<class USkeletalMesh>> PendingEquipmentMeshes;

public:
	// --- WEAPON STATE MACHINE DELEGATE ---
	// AnimNotify tamamlandığında broadcast edilir, InventoryComponent state geçişi yapar (AAA: DIP)
	FOnWeaponActionFinished OnWeaponActionFinished;
};
