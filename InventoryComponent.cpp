#include "Components/InventoryComponent.h"
#include "Characters/CharacterBase.h"
#include "DataAssets/ItemDataAsset.h"
#include "DataAssets/WeaponDataAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

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

	OwnerCharacter->OnWeaponActionFinished.AddUObject(this, &UInventoryComponent::OnWeaponActionCompleted);

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

	// AAA: Cache name before potential slot clear to avoid fragile pointer access
	const FString ConsumedItemName = ItemToConsume->ItemData.ItemName.ToString();

	if (OwnerCharacter)
	{
		OwnerCharacter->ApplyConsumableEffect(ItemToConsume);
	}

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
	if (!HandleTwoHandedEquipConstraints(ItemToEquip, TargetSlot))
	{
		return false;
	}

	if (HasItemEquippedAtSlot(TargetSlot))
	{
		Internal_SwapEquippedWithInventory(Index, TargetSlot);
	}
	else
	{
		EquippedItems.Add(TargetSlot, ItemToEquip);
		SetItemAtIndexInternal(nullptr, 0, Index, false);
		MountItemStats(ItemToEquip);
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
	if (UWeaponDataAsset* WeaponData = Cast<UWeaponDataAsset>(GetEquippedItem(TargetSlot)))
	{
		OwnerCharacter->SetPendingWeapon(WeaponData);
	}
	
	BroadcastInventoryUpdated();
	
	if (UItemDataAsset* EquippedItem = GetEquippedItem(TargetSlot))
	{
		UE_LOG(LogInventory, Log, TEXT("Equipped item: %s to slot %d"), *EquippedItem->ItemData.ItemName.ToString(), (int32)TargetSlot);
	}
}

void UInventoryComponent::Internal_SwapEquippedWithInventory(int32 Index, EEquipmentSlot TargetSlot)
{
	UItemDataAsset* ItemToBag = GetEquippedItem(TargetSlot);
	UItemDataAsset* ItemToHand = InventorySlots[Index].ItemData;

	DismountItemStats(ItemToBag);
	
	SetItemAtIndexInternal(ItemToBag, 1, Index, false); // Unstacking to bag
	EquippedItems.Add(TargetSlot, ItemToHand);

	MountItemStats(ItemToHand);
}

bool UInventoryComponent::HasItemEquippedAtSlot(EEquipmentSlot Slot) const
{
	return EquippedItems.Contains(Slot) && EquippedItems[Slot] != nullptr;
}

void UInventoryComponent::ToggleDrawHolster()
{
	if (!CanPerformWeaponAction()) return;

	UWeaponDataAsset* MainHandWeapon = Cast<UWeaponDataAsset>(GetEquippedItem(EEquipmentSlot::MainHand));
	if (!MainHandWeapon) return;

	const bool bIsDrawing = !OwnerCharacter->HasWeaponEquipped();
	Internal_HandleWeaponTransition(MainHandWeapon, bIsDrawing);
}

void UInventoryComponent::Internal_HandleWeaponTransition(UWeaponDataAsset* WeaponToTransition, bool bIsEquipping)
{
	const EWeaponEquipState RequestedState = bIsEquipping ? EWeaponEquipState::Equipping : EWeaponEquipState::Unequipping;
	SetEquipState(RequestedState);
	PendingWeaponTransitionData = WeaponToTransition;
	PendingWeaponTransitionState = RequestedState;
	++PendingWeaponTransitionRequestId;
	
	if (bIsEquipping)
	{
		OwnerCharacter->SetPendingWeapon(WeaponToTransition);
	}

	ExecuteVisualTransition(WeaponToTransition, bIsEquipping);
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
	if (!HasItemEquippedAtSlot(Slot)) return false;

	UItemDataAsset* ItemToUnequip = GetEquippedItem(Slot);
	const int32 DestinationIndex = ResolveUnequipTargetIndex(ItemToUnequip, TargetIndex);
	if (DestinationIndex == INDEX_NONE)
	{
		UE_LOG(LogInventory, Warning, TEXT("Cannot unequip item from slot %d because there is no valid inventory destination."), (int32)Slot);
		return false;
	}

	DismountItemStats(ItemToUnequip);
	
	EquippedItems.Remove(Slot);
	SetItemAtIndexInternal(ItemToUnequip, 1, DestinationIndex, false);

	if (UWeaponDataAsset* WeaponToUnequip = Cast<UWeaponDataAsset>(ItemToUnequip))
	{
		if (OwnerCharacter->HasWeaponEquipped())
		{
			Internal_HandleWeaponTransition(WeaponToUnequip, false);
		}
		else
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

void UInventoryComponent::MountItemStats(UItemDataAsset* Item)
{
	if (OwnerCharacter && Item)
	{
		OwnerCharacter->ApplyItemStats(Item);
	}
}

void UInventoryComponent::DismountItemStats(UItemDataAsset* Item)
{
	if (OwnerCharacter && Item)
	{
		OwnerCharacter->RemoveItemStats(Item);
	}
}

void UInventoryComponent::ExecuteVisualTransition(UWeaponDataAsset* WeaponData, bool bIsEquipping)
{
	const TSoftObjectPtr<UAnimMontage>& Montage = bIsEquipping ? WeaponData->WeaponData.EquipMontage : WeaponData->WeaponData.UnequipMontage;
	ProcessMontageLoad(WeaponData, Montage, bIsEquipping);
}

void UInventoryComponent::ExecuteInstantClear()
{
	if (OwnerCharacter)
	{
		OwnerCharacter->ClearWeaponMesh();
	}
	BroadcastInventoryUpdated();
}

void UInventoryComponent::OnEquipMontageLoaded(UWeaponDataAsset* WeaponToEquip, int32 RequestId)
{
	PlayWeaponMontage(WeaponToEquip, EWeaponEquipState::Equipping, RequestId);
}

void UInventoryComponent::OnUnequipMontageLoaded(UWeaponDataAsset* WeaponToUnequip, int32 RequestId)
{
	PlayWeaponMontage(WeaponToUnequip, EWeaponEquipState::Unequipping, RequestId);
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OwnerCharacter)
	{
		OwnerCharacter->OnWeaponActionFinished.RemoveAll(this);
	}
	ClearPendingWeaponTransition();
	Super::EndPlay(EndPlayReason);
}

void UInventoryComponent::OnWeaponActionCompleted()
{
	const EWeaponEquipState PreviousState = WeaponEquipState;

	if (PreviousState == EWeaponEquipState::Unequipping)
	{
		FinalizeUnequipAction();
	}

	SetEquipState(EWeaponEquipState::Idle);
	ClearPendingWeaponTransition();
}

void UInventoryComponent::FinalizeUnequipAction()
{
	// Assuming the map still doesn't have it since we removed it in Internal_ProcessUnequipFlow
	if (!HasItemEquippedAtSlot(EEquipmentSlot::MainHand) && OwnerCharacter)
	{
		OwnerCharacter->ClearWeaponMesh();
	}

	BroadcastInventoryUpdated();
}

void UInventoryComponent::SetEquipState(EWeaponEquipState NewState)
{
	WeaponEquipState = NewState;
}

void UInventoryComponent::PlayWeaponMontage(UWeaponDataAsset* WeaponData, EWeaponEquipState ExpectedState, int32 RequestId)
{
	if (!IsWeaponTransitionRequestCurrent(WeaponData, ExpectedState, RequestId))
	{
		return;
	}

	const TSoftObjectPtr<UAnimMontage>& Montage = (ExpectedState == EWeaponEquipState::Equipping)
		? WeaponData->WeaponData.EquipMontage
		: WeaponData->WeaponData.UnequipMontage;

	if (!OwnerCharacter || !Montage.IsValid())
	{
		SnapWeaponWithoutAnimation(ExpectedState == EWeaponEquipState::Equipping);
		return;
	}

	if (OwnerCharacter->PlayAnimMontage(Montage.Get()) <= 0.0f)
	{
		UE_LOG(LogInventory, Warning, TEXT("Failed to play %s montage for weapon %s. Falling back to notify-free transition."),
			ExpectedState == EWeaponEquipState::Equipping ? TEXT("equip") : TEXT("unequip"),
			*GetNameSafe(WeaponData));
		SnapWeaponWithoutAnimation(ExpectedState == EWeaponEquipState::Equipping);
	}
}

bool UInventoryComponent::CanEquipItem(int32 Index, EEquipmentSlot TargetSlot) const
{
	if (!OwnerCharacter || !HasValidInventoryItemAtIndex(Index)) return false;
	if (IsWeaponActionInProgress() || OwnerCharacter->IsInCombat()) return false;
	
	// AAA Backend Validation: Ensure item fits the destination slot
	return InventorySlots[Index].ItemData->ItemData.ValidEquipmentSlot == TargetSlot;
}

bool UInventoryComponent::CanPerformWeaponAction() const
{
	return OwnerCharacter && HasItemEquippedAtSlot(EEquipmentSlot::MainHand) && !IsWeaponActionInProgress() && !OwnerCharacter->IsInCombat();
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

bool UInventoryComponent::PrepareWeaponForCombat()
{
	if (!OwnerCharacter)
	{
		return false;
	}

	if (!EnsureCombatWeaponEquipped())
	{
		return false;
	}

	return EnsureCombatWeaponDrawn();
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
	return EquippedItems.FindRef(Slot);
}

int32 UInventoryComponent::FindFirstEquippableItemIndex(EEquipmentSlot TargetSlot) const
{
	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		if (const UItemDataAsset* ItemData = GetItemAtIndex(Index))
		{
			if (ItemData->ItemData.ValidEquipmentSlot == TargetSlot)
			{
				return Index;
			}
		}
	}

	return INDEX_NONE;
}

void UInventoryComponent::ProcessMontageLoad(UWeaponDataAsset* WeaponData, const TSoftObjectPtr<UAnimMontage>& MontageToLoad, bool bIsEquipping)
{
	if (MontageToLoad.IsPending()) HandlePendingMontage(WeaponData, MontageToLoad, bIsEquipping);
	else if (MontageToLoad.IsValid()) HandleValidMontage(WeaponData, bIsEquipping);
	else HandleMissingMontage(bIsEquipping);
}

void UInventoryComponent::HandlePendingMontage(UWeaponDataAsset* WeaponData, const TSoftObjectPtr<UAnimMontage>& MontageToLoad, bool bIsEquipping)
{
	const int32 RequestId = PendingWeaponTransitionRequestId;
	FStreamableDelegate Delegate = bIsEquipping 
		? FStreamableDelegate::CreateUObject(this, &UInventoryComponent::OnEquipMontageLoaded, WeaponData, RequestId)
		: FStreamableDelegate::CreateUObject(this, &UInventoryComponent::OnUnequipMontageLoaded, WeaponData, RequestId);
		
	UAssetManager::GetStreamableManager().RequestAsyncLoad(MontageToLoad.ToSoftObjectPath(), Delegate);
}

void UInventoryComponent::HandleValidMontage(UWeaponDataAsset* WeaponData, bool bIsEquipping)
{
	const int32 RequestId = PendingWeaponTransitionRequestId;
	bIsEquipping ? OnEquipMontageLoaded(WeaponData, RequestId) : OnUnequipMontageLoaded(WeaponData, RequestId);
}

void UInventoryComponent::HandleMissingMontage(bool bIsEquipping)
{
	SnapWeaponWithoutAnimation(bIsEquipping);
}

void UInventoryComponent::SnapWeaponWithoutAnimation(bool bIsEquipping)
{
	if (!OwnerCharacter) return;

	bIsEquipping ? OwnerCharacter->OnWeaponEquipNotify() : OwnerCharacter->OnWeaponUnequipNotify();
}

bool UInventoryComponent::IsWeaponTransitionRequestCurrent(UWeaponDataAsset* WeaponData, EWeaponEquipState ExpectedState, int32 RequestId) const
{
	return OwnerCharacter
		&& WeaponData
		&& WeaponEquipState == ExpectedState
		&& PendingWeaponTransitionState == ExpectedState
		&& PendingWeaponTransitionData == WeaponData
		&& PendingWeaponTransitionRequestId == RequestId;
}

void UInventoryComponent::ClearPendingWeaponTransition()
{
	PendingWeaponTransitionData = nullptr;
	PendingWeaponTransitionState = EWeaponEquipState::Idle;
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

bool UInventoryComponent::EnsureCombatWeaponEquipped()
{
	if (HasItemEquippedAtSlot(EEquipmentSlot::MainHand))
	{
		return true;
	}

	const int32 WeaponIndex = FindFirstEquippableItemIndex(EEquipmentSlot::MainHand);
	return WeaponIndex != INDEX_NONE && EquipItemAtIndex(WeaponIndex, EEquipmentSlot::MainHand);
}

bool UInventoryComponent::EnsureCombatWeaponDrawn()
{
	if (OwnerCharacter->HasWeaponEquipped())
	{
		return true;
	}

	if (!CanPerformWeaponAction())
	{
		return false;
	}

	ToggleDrawHolster();
	return true;
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
