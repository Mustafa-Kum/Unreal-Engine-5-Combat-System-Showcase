#include "Components/CombatFeedbackComponent.h"
#include "Characters/CharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Components/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/OverheadHealthBarWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatFeedback, Log, All);

UCombatFeedbackComponent::UCombatFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	InitializeOverheadHealthBarWidget();
}

void UCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedOverheadHealthBarWidget = nullptr;
	OwnerCharacter = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UCombatFeedbackComponent::HandleReceivedDamage(float AppliedDamage, const FDamageEvent& DamageEvent)
{
	if (AppliedDamage <= 0.0f || !CachedOverheadHealthBarWidget)
	{
		return;
	}

	CachedOverheadHealthBarWidget->ShowDamageText(AppliedDamage, IsCriticalDamageEvent(DamageEvent));
}

void UCombatFeedbackComponent::InitializeOverheadHealthBarWidget()
{
	if (!OwnerCharacter)
	{
		return;
	}

	UWidgetComponent* WidgetComponent = ResolveOverheadHealthBarComponent();
	if (!WidgetComponent)
	{
		return;
	}

	if (const UCapsuleComponent* CapsuleComp = OwnerCharacter->GetCapsuleComponent())
	{
		const FVector DesignerOffset = WidgetComponent->GetRelativeLocation();
		WidgetComponent->SetRelativeLocation(
			FVector(
				DesignerOffset.X,
				DesignerOffset.Y,
				CapsuleComp->GetScaledCapsuleHalfHeight() + DesignerOffset.Z));
	}

	WidgetComponent->InitWidget();
	CachedOverheadHealthBarWidget = Cast<UOverheadHealthBarWidget>(WidgetComponent->GetUserWidgetObject());
	if (CachedOverheadHealthBarWidget)
	{
		CachedOverheadHealthBarWidget->InitializeHealthBar(OwnerCharacter->GetAbilitySystemComponent());
	}
}

UWidgetComponent* UCombatFeedbackComponent::ResolveOverheadHealthBarComponent() const
{
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	TInlineComponentArray<UWidgetComponent*> WidgetComponents(OwnerCharacter);
	OwnerCharacter->GetComponents(WidgetComponents);

	TArray<UWidgetComponent*> MatchingWidgetComponents;
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent)
		{
			continue;
		}

		const TSubclassOf<UUserWidget> WidgetClass = WidgetComponent->GetWidgetClass();
		if (!WidgetClass || !WidgetClass->IsChildOf(UOverheadHealthBarWidget::StaticClass()))
		{
			continue;
		}

		if (!OverheadHealthBarComponentName.IsNone() && WidgetComponent->GetFName() == OverheadHealthBarComponentName)
		{
			return WidgetComponent;
		}

		MatchingWidgetComponents.Add(WidgetComponent);
	}

	if (MatchingWidgetComponents.Num() == 1)
	{
		return MatchingWidgetComponents[0];
	}

	if (MatchingWidgetComponents.Num() > 1)
	{
		UE_LOG(
			LogCombatFeedback,
			Warning,
			TEXT("CombatFeedbackComponent on %s found multiple overhead health bar widgets. Set OverheadHealthBarComponentName to choose one explicitly."),
			*GetNameSafe(OwnerCharacter));
	}

	return nullptr;
}

bool UCombatFeedbackComponent::IsCriticalDamageEvent(const FDamageEvent& DamageEvent) const
{
	if (DamageEvent.IsOfType(FWoWCloneCombatDamageEvent::ClassID))
	{
		return static_cast<const FWoWCloneCombatDamageEvent&>(DamageEvent).bIsCriticalHit;
	}

	return false;
}

