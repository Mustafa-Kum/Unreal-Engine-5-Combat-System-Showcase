#include "Characters/HeroInputComponent.h"
#include "Characters/HeroCharacter.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"

UHeroInputComponent::UHeroInputComponent()
{
	// Disable ticking entirely for the generic Input handler as it is event driven (AAA Optimization)
	PrimaryComponentTick.bCanEverTick = false;
}

void UHeroInputComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AHeroCharacter>(GetOwner());
}

void UHeroInputComponent::InitializeInput(UEnhancedInputComponent* EnhancedInputComponent, AHeroCharacter* InOwnerCharacter)
{
	if (!ShouldBindInput(EnhancedInputComponent, InOwnerCharacter)) return;
	
	OwnerCharacter = InOwnerCharacter;
	BoundInputComponent = EnhancedInputComponent;
	BindInputActions(EnhancedInputComponent);
}

void UHeroInputComponent::BindInputActions(UEnhancedInputComponent* EnhancedInputComponent)
{
	// AAA Clean Code: Generic lambda to encapsulate and eliminate repetitive pointer checks and bind calls (DRY Principle)
	auto BindActionIfValid = [&](UInputAction* Action, ETriggerEvent TriggerEvent, auto Func)
	{
		if (Action)
		{
			EnhancedInputComponent->BindAction(Action, TriggerEvent, this, Func);
		}
	};

	BindActionIfValid(MoveAction,         ETriggerEvent::Triggered, &UHeroInputComponent::OnMoveAction);
	BindActionIfValid(LookAction,         ETriggerEvent::Triggered, &UHeroInputComponent::OnLookAction);
	BindActionIfValid(RightClickAction,   ETriggerEvent::Started,   &UHeroInputComponent::OnRightClickStarted);
	BindActionIfValid(RightClickAction,   ETriggerEvent::Completed, &UHeroInputComponent::OnRightClickCompleted);
	BindActionIfValid(LeftClickAction,    ETriggerEvent::Started,   &UHeroInputComponent::OnLeftClickStarted);
	BindActionIfValid(LeftClickAction,    ETriggerEvent::Completed, &UHeroInputComponent::OnLeftClickCompleted);
	BindActionIfValid(ToggleWeaponAction, ETriggerEvent::Started,   &UHeroInputComponent::OnToggleWeapon);
	BindActionIfValid(PrimaryAbilityAction, ETriggerEvent::Started, &UHeroInputComponent::OnPrimaryAbilityAction);
	BindActionIfValid(ActionBarSlot2Action, ETriggerEvent::Started, &UHeroInputComponent::OnActionBarSlot2Action);
	BindActionIfValid(ActionBarSlot3Action, ETriggerEvent::Started, &UHeroInputComponent::OnActionBarSlot3Action);
	BindActionIfValid(ActionBarSlot4Action, ETriggerEvent::Started, &UHeroInputComponent::OnActionBarSlot4Action);
	BindActionIfValid(ActionBarSlot5Action, ETriggerEvent::Started, &UHeroInputComponent::OnActionBarSlot5Action);
	BindActionIfValid(ZoomAction,         ETriggerEvent::Triggered, &UHeroInputComponent::OnZoomAction);
	BindActionIfValid(ToggleWalkAction,   ETriggerEvent::Started,   &UHeroInputComponent::OnToggleWalk);
	BindActionIfValid(ToggleInventoryAction, ETriggerEvent::Started, &UHeroInputComponent::OnToggleInventory);
	BindActionIfValid(ToggleSkillBookAction, ETriggerEvent::Started, &UHeroInputComponent::OnToggleSkillBook);
}

// ==============================================================================
// AAA: Single Dispatch Guard — eliminates 10x repetitive null checks (DRY)
// All input handlers route through this single validation choke point.
// ==============================================================================

bool UHeroInputComponent::IsOwnerValid() const
{
	return OwnerCharacter != nullptr;
}

bool UHeroInputComponent::ShouldBindInput(UEnhancedInputComponent* EnhancedInputComponent, AHeroCharacter* InOwnerCharacter) const
{
	if (!EnhancedInputComponent || !InOwnerCharacter)
	{
		return false;
	}

	return BoundInputComponent.Get() != EnhancedInputComponent || OwnerCharacter != InOwnerCharacter;
}

// ==============================================================================
// Input Handlers - Routing commands to the Orchestrator Character (SRP & DIP)
// ==============================================================================

void UHeroInputComponent::OnMoveAction(const FInputActionValue& Value)
{
	if (IsOwnerValid()) OwnerCharacter->Move(Value);
}

void UHeroInputComponent::OnLookAction(const FInputActionValue& Value)
{
	if (IsOwnerValid()) OwnerCharacter->Look(Value);
}

void UHeroInputComponent::OnRightClickStarted()
{
	if (IsOwnerValid()) OwnerCharacter->HeavyAttackStarted();
}

void UHeroInputComponent::OnRightClickCompleted()
{
	if (IsOwnerValid()) OwnerCharacter->HeavyAttackCompleted();
}

void UHeroInputComponent::OnLeftClickStarted()
{
	if (IsOwnerValid()) OwnerCharacter->LightAttackStarted();
}

void UHeroInputComponent::OnLeftClickCompleted()
{
	if (IsOwnerValid()) OwnerCharacter->LightAttackCompleted();
}

void UHeroInputComponent::OnPrimaryAbilityAction()
{
	if (IsOwnerValid()) OwnerCharacter->ActivateActionBarSlot(0);
}

void UHeroInputComponent::OnActionBarSlot2Action()
{
	if (IsOwnerValid()) OwnerCharacter->ActivateActionBarSlot(1);
}

void UHeroInputComponent::OnActionBarSlot3Action()
{
	if (IsOwnerValid()) OwnerCharacter->ActivateActionBarSlot(2);
}

void UHeroInputComponent::OnActionBarSlot4Action()
{
	if (IsOwnerValid()) OwnerCharacter->ActivateActionBarSlot(3);
}

void UHeroInputComponent::OnActionBarSlot5Action()
{
	if (IsOwnerValid()) OwnerCharacter->ActivateActionBarSlot(4);
}

void UHeroInputComponent::OnZoomAction(const FInputActionValue& Value)
{
	if (IsOwnerValid()) OwnerCharacter->Zoom(Value);
}

void UHeroInputComponent::OnToggleWeapon()
{
	if (IsOwnerValid()) OwnerCharacter->ToggleWeapon();
}

void UHeroInputComponent::OnToggleWalk()
{
	if (IsOwnerValid()) OwnerCharacter->ToggleWalk();
}

void UHeroInputComponent::OnToggleInventory()
{
	if (IsOwnerValid()) OwnerCharacter->ToggleInventory();
}

void UHeroInputComponent::OnToggleSkillBook()
{
	if (IsOwnerValid()) OwnerCharacter->ToggleSkillBook();
}
