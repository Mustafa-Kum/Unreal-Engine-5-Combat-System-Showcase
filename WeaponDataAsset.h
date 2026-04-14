#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "DataAssets/ItemDataAsset.h"
#include "WeaponDataAsset.generated.h"

class UCameraShakeBase;

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	None		UMETA(DisplayName = "None"),
	Sword		UMETA(DisplayName = "Sword"),
	Axe			UMETA(DisplayName = "Axe"),
	Mace		UMETA(DisplayName = "Mace"),
	Staff		UMETA(DisplayName = "Staff"),
	Dagger		UMETA(DisplayName = "Dagger")
};

USTRUCT(BlueprintType)
struct FCombatComboData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TArray<TSoftObjectPtr<class UAnimMontage>> ComboMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	TArray<FMeleeKnockbackConfig> ComboStepKnockbackConfigs;

	/** Window after the last attack where the combo resets if no input is received */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float ComboResetDelay = 1.5f;
};

USTRUCT(BlueprintType)
struct FWeaponData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	EWeaponType WeaponType = EWeaponType::Sword;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Info")
	bool bIsTwoHanded = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<class UAnimMontage> EquipMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<class UAnimMontage> UnequipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Socket")
	FName EquipSocketName = FName("WeaponSocket_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Socket")
	FName HolsterSocketName = FName("HolsterSocket_Back");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<class UStaticMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0", DisplayName = "Fallback Attack Damage"))
	float BaseDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats", meta = (ClampMin = "0.0", DisplayName = "Fallback Cast Speed"))
	float WeaponCastSpeed = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (DisplayName = "Light Attack Combo Data"))
	FCombatComboData ComboData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (DisplayName = "Heavy Attack Combo Data"))
	FCombatComboData HeavyAttackComboData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Stop")
	bool bEnableHitStop = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Stop", meta = (ClampMin = "0.0"))
	float HitStopDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Stop", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float HitStopTimeDilation = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Feedback")
	TSubclassOf<UCameraShakeBase> HitCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Feedback", meta = (ClampMin = "0.0"))
	float HitCameraShakeScale = 1.0f;
};

UCLASS(BlueprintType)
class WOWCLONE_API UWeaponDataAsset : public UItemDataAsset
{
	GENERATED_BODY()
	
public:
	UWeaponDataAsset();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Data", meta=(ShowOnlyInnerProperties))
	FWeaponData WeaponData;
};
