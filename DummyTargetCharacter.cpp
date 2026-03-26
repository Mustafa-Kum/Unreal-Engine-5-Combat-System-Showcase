#include "Characters/DummyTargetCharacter.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDummyTargetCharacter, Log, All);

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

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).AddUObject(this, &ADummyTargetCharacter::OnHealthChanged);
	}
}

void ADummyTargetCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ADummyTargetCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float MaxHealth = ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxHealthAttribute());

	UE_LOG(LogDummyTargetCharacter, Log, TEXT("%s Health: %.2f / %.2f"), *GetName(), Data.NewValue, MaxHealth);

	if (Data.NewValue <= 0.0f)
	{
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), MaxHealth);
	}
}

