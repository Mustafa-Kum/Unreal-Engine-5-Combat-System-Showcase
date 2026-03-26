#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "HeroInputComponent.generated.h"

class UInputAction;
class UEnhancedInputComponent;
class ACharacterBase;

UCLASS( ClassGroup=(Input), meta=(BlueprintSpawnableComponent) )
class WOWCLONE_API UHeroInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHeroInputComponent();

	// Initializes the input component and binds standard character actions
	void InitializeInput(UEnhancedInputComponent* EnhancedInputComponent, ACharacterBase* InOwnerCharacter);

	/** --- ENHANCED INPUT YUVALARI --- */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RightClickAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LeftClickAction;

	// Mouse Wheel Input for Zoom (1D Axis)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;
	
	// E veya Z tuşu ile Silahı Kuşanma/Çıkarma (Toggle Weapon)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleWeaponAction;

	// Num/ veya belirlediğiniz tuş ile Yürüme/Koşma geçişi (Toggle Walk)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleWalkAction;

	// J tuşu ile Combat durumuna girip çıkma (Toggle Combat)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleCombatAction;

	// C tuşu ile Envanteri açıp kapatma (Toggle Inventory)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	// UI Smoothness Test (T tuşu vs.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> TestVitalsAction;

protected:
	virtual void BeginPlay() override;

private:
	// Owner Reference
	UPROPERTY(Transient)
	TObjectPtr<ACharacterBase> OwnerCharacter;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	// Internal binding method
	void BindInputActions(UEnhancedInputComponent* EnhancedInputComponent);
	[[nodiscard]] bool ShouldBindInput(UEnhancedInputComponent* EnhancedInputComponent, ACharacterBase* InOwnerCharacter) const;

	// AAA DRY: Single validation choke point — eliminates 10x repetitive null checks across all handlers
	[[nodiscard]] bool IsOwnerValid() const;

	// Action Delegates (Routed back to Character)
	void OnMoveAction(const FInputActionValue& Value);
	void OnLookAction(const FInputActionValue& Value);
	void OnRightClickStarted();
	void OnRightClickCompleted();
	void OnLeftClickStarted();
	void OnLeftClickCompleted();
	void OnZoomAction(const FInputActionValue& Value);
	void OnToggleWeapon();
	void OnToggleWalk();
	void OnToggleCombat();
	void OnToggleInventory();
	void OnTestVitals();
};
