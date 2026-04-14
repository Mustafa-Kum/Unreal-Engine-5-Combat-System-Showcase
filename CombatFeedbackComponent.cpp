#include "Components/CombatFeedbackComponent.h"
#include "CombatTypes.h"
#include "Characters/CharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
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
	InitializeAreaIndicator();
}

void UCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UpdateGroundAreaIndicatorRefreshTimer(false);
	ActiveGroundAreaIndicatorRequests.Reset();
	HideGroundAreaIndicator();

	if (AreaIndicatorDecalComponent)
	{
		AreaIndicatorDecalComponent->DestroyComponent();
		AreaIndicatorDecalComponent = nullptr;
	}

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

void UCombatFeedbackComponent::InitializeAreaIndicator()
{
	if (!OwnerCharacter || !AreaIndicatorMaterial || AreaIndicatorDecalComponent)
	{
		return;
	}

	AreaIndicatorDecalComponent = NewObject<UDecalComponent>(OwnerCharacter, TEXT("GroundAreaIndicatorDecal"));
	if (!AreaIndicatorDecalComponent)
	{
		return;
	}

	AreaIndicatorDecalComponent->SetupAttachment(OwnerCharacter->GetRootComponent());
	AreaIndicatorDecalComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	AreaIndicatorDecalComponent->SetRelativeLocation(FVector(0.0f, 0.0f, AreaIndicatorGroundOffset));
	AreaIndicatorDecalComponent->SetDecalMaterial(AreaIndicatorMaterial);
	AreaIndicatorDecalComponent->SetVisibility(false);
	AreaIndicatorDecalComponent->RegisterComponent();
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

void UCombatFeedbackComponent::RequestGroundAreaIndicator(UObject* RequestOwner, float Radius)
{
	if (!RequestOwner)
	{
		return;
	}

	if (!ShouldDisplayLocalAreaIndicator() || Radius <= 0.0f)
	{
		ClearGroundAreaIndicatorRequest(RequestOwner);
		return;
	}

	for (FGroundAreaIndicatorRequest& Request : ActiveGroundAreaIndicatorRequests)
	{
		if (Request.RequestOwner.Get() == RequestOwner)
		{
			Request.Radius = Radius;
			RefreshGroundAreaIndicator();
			return;
		}
	}

	FGroundAreaIndicatorRequest& NewRequest = ActiveGroundAreaIndicatorRequests.AddDefaulted_GetRef();
	NewRequest.RequestOwner = RequestOwner;
	NewRequest.Radius = Radius;
	RefreshGroundAreaIndicator();
}

void UCombatFeedbackComponent::ClearGroundAreaIndicatorRequest(UObject* RequestOwner)
{
	if (!RequestOwner)
	{
		return;
	}

	ActiveGroundAreaIndicatorRequests.RemoveAll(
		[RequestOwner](const FGroundAreaIndicatorRequest& Request)
		{
			return !Request.RequestOwner.IsValid() || Request.RequestOwner.Get() == RequestOwner;
		});

	RefreshGroundAreaIndicator();
}

void UCombatFeedbackComponent::RefreshGroundAreaIndicator()
{
	ActiveGroundAreaIndicatorRequests.RemoveAll(
		[](const FGroundAreaIndicatorRequest& Request)
		{
			return !Request.RequestOwner.IsValid() || Request.Radius <= 0.0f;
		});

	const bool bHasActiveRequests = ActiveGroundAreaIndicatorRequests.Num() > 0;
	UpdateGroundAreaIndicatorRefreshTimer(bHasActiveRequests);

	if (!bHasActiveRequests)
	{
		HideGroundAreaIndicator();
		return;
	}

	if (!ShouldDisplayLocalAreaIndicator())
	{
		HideGroundAreaIndicator();
		return;
	}

	float LargestRequestedRadius = 0.0f;
	for (const FGroundAreaIndicatorRequest& Request : ActiveGroundAreaIndicatorRequests)
	{
		LargestRequestedRadius = FMath::Max(LargestRequestedRadius, Request.Radius);
	}

	if (LargestRequestedRadius > 0.0f)
	{
		ShowGroundAreaIndicator(LargestRequestedRadius);
		return;
	}

	HideGroundAreaIndicator();
}

void UCombatFeedbackComponent::HandleGroundAreaIndicatorRefreshTimer()
{
	if (ActiveGroundAreaIndicatorRequests.Num() == 0)
	{
		UpdateGroundAreaIndicatorRefreshTimer(false);
		return;
	}

	RefreshGroundAreaIndicator();
}

void UCombatFeedbackComponent::ShowGroundAreaIndicator(float Radius)
{
	if (!ShouldDisplayLocalAreaIndicator() || Radius <= 0.0f)
	{
		return;
	}

	if (!AreaIndicatorDecalComponent)
	{
		InitializeAreaIndicator();
	}

	if (!AreaIndicatorDecalComponent)
	{
		return;
	}

	AreaIndicatorDecalComponent->DecalSize = FVector(AreaIndicatorProjectionDepth, Radius, Radius);
	AreaIndicatorDecalComponent->SetVisibility(true);
}

void UCombatFeedbackComponent::HideGroundAreaIndicator()
{
	if (AreaIndicatorDecalComponent)
	{
		AreaIndicatorDecalComponent->SetVisibility(false);
	}
}

void UCombatFeedbackComponent::UpdateGroundAreaIndicatorRefreshTimer(bool bShouldRun)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (!bShouldRun)
	{
		TimerManager.ClearTimer(GroundAreaIndicatorRefreshTimerHandle);
		return;
	}

	if (!TimerManager.IsTimerActive(GroundAreaIndicatorRefreshTimerHandle))
	{
		TimerManager.SetTimer(
			GroundAreaIndicatorRefreshTimerHandle,
			this,
			&UCombatFeedbackComponent::HandleGroundAreaIndicatorRefreshTimer,
			AreaIndicatorRefreshInterval,
			true);
	}
}

bool UCombatFeedbackComponent::ShouldDisplayLocalAreaIndicator() const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(OwnerCharacter);
	return OwnerPawn && OwnerPawn->IsLocallyControlled();
}

