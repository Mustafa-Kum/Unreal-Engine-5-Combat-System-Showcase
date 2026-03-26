#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataAssets/ItemDataAsset.h"
#include "DataAssets/WeaponDataAsset.h"
#include "InventoryComponent.generated.h"

// Forward Declarations (AAA: Minimize header coupling)
class ACharacterBase;

// --- WEAPON EQUIP STATE MACHINE ---
UENUM(BlueprintType)
enum class EWeaponEquipState : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	Equipping		UMETA(DisplayName = "Equipping"),
	Unequipping		UMETA(DisplayName = "Unequipping")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WOWCLONE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInventoryComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	/** --- PUBLIC API --- */
	UFUNCTION(BlueprintCallable, Category = "Inventory", meta = (BlueprintInternalUseOnly = "true"))
	void SetItemAtIndex(UItemDataAsset* Item, int32 Quantity, int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItem(UItemDataAsset* NewItem, int32 Quantity = 1);

	// Multi-Slot Equip logic
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool EquipItemAtIndex(int32 Index, EEquipmentSlot TargetSlot);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UnequipItem(EEquipmentSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UnequipItemToSlot(EEquipmentSlot Slot, int32 TargetIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeItemAtIndex(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ToggleDrawHolster();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SwapInventorySlots(int32 SourceIndex, int32 TargetIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool PrepareWeaponForCombat();

	/** --- QUERY API --- */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	[[nodiscard]] bool IsWeaponActionInProgress() const { return WeaponEquipState != EWeaponEquipState::Idle; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	[[nodiscard]] bool HasItemEquippedAtSlot(EEquipmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	[[nodiscard]] UItemDataAsset* GetItemAtIndex(int32 Index) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	[[nodiscard]] int32 GetItemQuantityAtIndex(int32 Index) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	[[nodiscard]] UItemDataAsset* GetEquippedItem(EEquipmentSlot Slot) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	[[nodiscard]] int32 GetInventorySlotCount() const { return InventorySlots.Num(); }

	[[nodiscard]] int32 FindEmptySlotIndex() const;
	[[nodiscard]] int32 FindFirstEquippableItemIndex(EEquipmentSlot TargetSlot) const;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

protected:
	virtual void OnEquipMontageLoaded(class UWeaponDataAsset* WeaponToEquip, int32 RequestId);
	virtual void OnUnequipMontageLoaded(class UWeaponDataAsset* WeaponToUnequip, int32 RequestId);

	void OnWeaponActionCompleted();
	void SetEquipState(EWeaponEquipState NewState);
	
	[[nodiscard]] bool CanEquipItem(int32 Index, EEquipmentSlot TargetSlot) const;
	[[nodiscard]] bool CanPerformWeaponAction() const;
	[[nodiscard]] bool HasValidInventoryItemAtIndex(int32 Index) const;
	[[nodiscard]] bool EnsureCombatWeaponEquipped();
	[[nodiscard]] bool EnsureCombatWeaponDrawn();

	bool Internal_ProcessEquipFlow(int32 Index, EEquipmentSlot TargetSlot);
	bool ProcessEquipBackend(int32 Index, EEquipmentSlot TargetSlot, UItemDataAsset* ItemToEquip);
	bool HandleTwoHandedEquipConstraints(UItemDataAsset* ItemToEquip, EEquipmentSlot TargetSlot);
	void ExecuteEquipVisuals(EEquipmentSlot TargetSlot);
	bool Internal_ProcessUnequipFlow(EEquipmentSlot Slot, int32 TargetIndex);
	void Internal_SwapEquippedWithInventory(int32 Index, EEquipmentSlot TargetSlot);
	void Internal_HandleWeaponTransition(UWeaponDataAsset* WeaponToTransition, bool bIsEquipping);
	
	void MountItemStats(UItemDataAsset* Item);
	void DismountItemStats(UItemDataAsset* Item);

	void ExecuteVisualTransition(UWeaponDataAsset* WeaponData, bool bIsEquipping);
	void ExecuteInstantClear();
	void PlayWeaponMontage(UWeaponDataAsset* WeaponData, EWeaponEquipState ExpectedState, int32 RequestId);

	void ProcessMontageLoad(UWeaponDataAsset* WeaponData, const TSoftObjectPtr<UAnimMontage>& MontageToLoad, bool bIsEquipping);
	void HandlePendingMontage(UWeaponDataAsset* WeaponData, const TSoftObjectPtr<UAnimMontage>& MontageToLoad, bool bIsEquipping);
	void HandleValidMontage(UWeaponDataAsset* WeaponData, bool bIsEquipping);
	void HandleMissingMontage(bool bIsEquipping);
	void SnapWeaponWithoutAnimation(bool bIsEquip);
	[[nodiscard]] bool IsWeaponTransitionRequestCurrent(UWeaponDataAsset* WeaponData, EWeaponEquipState ExpectedState, int32 RequestId) const;
	void ClearPendingWeaponTransition();

	void FinalizeUnequipAction();

	void SetItemAtIndexInternal(UItemDataAsset* Item, int32 Quantity, int32 Index, bool bBroadcast);
	void BroadcastInventoryUpdated();
	[[nodiscard]] int32 ResolveUnequipTargetIndex(UItemDataAsset* ItemToUnequip, int32 PreferredIndex) const;
	void NormalizeSlotWrite(UItemDataAsset*& Item, int32& Quantity) const;

private:
	/** --- CORE DATA --- */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UItemDataAsset> DefaultStartingItem;

	UPROPERTY(VisibleAnywhere, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryItem> InventorySlots;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	int32 MaxInventorySlots = 20;

	UPROPERTY(VisibleAnywhere, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TMap<EEquipmentSlot, TObjectPtr<UItemDataAsset>> EquippedItems;

	UPROPERTY(VisibleAnywhere, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	EWeaponEquipState WeaponEquipState = EWeaponEquipState::Idle;

	UPROPERTY(Transient)
	TObjectPtr<ACharacterBase> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponDataAsset> PendingWeaponTransitionData;

	EWeaponEquipState PendingWeaponTransitionState = EWeaponEquipState::Idle;
	int32 PendingWeaponTransitionRequestId = 0;
};
