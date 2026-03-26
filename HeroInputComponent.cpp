#include "Characters/HeroInputComponent.h"
#include "Characters/CharacterBase.h"
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
	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
}

void UHeroInputComponent::InitializeInput(UEnhancedInputComponent* EnhancedInputComponent, ACharacterBase* InOwnerCharacter)
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
	BindActionIfValid(ZoomAction,         ETriggerEvent::Triggered, &UHeroInputComponent::OnZoomAction);
	BindActionIfValid(ToggleWeaponAction, ETriggerEvent::Started,   &UHeroInputComponent::OnToggleWeapon);
	BindActionIfValid(ToggleWalkAction,   ETriggerEvent::Started,   &UHeroInputComponent::OnToggleWalk);
	BindActionIfValid(ToggleCombatAction, ETriggerEvent::Started,   &UHeroInputComponent::OnToggleCombat);
	BindActionIfValid(ToggleInventoryAction, ETriggerEvent::Started, &UHeroInputComponent::OnToggleInventory);
	BindActionIfValid(TestVitalsAction, ETriggerEvent::Started,     &UHeroInputComponent::OnTestVitals);
}

// ==============================================================================
// AAA: Single Dispatch Guard — eliminates 10x repetitive null checks (DRY)
// All input handlers route through this single validation choke point.
// ==============================================================================

bool UHeroInputComponent::IsOwnerValid() const
{
	return OwnerCharacter != nullptr;
}

bool UHeroInputComponent::ShouldBindInput(UEnhancedInputComponent* EnhancedInputComponent, ACharacterBase* InOwnerCharacter) const
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
	if (IsOwnerValid()) OwnerCharacter->RightClickStarted();
}

void UHeroInputComponent::OnRightClickCompleted()
{
	if (IsOwnerValid()) OwnerCharacter->RightClickCompleted();
}

void UHeroInputComponent::OnLeftClickStarted()
{
	if (IsOwnerValid()) OwnerCharacter->LeftClickStarted();
}

void UHeroInputComponent::OnLeftClickCompleted()
{
	if (IsOwnerValid()) OwnerCharacter->LeftClickCompleted();
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

void UHeroInputComponent::OnToggleCombat()
{
	if (IsOwnerValid()) OwnerCharacter->ToggleCombat();
}

void UHeroInputComponent::OnToggleInventory()
{
	if (IsOwnerValid()) OwnerCharacter->ToggleInventory();
}

void UHeroInputComponent::OnTestVitals()
{
	if (IsOwnerValid()) OwnerCharacter->TestVitals();
}
