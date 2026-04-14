#include "Characters/HeroCharacter.h"
#include "AModularRPGPlayerController.h"
#include "Characters/HeroInputComponent.h"
#include "Characters/HeroCameraComponent.h"
#include "Characters/HeroLocomotionComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/CombatComponent.h"
#include "Components/AbilityLoadoutComponent.h"
#include "Components/WeaponActionComponent.h"
#include "DataAssets/WeaponDataAsset.h"

AHeroCharacter::AHeroCharacter()
{
	// Character orchestration stays event-driven; camera and locomotion own their own ticks.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The character should not rotate with the camera
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Reset bit-packed booleans (AAA Standards)
	bIsHeavyAttackPressed = 0;
	bIsLightAttackPressed = 0;

	// Default initialization prevents warnings; proper values set in component
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Input Component Setup
	HeroInputComp = CreateDefaultSubobject<UHeroInputComponent>(TEXT("HeroInputComponent"));

	// Camera Component Setup
	HeroCameraComp = CreateDefaultSubobject<UHeroCameraComponent>(TEXT("HeroCameraComponent"));

	// Lifecycle-safe camera rig creation belongs in the actor constructor.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Locomotion Component Setup
	HeroLocomotionComp = CreateDefaultSubobject<UHeroLocomotionComponent>(TEXT("HeroLocomotionComponent"));

	// Inventory Setup
	InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// Weapon runtime transitions live outside inventory data ownership.
	WeaponActionComp = CreateDefaultSubobject<UWeaponActionComponent>(TEXT("WeaponActionComponent"));

	// Combat Setup
	CombatComp = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));

	// Hotbar/loadout state belongs to a gameplay component, not the UI.
	AbilityLoadoutComp = CreateDefaultSubobject<UAbilityLoadoutComponent>(TEXT("AbilityLoadoutComponent"));
}

void AHeroCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeComponents();
	InitializePlayerInputState();
	InitializeHUD();
}

void AHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AHeroCharacter::InitializePlayerInputState()
{
	if (GetPlayerController() && IsLocalPlayerControlled())
	{
		SetCharacterYawDecoupled(true);
	}
}

void AHeroCharacter::InitializeComponents()
{
	if (HeroLocomotionComp)
	{
		HeroLocomotionComp->InitializeLocomotion(this);
	}

	if (HeroCameraComp)
	{
		HeroCameraComp->InitializeCamera(this);
	}
}

void AHeroCharacter::InitializeHUD()
{
	if (!IsLocalPlayerControlled())
	{
		return;
	}

	if (AModularRPGPlayerController* PC = Cast<AModularRPGPlayerController>(GetPlayerController()))
	{
		PC->InitializeHeroUI();
	}
}

float AHeroCharacter::GetBackwardMovementSpeedMultiplier() const
{
	return HeroLocomotionComp ? HeroLocomotionComp->GetBackwardSpeedMultiplier() : 1.0f;
}

bool AHeroCharacter::IsInCombat() const
{
	return CombatComp && CombatComp->IsInCombat();
}

void AHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	InitializeInputModule(PlayerInputComponent);
}

void AHeroCharacter::InitializeInputModule(UInputComponent* PlayerInputComponent)
{
	// AAA: Enhanced Input system routing
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (HeroInputComp)
		{
			HeroInputComp->InitializeInput(EnhancedInputComponent, this);
		}
	}
}

void AHeroCharacter::Move(const FInputActionValue& Value)
{
	if (!CanMove() || !HeroLocomotionComp) return;

	const FVector2D ProcessedInput = HeroLocomotionComp->GetNormalizedAndScaledMovementInput(Value.Get<FVector2D>());
	if (ProcessedInput.IsNearlyZero()) return;

	if (CombatComp)
	{
		CombatComp->TryInterruptAttackForMovement();
	}

	AddMovementInput(GetMovementDirection(EAxis::X), ProcessedInput.X);
	AddMovementInput(GetMovementDirection(EAxis::Y), ProcessedInput.Y);
}

bool AHeroCharacter::CanMove() const
{
	return Controller != nullptr;
}

FVector AHeroCharacter::GetMovementDirection(EAxis::Type Axis) const
{
	const FRotator YawRotation(0, GetActorRotation().Yaw, 0);
	return FRotationMatrix(YawRotation).GetUnitAxis(Axis);
}

void AHeroCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr && !LookAxisVector.IsZero())
	{
		HandleLookInput(LookAxisVector);
	}
}

void AHeroCharacter::HandleLookInput(const FVector2D& LookAxisVector)
{
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AHeroCharacter::HeavyAttackStarted()
{
	bIsHeavyAttackPressed = 1;

	if (CombatComp)
	{
		CombatComp->ProcessAttackInput(ECombatAttackType::Heavy);
	}
}

void AHeroCharacter::HeavyAttackCompleted()
{
	bIsHeavyAttackPressed = 0;
}

void AHeroCharacter::SetCharacterYawDecoupled(bool bIsDecoupled)
{
	ConfigureRotationSettings(bIsDecoupled);
	
	if (HeroCameraComp)
	{
		HeroCameraComp->SetCameraDecoupled(bIsDecoupled);
	}
}

void AHeroCharacter::ConfigureRotationSettings(bool bIsDecoupled)
{
	bUseControllerRotationYaw = bIsDecoupled;

	// AAA Standard: Ensure movement rotation is strictly handled by the design pattern
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false; 
	}
}

void AHeroCharacter::LightAttackStarted()
{
	bIsLightAttackPressed = 1;

	if (CombatComp)
	{
		CombatComp->ProcessAttackInput(ECombatAttackType::Light);
	}
}

void AHeroCharacter::LightAttackCompleted()
{
	bIsLightAttackPressed = 0;
	// No longer hiding/showing cursor here to avoid conflict with look input
}

void AHeroCharacter::ActivatePrimaryAbility()
{
	ActivateActionBarSlot(0);
}

void AHeroCharacter::ActivateActionBarSlot(int32 SlotIndex)
{
	if (AbilityLoadoutComp)
	{
		AbilityLoadoutComp->TryActivateSlot(SlotIndex);
	}
}

void AHeroCharacter::Zoom(const FInputActionValue& Value)
{
	if (!HeroCameraComp) return;

	const float ZoomValue = Value.Get<float>();
	if (ZoomValue == 0.0f) return;

	HeroCameraComp->ApplyCameraZoom(ZoomValue);
}

void AHeroCharacter::ToggleWeapon()
{
	if (WeaponActionComp)
	{
		WeaponActionComp->ToggleDrawHolster();
	}
}

void AHeroCharacter::ToggleWalk()
{
	if (CanToggleWalk())
	{
		HandleWalkSpeedToggle();
	}
}

bool AHeroCharacter::CanToggleWalk() const
{
	return !IsInCombat();
}

void AHeroCharacter::HandleWalkSpeedToggle()
{
	if (HeroLocomotionComp)
	{
		HeroLocomotionComp->ToggleWalkSpeed();
	}
}

void AHeroCharacter::ToggleInventory()
{
	if (AModularRPGPlayerController* PC = Cast<AModularRPGPlayerController>(GetPlayerController()))
	{
		PC->ToggleInventoryUI();
	}

	OnInventoryToggled.Broadcast();
}

void AHeroCharacter::ToggleSkillBook()
{
	if (AModularRPGPlayerController* PC = Cast<AModularRPGPlayerController>(GetPlayerController()))
	{
		PC->ToggleSkillBookUI();
	}
}

APlayerController* AHeroCharacter::GetPlayerController() const
{
	return Cast<APlayerController>(GetController());
}

bool AHeroCharacter::IsLocalPlayerControlled() const
{
	if (const APlayerController* PC = GetPlayerController())
	{
		return PC->IsLocalPlayerController();
	}

	return false;
}
