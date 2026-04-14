#pragma once

#include "CoreMinimal.h"
#include "Characters/CharacterBase.h"
#include "InputActionValue.h"
#include "HeroCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryToggled);

class UCombatComponent;
class UAbilityLoadoutComponent;
class UWeaponActionComponent;
class UUserWidget;

UCLASS()
class WOWCLONE_API AHeroCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	AHeroCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/** --- CORE QUERIES (SOLID) --- */
	[[nodiscard]] virtual float GetBackwardMovementSpeedMultiplier() const override;
	[[nodiscard]] virtual bool IsInCombat() const override;
	[[nodiscard]] class UInventoryComponent* GetInventoryComponent() const { return InventoryComp; }
	[[nodiscard]] UWeaponActionComponent* GetWeaponActionComponent() const { return WeaponActionComp; }
	[[nodiscard]] UAbilityLoadoutComponent* GetAbilityLoadoutComponent() const { return AbilityLoadoutComp; }
	[[nodiscard]] TSubclassOf<class UInventoryWidget> GetInventoryWidgetClass() const { return InventoryWidgetClass; }
	[[nodiscard]] TSubclassOf<class UPlayerVitalsWidget> GetPlayerVitalsWidgetClass() const { return PlayerVitalsWidgetClass; }
	[[nodiscard]] TSubclassOf<class UActionBarWidget> GetActionBarWidgetClass() const { return ActionBarWidgetClass; }
	[[nodiscard]] TSubclassOf<class USkillBookWidget> GetSkillBookWidgetClass() const { return SkillBookWidgetClass; }
	[[nodiscard]] TSubclassOf<class UAbilityFailureWidget> GetAbilityFailureWidgetClass() const { return AbilityFailureWidgetClass; }
	[[nodiscard]] TSubclassOf<UUserWidget> GetCastBarWidgetClass() const { return CastBarWidgetClass; }

	/** --- INPUT ORCHESTRATORS (Enhanced Input) --- */
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void HeavyAttackStarted();
	void HeavyAttackCompleted();
	void LightAttackStarted();
	void LightAttackCompleted();
	void ActivatePrimaryAbility();
	void ActivateActionBarSlot(int32 SlotIndex);
	void Zoom(const FInputActionValue& Value);
	void ToggleWeapon();
	void ToggleWalk();
	void ToggleInventory();
	void ToggleSkillBook();

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWeaponActionComponent> WeaponActionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCombatComponent> CombatComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UAbilityLoadoutComponent> AbilityLoadoutComp;

	/** --- UI CONFIGURATION --- */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UPlayerVitalsWidget> PlayerVitalsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UActionBarWidget> ActionBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class USkillBookWidget> SkillBookWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<class UAbilityFailureWidget> AbilityFailureWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> CastBarWidgetClass;

	/** --- STATE DATA --- */
	uint8 bIsHeavyAttackPressed : 1;
	uint8 bIsLightAttackPressed : 1;

	/** --- ORCHESTRATOR HELPERS (Validation -> Processing -> Execution) --- */
	
	// Initialization
	void InitializeComponents();
	void InitializeInputModule(class UInputComponent* PlayerInputComponent);
	void InitializePlayerInputState();
	void InitializeHUD();

	// Rotation & Camera
	void HandleLookInput(const FVector2D& LookAxisVector);
	void SetCharacterYawDecoupled(bool bIsDecoupled);
	void ConfigureRotationSettings(bool bIsDecoupled);
	[[nodiscard]] bool IsLocalPlayerControlled() const;

	// DRY: Shared PlayerController accessor (eliminates repeated casts)
	[[nodiscard]] APlayerController* GetPlayerController() const;

	// Locomotion Predicates
	[[nodiscard]] bool CanMove() const;
	[[nodiscard]] bool CanToggleWalk() const;
	[[nodiscard]] FVector GetMovementDirection(EAxis::Type Axis) const;
	void HandleWalkSpeedToggle();

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnInventoryToggled OnInventoryToggled;
};
