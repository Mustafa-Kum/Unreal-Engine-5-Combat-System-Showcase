#include "Characters/DummyTargetCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/DamageReceiverComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADummyTargetCharacter::ADummyTargetCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(true);

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetGenerateOverlapEvents(true);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}

	OverheadHealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadHealthBarComponent"));
	if (OverheadHealthBarComponent)
	{
		OverheadHealthBarComponent->SetupAttachment(GetRootComponent());
		OverheadHealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
		OverheadHealthBarComponent->SetDrawAtDesiredSize(true);
		OverheadHealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OverheadHealthBarComponent->SetGenerateOverlapEvents(false);
		OverheadHealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	}
}

void ADummyTargetCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UDamageReceiverComponent* DamageReceiverComponent = FindComponentByClass<UDamageReceiverComponent>())
	{
		DamageReceiverComponent->SetRestoreHealthToMaxOnDefeat(true);
	}
}

