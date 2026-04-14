#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataAssets/ItemDataAsset.h"
#include "InventoryComponent.generated.h"

// Forward Declarations (AAA: Minimize header coupling)
class ACharacterBase;
class UEquipmentComponent;
class UWeaponActionComponent;
enum class EWeaponEquipState : uint8;

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

	/** --- QUERY API --- */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	[[nodiscard]] bool IsWeaponActionInProgress() const;

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

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

protected:
	[[nodiscard]] bool CanEquipItem(int32 Index, EEquipmentSlot TargetSlot) const;
	[[nodiscard]] bool HasValidInventoryItemAtIndex(int32 Index) const;

	bool Internal_ProcessEquipFlow(int32 Index, EEquipmentSlot TargetSlot);
	bool ProcessEquipBackend(int32 Index, EEquipmentSlot TargetSlot, UItemDataAsset* ItemToEquip);
	bool HandleTwoHandedEquipConstraints(UItemDataAsset* ItemToEquip, EEquipmentSlot TargetSlot);
	void ExecuteEquipVisuals(EEquipmentSlot TargetSlot);
	bool Internal_ProcessUnequipFlow(EEquipmentSlot Slot, int32 TargetIndex);
	[[nodiscard]] bool Internal_SwapEquippedWithInventory(int32 Index, EEquipmentSlot TargetSlot);
	[[nodiscard]] bool BeginDeferredMainHandSwap(int32 Index, EEquipmentSlot TargetSlot, UItemDataAsset* IncomingItem);
	void FinalizeDeferredMainHandSwap();
	void ClearDeferredMainHandSwap();
	[[nodiscard]] bool HasDeferredMainHandSwap() const;
	void ExecuteInstantClear();
	void HandleWeaponTransitionCompleted(EWeaponEquipState CompletedState);
	void FinalizeUnequipAction();

	void SetItemAtIndexInternal(UItemDataAsset* Item, int32 Quantity, int32 Index, bool bBroadcast);
	void BroadcastInventoryUpdated();
	[[nodiscard]] int32 FindEmptySlotIndex() const;
	[[nodiscard]] int32 ResolveUnequipTargetIndex(UItemDataAsset* ItemToUnequip, int32 PreferredIndex) const;
	void NormalizeSlotWrite(UItemDataAsset*& Item, int32& Quantity) const;
	[[nodiscard]] UEquipmentComponent* GetEquipmentComponent() const;
	[[nodiscard]] UWeaponActionComponent* GetWeaponActionComponent() const;

private:
	/** --- CORE DATA --- */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UItemDataAsset> DefaultStartingItem;

	UPROPERTY(VisibleAnywhere, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<FInventoryItem> InventorySlots;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	int32 MaxInventorySlots = 20;

	UPROPERTY(Transient)
	TObjectPtr<ACharacterBase> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UItemDataAsset> PendingMainHandSwapIncomingItem;

	int32 PendingMainHandSwapInventoryIndex = INDEX_NONE;
	EEquipmentSlot PendingMainHandSwapSlot = EEquipmentSlot::None;
	FDelegateHandle WeaponTransitionCompletedDelegateHandle;

	mutable TWeakObjectPtr<UEquipmentComponent> CachedEquipment;
	mutable TWeakObjectPtr<UWeaponActionComponent> CachedWeaponAction;
};
