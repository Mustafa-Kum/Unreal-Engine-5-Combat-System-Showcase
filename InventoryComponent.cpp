#include "Components/InventoryComponent.h"
#include "Characters/CharacterBase.h"
#include "Components/EquipmentComponent.h"
#include "Components/WeaponActionComponent.h"
#include "DataAssets/ItemDataAsset.h"
#include "DataAssets/WeaponDataAsset.h"

// Custom Log Category
DEFINE_LOG_CATEGORY_STATIC(LogInventory, Log, All);

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// Pre-allocate slots
	InventorySlots.SetNum(MaxInventorySlots);

	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogInventory, Error, TEXT("InventoryComponent MUST be attached to a CharacterBase!"));
		return;
	}

	if (!GetEquipmentComponent())
	{
		UE_LOG(LogInventory, Error, TEXT("InventoryComponent requires EquipmentComponent on %s."), *GetNameSafe(OwnerCharacter));
		return;
	}

	UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent();
	if (!WeaponActionComp)
	{
		UE_LOG(LogInventory, Error, TEXT("InventoryComponent requires WeaponActionComponent on %s."), *GetNameSafe(OwnerCharacter));
		return;
	}

	WeaponTransitionCompletedDelegateHandle = WeaponActionComp->OnWeaponTransitionCompleted.AddUObject(this, &UInventoryComponent::HandleWeaponTransitionCompleted);

	if (DefaultStartingItem)
	{
		AddItem(DefaultStartingItem, 1);
		UE_LOG(LogInventory, Log, TEXT("Started with item in bag: %s"), *DefaultStartingItem->ItemData.ItemName.ToString());
	}
}

void UInventoryComponent::SetItemAtIndex(UItemDataAsset* Item, int32 Quantity, int32 Index)
{
	SetItemAtIndexInternal(Item, Quantity, Index, true);
}

void UInventoryComponent::AddItem(UItemDataAsset* NewItem, int32 Quantity)
{
	if (!NewItem || Quantity <= 0) return;

	int32 RemainingQuantity = Quantity;
	bool bInventoryChanged = false;
	const int32 SlotCount = InventorySlots.Num();

	// Phase 1: Try to stack with existing items (AAA Stacking Logic)
	for (int32 i = 0; i < SlotCount; ++i)
	{
		if (InventorySlots[i].IsValid() && InventorySlots[i].ItemData == NewItem)
		{
			const int32 SpaceLeft = NewItem->ItemData.MaxStackSize - InventorySlots[i].Quantity;
			if (SpaceLeft > 0)
			{
				const int32 AmountToAdd = FMath::Min(RemainingQuantity, SpaceLeft);
				InventorySlots[i].Quantity += AmountToAdd;
				RemainingQuantity -= AmountToAdd;
				bInventoryChanged = true;
				UE_LOG(LogInventory, Log, TEXT("Stacked Item: %s to Slot: %d. New Qty: %d"), *NewItem->ItemData.ItemName.ToString(), i, InventorySlots[i].Quantity);

				if (RemainingQuantity <= 0)
				{
					BroadcastInventoryUpdated();
					return;
				}
			}
		}
	}

	// Phase 2: Add into empty slots
	while (RemainingQuantity > 0)
	{
		const int32 EmptyIndex = FindEmptySlotIndex();
		if (EmptyIndex == INDEX_NONE)
		{
			if (bInventoryChanged)
			{
				BroadcastInventoryUpdated();
			}
			UE_LOG(LogInventory, Warning, TEXT("Inventory Full! Partially added %d. Dropping %d of %s"), Quantity - RemainingQuantity, RemainingQuantity, *NewItem->ItemData.ItemName.ToString());
			return;
		}

		const int32 AmountToAdd = FMath::Min(RemainingQuantity, NewItem->ItemData.MaxStackSize);
		SetItemAtIndexInternal(NewItem, AmountToAdd, EmptyIndex, false);
		RemainingQuantity -= AmountToAdd;
		bInventoryChanged = true;
	}

	if (bInventoryChanged)
	{
		BroadcastInventoryUpdated();
	}
}

int32 UInventoryComponent::FindEmptySlotIndex() const
{
	for (int32 i = 0; i < InventorySlots.Num(); ++i)
	{
		if (!InventorySlots[i].IsValid())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool UInventoryComponent::EquipItemAtIndex(int32 Index, EEquipmentSlot TargetSlot)
{
	if (!CanEquipItem(Index, TargetSlot)) return false;

	return Internal_ProcessEquipFlow(Index, TargetSlot);
}

bool UInventoryComponent::ConsumeItemAtIndex(int32 SlotIndex)
{
	if (!HasValidInventoryItemAtIndex(SlotIndex)) return false;

	UItemDataAsset* ItemToConsume = InventorySlots[SlotIndex].ItemData;
	if (!ItemToConsume || ItemToConsume->ItemData.ItemType != EItemType::Consumable) return false;

	UEquipmentComponent* EquipmentComp = GetEquipmentComponent();
	if (!EquipmentComp)
	{
		return false;
	}

	// AAA: Cache name before potential slot clear to avoid fragile pointer access
	const FString ConsumedItemName = ItemToConsume->ItemData.ItemName.ToString();
	EquipmentComp->ApplyConsumableEffect(ItemToConsume);

	InventorySlots[SlotIndex].Quantity--;
	const int32 RemainingQuantity = InventorySlots[SlotIndex].Quantity;

	if (RemainingQuantity <= 0)
	{
		SetItemAtIndexInternal(nullptr, 0, SlotIndex, false);
	}

	BroadcastInventoryUpdated();

	UE_LOG(LogInventory, Log, TEXT("Consumed item: %s (Remaining: %d)"), *ConsumedItemName, RemainingQuantity);
	return true;
}

bool UInventoryComponent::Internal_ProcessEquipFlow(int32 Index, EEquipmentSlot TargetSlot)
{
	UItemDataAsset* ItemToEquip = InventorySlots[Index].ItemData;

	// AAA Orchestration Layer
	// Validation happened in CanEquipItem
	
	// Processing Method
	if (!ProcessEquipBackend(Index, TargetSlot, ItemToEquip))
	{
		return false;
	}

	// Execution Method
	ExecuteEquipVisuals(TargetSlot);
	return true;
}

bool UInventoryComponent::ProcessEquipBackend(int32 Index, EEquipmentSlot TargetSlot, UItemDataAsset* ItemToEquip)
{
	UEquipmentComponent* EquipmentComp = GetEquipmentComponent();
	if (!EquipmentComp)
	{
		return false;
	}

	if (!HandleTwoHandedEquipConstraints(ItemToEquip, TargetSlot))
	{
		return false;
	}

	if (HasItemEquippedAtSlot(TargetSlot))
	{
		if (!Internal_SwapEquippedWithInventory(Index, TargetSlot))
		{
			return false;
		}
	}
	else
	{
		EquipmentComp->EquipItem(ItemToEquip, TargetSlot);
		SetItemAtIndexInternal(nullptr, 0, Index, false);
	}

	return true;
}

bool UInventoryComponent::HandleTwoHandedEquipConstraints(UItemDataAsset* ItemToEquip, EEquipmentSlot TargetSlot)
{
	// Ensure two-handed and off-hand slots maintain mutual exclusivity
	if (TargetSlot == EEquipmentSlot::MainHand)
	{
		UWeaponDataAsset* WeaponData = Cast<UWeaponDataAsset>(ItemToEquip);
		if (WeaponData && WeaponData->WeaponData.bIsTwoHanded && HasItemEquippedAtSlot(EEquipmentSlot::OffHand))
		{
			return UnequipItem(EEquipmentSlot::OffHand);
		}
	}
	else if (TargetSlot == EEquipmentSlot::OffHand)
	{
		UWeaponDataAsset* MainHandWeapon = Cast<UWeaponDataAsset>(GetEquippedItem(EEquipmentSlot::MainHand));
		if (MainHandWeapon && MainHandWeapon->WeaponData.bIsTwoHanded)
		{
			return UnequipItem(EEquipmentSlot::MainHand);
		}
	}

	return true;
}

void UInventoryComponent::ExecuteEquipVisuals(EEquipmentSlot TargetSlot)
{
	if (HasDeferredMainHandSwap())
	{
		return;
	}

	BroadcastInventoryUpdated();
	
	if (UItemDataAsset* EquippedItem = GetEquippedItem(TargetSlot))
	{
		UE_LOG(LogInventory, Log, TEXT("Equipped item: %s to slot %d"), *EquippedItem->ItemData.ItemName.ToString(), (int32)TargetSlot);
	}
}

bool UInventoryComponent::Internal_SwapEquippedWithInventory(int32 Index, EEquipmentSlot TargetSlot)
{
	UItemDataAsset* ItemToHand = InventorySlots[Index].ItemData;
	if (!ItemToHand)
	{
		return false;
	}

	if (UEquipmentComponent* EquipmentComp = GetEquipmentComponent())
	{
		if (BeginDeferredMainHandSwap(Index, TargetSlot, ItemToHand))
		{
			return true;
		}

		UItemDataAsset* ItemToBag = EquipmentComp->UnequipItem(TargetSlot);
		if (!ItemToBag)
		{
			return false;
		}

		EquipmentComp->EquipItem(ItemToHand, TargetSlot);
		SetItemAtIndexInternal(ItemToBag, 1, Index, false); // Unstacking to bag
		return true;
	}

	return false;
}

bool UInventoryComponent::HasItemEquippedAtSlot(EEquipmentSlot Slot) const
{
	if (const UEquipmentComponent* EquipmentComp = GetEquipmentComponent())
	{
		return EquipmentComp->HasItemEquippedAtSlot(Slot);
	}

	return false;
}

void UInventoryComponent::ToggleDrawHolster()
{
	if (UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent())
	{
		WeaponActionComp->ToggleDrawHolster();
	}
}

bool UInventoryComponent::UnequipItem(EEquipmentSlot Slot)
{
	// -1 means find first available
	return Internal_ProcessUnequipFlow(Slot, INDEX_NONE);
}

bool UInventoryComponent::UnequipItemToSlot(EEquipmentSlot Slot, int32 TargetIndex)
{
	return Internal_ProcessUnequipFlow(Slot, TargetIndex);
}

bool UInventoryComponent::Internal_ProcessUnequipFlow(EEquipmentSlot Slot, int32 TargetIndex)
{
	UEquipmentComponent* EquipmentComp = GetEquipmentComponent();
	if (!EquipmentComp)
	{
		return false;
	}

	if (IsWeaponActionInProgress())
	{
		return false;
	}

	if (!HasItemEquippedAtSlot(Slot)) return false;

	UItemDataAsset* ItemToUnequip = GetEquippedItem(Slot);
	const int32 DestinationIndex = ResolveUnequipTargetIndex(ItemToUnequip, TargetIndex);
	if (DestinationIndex == INDEX_NONE)
	{
		UE_LOG(LogInventory, Warning, TEXT("Cannot unequip item from slot %d because there is no valid inventory destination."), (int32)Slot);
		return false;
	}

	UWeaponDataAsset* WeaponToUnequip = Cast<UWeaponDataAsset>(ItemToUnequip);
	const bool bRequiresWeaponTransition = WeaponToUnequip != nullptr && EquipmentComp->HasWeaponEquipped();
	if (bRequiresWeaponTransition)
	{
		UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent();
		if (!WeaponActionComp || !WeaponActionComp->BeginWeaponTransition(WeaponToUnequip, false))
		{
			return false;
		}
	}

	UItemDataAsset* RemovedItem = EquipmentComp->UnequipItem(Slot);
	if (!RemovedItem)
	{
		return false;
	}

	ItemToUnequip = RemovedItem;
	SetItemAtIndexInternal(ItemToUnequip, 1, DestinationIndex, false);

	if (WeaponToUnequip)
	{
		if (!bRequiresWeaponTransition)
		{
			ExecuteInstantClear();
		}
	}
	else
	{
		BroadcastInventoryUpdated();
	}

	return true;
}

void UInventoryComponent::ExecuteInstantClear()
{
	if (UEquipmentComponent* EquipmentComp = GetEquipmentComponent())
	{
		EquipmentComp->ClearWeaponMesh();
	}
	BroadcastInventoryUpdated();
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent())
	{
		if (WeaponTransitionCompletedDelegateHandle.IsValid())
		{
			WeaponActionComp->OnWeaponTransitionCompleted.Remove(WeaponTransitionCompletedDelegateHandle);
			WeaponTransitionCompletedDelegateHandle.Reset();
		}
	}

	ClearDeferredMainHandSwap();
	CachedWeaponAction.Reset();
	CachedEquipment.Reset();

	Super::EndPlay(EndPlayReason);
}

void UInventoryComponent::HandleWeaponTransitionCompleted(EWeaponEquipState CompletedState)
{
	if (CompletedState == EWeaponEquipState::Unequipping)
	{
		if (HasDeferredMainHandSwap())
		{
			FinalizeDeferredMainHandSwap();
			return;
		}

		FinalizeUnequipAction();
	}
}

void UInventoryComponent::FinalizeUnequipAction()
{
	// Assuming the map still doesn't have it since we removed it in Internal_ProcessUnequipFlow
	if (!HasItemEquippedAtSlot(EEquipmentSlot::MainHand))
	{
		if (UEquipmentComponent* EquipmentComp = GetEquipmentComponent())
		{
			EquipmentComp->ClearWeaponMesh();
		}
	}

	BroadcastInventoryUpdated();
}

bool UInventoryComponent::BeginDeferredMainHandSwap(int32 Index, EEquipmentSlot TargetSlot, UItemDataAsset* IncomingItem)
{
	if (TargetSlot != EEquipmentSlot::MainHand || !IncomingItem)
	{
		return false;
	}

	UEquipmentComponent* EquipmentComp = GetEquipmentComponent();
	UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent();
	UWeaponDataAsset* EquippedWeapon = EquipmentComp ? Cast<UWeaponDataAsset>(EquipmentComp->GetEquippedItem(TargetSlot)) : nullptr;
	if (!EquipmentComp || !WeaponActionComp || !EquippedWeapon || !EquipmentComp->HasWeaponEquipped())
	{
		return false;
	}

	if (!WeaponActionComp->BeginWeaponTransition(EquippedWeapon, false))
	{
		return false;
	}

	PendingMainHandSwapInventoryIndex = Index;
	PendingMainHandSwapSlot = TargetSlot;
	PendingMainHandSwapIncomingItem = IncomingItem;
	return true;
}

void UInventoryComponent::FinalizeDeferredMainHandSwap()
{
	if (!HasDeferredMainHandSwap())
	{
		return;
	}

	UEquipmentComponent* EquipmentComp = GetEquipmentComponent();
	UItemDataAsset* IncomingItem = PendingMainHandSwapIncomingItem;
	const int32 SourceIndex = PendingMainHandSwapInventoryIndex;
	const EEquipmentSlot TargetSlot = PendingMainHandSwapSlot;
	ClearDeferredMainHandSwap();

	if (!EquipmentComp || !IncomingItem || !InventorySlots.IsValidIndex(SourceIndex))
	{
		return;
	}

	UItemDataAsset* RemovedItem = EquipmentComp->UnequipItem(TargetSlot);
	if (!RemovedItem)
	{
		return;
	}

	EquipmentComp->EquipItem(IncomingItem, TargetSlot);
	SetItemAtIndexInternal(RemovedItem, 1, SourceIndex, false);
	ExecuteEquipVisuals(TargetSlot);
}

void UInventoryComponent::ClearDeferredMainHandSwap()
{
	PendingMainHandSwapIncomingItem = nullptr;
	PendingMainHandSwapInventoryIndex = INDEX_NONE;
	PendingMainHandSwapSlot = EEquipmentSlot::None;
}

bool UInventoryComponent::HasDeferredMainHandSwap() const
{
	return PendingMainHandSwapIncomingItem != nullptr
		&& PendingMainHandSwapInventoryIndex != INDEX_NONE
		&& PendingMainHandSwapSlot != EEquipmentSlot::None;
}

bool UInventoryComponent::CanEquipItem(int32 Index, EEquipmentSlot TargetSlot) const
{
	if (!OwnerCharacter || !HasValidInventoryItemAtIndex(Index)) return false;
	if (IsWeaponActionInProgress() || OwnerCharacter->IsInCombat()) return false;
	
	// AAA Backend Validation: Ensure item fits the destination slot
	return InventorySlots[Index].ItemData->ItemData.ValidEquipmentSlot == TargetSlot;
}

bool UInventoryComponent::SwapInventorySlots(int32 SourceIndex, int32 TargetIndex)
{
	if (!InventorySlots.IsValidIndex(SourceIndex) || !InventorySlots.IsValidIndex(TargetIndex) || SourceIndex == TargetIndex)
	{
		return false;
	}

	InventorySlots.Swap(SourceIndex, TargetIndex);
	BroadcastInventoryUpdated();
	return true;
}

UItemDataAsset* UInventoryComponent::GetItemAtIndex(int32 Index) const
{
	return InventorySlots.IsValidIndex(Index) ? InventorySlots[Index].ItemData : nullptr;
}

int32 UInventoryComponent::GetItemQuantityAtIndex(int32 Index) const
{
	return InventorySlots.IsValidIndex(Index) ? InventorySlots[Index].Quantity : 0;
}

UItemDataAsset* UInventoryComponent::GetEquippedItem(EEquipmentSlot Slot) const
{
	if (const UEquipmentComponent* EquipmentComp = GetEquipmentComponent())
	{
		return EquipmentComp->GetEquippedItem(Slot);
	}

	return nullptr;
}

void UInventoryComponent::SetItemAtIndexInternal(UItemDataAsset* Item, int32 Quantity, int32 Index, bool bBroadcast)
{
	if (!InventorySlots.IsValidIndex(Index))
	{
		return;
	}

	NormalizeSlotWrite(Item, Quantity);
	InventorySlots[Index] = FInventoryItem(Item, Quantity);

	if (bBroadcast)
	{
		BroadcastInventoryUpdated();
	}

	if (Item)
	{
		UE_LOG(LogInventory, Log, TEXT("Moved Item: %s to Slot: %d"), *Item->ItemData.ItemName.ToString(), Index);
	}
}

void UInventoryComponent::BroadcastInventoryUpdated()
{
	OnInventoryUpdated.Broadcast();
}

bool UInventoryComponent::HasValidInventoryItemAtIndex(int32 Index) const
{
	return InventorySlots.IsValidIndex(Index) && InventorySlots[Index].IsValid();
}

bool UInventoryComponent::IsWeaponActionInProgress() const
{
	if (const UWeaponActionComponent* WeaponActionComp = GetWeaponActionComponent())
	{
		return WeaponActionComp->IsWeaponActionInProgress();
	}

	return false;
}

int32 UInventoryComponent::ResolveUnequipTargetIndex(UItemDataAsset* ItemToUnequip, int32 PreferredIndex) const
{
	if (!ItemToUnequip)
	{
		return INDEX_NONE;
	}

	if (InventorySlots.IsValidIndex(PreferredIndex) && !InventorySlots[PreferredIndex].IsValid())
	{
		return PreferredIndex;
	}

	return FindEmptySlotIndex();
}

void UInventoryComponent::NormalizeSlotWrite(UItemDataAsset*& Item, int32& Quantity) const
{
	if (!Item || Quantity <= 0)
	{
		Item = nullptr;
		Quantity = 0;
		return;
	}

	Quantity = FMath::Clamp(Quantity, 1, Item->ItemData.MaxStackSize);
}

UEquipmentComponent* UInventoryComponent::GetEquipmentComponent() const
{
	if (!CachedEquipment.IsValid() && OwnerCharacter)
	{
		CachedEquipment = OwnerCharacter->FindComponentByClass<UEquipmentComponent>();
	}

	return CachedEquipment.Get();
}

UWeaponActionComponent* UInventoryComponent::GetWeaponActionComponent() const
{
	if (!CachedWeaponAction.IsValid() && OwnerCharacter)
	{
		CachedWeaponAction = OwnerCharacter->FindComponentByClass<UWeaponActionComponent>();
	}

	return CachedWeaponAction.Get();
}
