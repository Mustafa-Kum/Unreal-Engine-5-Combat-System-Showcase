#pragma once

#include "CoreMinimal.h"
#include "Characters/CharacterBase.h"
#include "InputActionValue.h"
#include "HeroCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryToggled);

class UCombatComponent;

UCLASS()
class WOWCLONE_API AHeroCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	AHeroCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/** --- CORE QUERIES (SOLID) --- */
	[[nodiscard]] virtual float GetBackwardMovementSpeedMultiplier() const override;
	[[nodiscard]] virtual bool IsInCombat() const override;

	/** --- INPUT ORCHESTRATORS (Enhanced Input) --- */
	virtual void Move(const FInputActionValue& Value) override;
	virtual void Look(const FInputActionValue& Value) override;
	virtual void RightClickStarted() override;
	virtual void RightClickCompleted() override;
	virtual void LeftClickStarted() override;
	virtual void LeftClickCompleted() override;
	virtual void Zoom(const FInputActionValue& Value) override;
	virtual void ToggleWeapon() override;
	virtual void ToggleWalk() override;
	virtual void ToggleCombat() override;
	virtual void ToggleInventory() override;
	virtual void TestVitals() override;

private:
	/** --- COMPONENTS (AAA Layout) --- */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UHeroInputComponent> HeroInputComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UHeroCameraComponent> HeroCameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UHeroLocomotionComponent> HeroLocomotionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInventoryComponent> InventoryComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCombatComponent> CombatComp;

	/** --- UI CONFIGURATION --- */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<class UInventoryWidget> InventoryWidgetInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UPlayerVitalsWidget> PlayerVitalsWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<class UPlayerVitalsWidget> PlayerVitalsWidgetInstance;

	/** --- STATE DATA --- */
	uint8 bIsRightClickPressed : 1;
	uint8 bIsLeftClickPressed : 1;
	uint8 bIsInCombat : 1;

	/** --- ORCHESTRATOR HELPERS (Validation -> Processing -> Execution) --- */
	
	// Initialization
	void InitializeComponents();
	void InitializeCombatState();
	void InitializeInputModule(class UInputComponent* PlayerInputComponent);
	void InitializePlayerInputState();
	void InitializeHUD();

	// Rotation & Camera
	void HandleLookInput(const FVector2D& LookAxisVector);
	void SetCharacterYawDecoupled(bool bIsDecoupled);
	void ConfigureRotationSettings(bool bIsDecoupled);
	void SetMouseCursorVisibility(bool bIsVisible);
	[[nodiscard]] bool IsLocalPlayerControlled() const;

	// DRY: Shared PlayerController accessor (eliminates repeated casts)
	[[nodiscard]] APlayerController* GetPlayerController() const;

	// Combat Logic
	void ToggleCombatState(UAbilitySystemComponent* ASC);
	void EnterCombatState(UAbilitySystemComponent* ASC);
	void ExitCombatState(UAbilitySystemComponent* ASC);
	void HandleCombatTagChange(UAbilitySystemComponent* ASC, bool bEnteringCombat);
	void ApplyCombatStateMovementOverrides();
	void RevertCombatStateMovementOverrides();
	[[nodiscard]] bool PrepareCombatLoadout();

	// Locomotion Predicates
	[[nodiscard]] bool CanMove() const;
	[[nodiscard]] bool CanToggleWalk() const;
	[[nodiscard]] FVector GetMovementDirection(EAxis::Type Axis) const;
	void HandleWalkSpeedToggle();

	// Inventory UI Flow
	void EnsureInventoryWidgetCreated();
	void ToggleInventoryVisibility();
	void SetInputModeForInventory(bool bInventoryOpen);

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnInventoryToggled OnInventoryToggled;
};
