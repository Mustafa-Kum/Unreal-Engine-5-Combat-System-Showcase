#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "HeroInputComponent.generated.h"

class UInputAction;
class UEnhancedInputComponent;
class AHeroCharacter;

UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class WOWCLONE_API UHeroInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeroInputComponent();

	// Initializes the input component and binds standard character actions.
	void InitializeInput(UEnhancedInputComponent* EnhancedInputComponent, AHeroCharacter* InOwnerCharacter);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RightClickAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LeftClickAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleWeaponAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "Action Bar Slot 1 Action"))
	TObjectPtr<UInputAction> PrimaryAbilityAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "Action Bar Slot 2 Action"))
	TObjectPtr<UInputAction> ActionBarSlot2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "Action Bar Slot 3 Action"))
	TObjectPtr<UInputAction> ActionBarSlot3Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "Action Bar Slot 4 Action"))
	TObjectPtr<UInputAction> ActionBarSlot4Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "Action Bar Slot 5 Action"))
	TObjectPtr<UInputAction> ActionBarSlot5Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleWalkAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleSkillBookAction;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AHeroCharacter> OwnerCharacter;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	void BindInputActions(UEnhancedInputComponent* EnhancedInputComponent);
	[[nodiscard]] bool ShouldBindInput(UEnhancedInputComponent* EnhancedInputComponent, AHeroCharacter* InOwnerCharacter) const;
	[[nodiscard]] bool IsOwnerValid() const;

	void OnMoveAction(const FInputActionValue& Value);
	void OnLookAction(const FInputActionValue& Value);
	void OnRightClickStarted();
	void OnRightClickCompleted();
	void OnLeftClickStarted();
	void OnLeftClickCompleted();
	void OnPrimaryAbilityAction();
	void OnActionBarSlot2Action();
	void OnActionBarSlot3Action();
	void OnActionBarSlot4Action();
	void OnActionBarSlot5Action();
	void OnZoomAction(const FInputActionValue& Value);
	void OnToggleWeapon();
	void OnToggleWalk();
	void OnToggleInventory();
	void OnToggleSkillBook();
};
