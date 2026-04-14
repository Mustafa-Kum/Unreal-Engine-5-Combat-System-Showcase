#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "CharacterBase.generated.h"

enum class EWeaponType : uint8;
class UCombatFeedbackComponent;
class UCombatImpactComponent;
class UDamageReceiverComponent;
class UEquipmentComponent;
class UResourceComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWeaponActionFinished, const class UAnimMontage* /*CompletedMontage*/, bool /*bCompletedEquipAction*/);

UCLASS(Abstract, Blueprintable)
class WOWCLONE_API ACharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACharacterBase();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	[[nodiscard]] virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	float DealDamageToActor(AActor* TargetActor, float DamageAmount, const struct FDamageEvent& DamageEvent);

	[[nodiscard]] virtual float GetBackwardMovementSpeedMultiplier() const { return 1.0f; }
	[[nodiscard]] virtual bool IsInCombat() const { return false; }
	[[nodiscard]] virtual EWeaponType GetEquippedWeaponType() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	[[nodiscard]] bool HasWeaponEquipped() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void OnWeaponEquipNotify();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void OnWeaponUnequipNotify();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ClearWeaponMesh();

	void RequestAreaIndicatorPreview(UObject* RequestOwner, float Radius) const;
	void ClearAreaIndicatorPreview(UObject* RequestOwner) const;

protected:
	void ToggleGameplayTagPair(class UAbilitySystemComponent* ASC, const FGameplayTag& TagToRemove, const FGameplayTag& TagToAdd);

private:
	friend class UEquipmentComponent;

	void ApplyStartingStats(const struct FCharacterStartingStats& Stats);
	void InitializeCharacterStats();
	void GrantStartupAbilities();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCharacterAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Setup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCharacterDataAsset> CharacterClassData;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStaticMeshComponent> WeaponMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEquipmentComponent> EquipmentComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatImpactComponent> CombatImpactComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatFeedbackComponent> CombatFeedbackComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDamageReceiverComponent> DamageReceiverComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resources", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UResourceComponent> ResourceComp;

public:
	FOnWeaponActionFinished OnWeaponActionFinished;
};
