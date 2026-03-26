#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "DamageReceiverComponent.generated.h"

class UAbilitySystemComponent;
class AActor;
class AController;

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

private:
	void CacheDependencies();
	[[nodiscard]] bool CanProcessDamage() const;
	[[nodiscard]] float SanitizeRequestedDamage(float DamageAmount) const;
	[[nodiscard]] float ResolveAppliedDamage(float RequestedDamage) const;
	void ApplyHealthDelta(float AppliedDamage) const;

	UPROPERTY(Transient)
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};
