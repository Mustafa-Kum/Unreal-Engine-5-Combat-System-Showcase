#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "DamageReceiverComponent.generated.h"

class UAbilitySystemComponent;
class AActor;
class AController;
class UGameplayEffect;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WOWCLONE_API UDamageReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageReceiverComponent();

protected:
	virtual void BeginPlay() override;

public:
	[[nodiscard]] float ReceiveDamage(float DamageAmount, const FDamageEvent& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser);
	void SetRestoreHealthToMaxOnDefeat(bool bInRestoreHealthToMaxOnDefeat) { bRestoreHealthToMaxOnDefeat = bInRestoreHealthToMaxOnDefeat; }

private:
	void CacheDependencies();
	[[nodiscard]] bool CanProcessDamage() const;
	[[nodiscard]] float SanitizeRequestedDamage(float DamageAmount) const;
	[[nodiscard]] float ResolveAppliedDamage(float RequestedDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const;
	[[nodiscard]] float ApplyMitigation(float RequestedDamage, const FDamageEvent& DamageEvent) const;
	[[nodiscard]] float ApplySourceContextMultiplier(float DamageAmount, AController* EventInstigator, AActor* DamageCauser) const;
	[[nodiscard]] float ApplyDamageTypeMitigation(float DamageAmount, EWoWCloneDamageType DamageType) const;
	[[nodiscard]] bool IsPlayerControlledSource(AController* EventInstigator, AActor* DamageCauser) const;
	[[nodiscard]] bool IsPlayerControlledTarget() const;
	void ApplyHealthDelta(float AppliedDamage, AController* EventInstigator, AActor* DamageCauser) const;
	void RestoreHealthAfterDefeat() const;
	[[nodiscard]] UGameplayEffect* GetOrCreateReusableDamageEffect() const;

	UPROPERTY(Transient)
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	mutable TObjectPtr<UGameplayEffect> ReusableDamageEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Mitigation", meta = (ClampMin = "0.0"))
	float PhysicalMitigationScale = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Mitigation", meta = (ClampMin = "0.0"))
	float MagicalMitigationScale = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Context", meta = (ClampMin = "0.0"))
	float PlayerVsPlayerDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Context", meta = (ClampMin = "0.0"))
	float PlayerVsEnvironmentDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Context", meta = (ClampMin = "0.0"))
	float EnvironmentVsPlayerDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Context", meta = (ClampMin = "0.0"))
	float EnvironmentVsEnvironmentDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Context", meta = (ClampMin = "0.0"))
	float AreaDamageTakenMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Damage|Lifecycle")
	bool bRestoreHealthToMaxOnDefeat = false;
};
