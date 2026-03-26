// Fill out your copyright notice in the Description page of Project Settings.

#include "DataAssets/WeaponDataAsset.h"

UWeaponDataAsset::UWeaponDataAsset()
{
	// AAA Defaults: Setup base item properties specifically for weapons
	ItemData.ItemType = EItemType::Weapon;
	ItemData.ValidEquipmentSlot = EEquipmentSlot::MainHand;
	ItemData.MaxStackSize = 1; // Weapons generally don't stack
}
