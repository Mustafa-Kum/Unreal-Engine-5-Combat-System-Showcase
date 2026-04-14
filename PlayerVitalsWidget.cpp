#include "UI/PlayerVitalsWidget.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

namespace
{
	constexpr float VitalsWidgetUpdateInterval = 1.0f / 60.0f;
}

void UPlayerVitalsWidget::InitializeVitals(UAbilitySystemComponent* InASC)
{
	if (!InASC) return;

	UnbindFromAbilitySystem();
	CachedASC = InASC;
	BindToAbilitySystem(CachedASC);

	// Initial display refresh with current attribute values
	UpdateHealthDisplay();
	UpdateManaDisplay();
	RefreshScheduledUpdateState();
}

void UPlayerVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// Start trailing timers effectively infinite so they don't move initially
	TrailingHealthTimer = 999.0f;
	TrailingManaTimer = 999.0f;
	PendingTrailingHealthRaiseTarget = 1.0f;
	PendingTrailingManaRaiseTarget = 1.0f;

	// If already initialized (InitializeVitals called before AddToViewport), force a visual refresh
	if (CachedASC)
	{
		UpdateHealthDisplay();
		UpdateManaDisplay();
	}

	RefreshScheduledUpdateState();
}

void UPlayerVitalsWidget::NativeDestruct()
{
	StopScheduledUpdates();
	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

void UPlayerVitalsWidget::ProcessAllInterpolations(float DeltaTime)
{
	// Orchestrate Main Bars
	ProcessMainBarInterpolation(HealthBar, TargetHealthPercent, DeltaTime);
	ProcessMainBarInterpolation(ManaBar, TargetManaPercent, DeltaTime);

	// Orchestrate Trailing Bars
	ProcessTrailingBarInterpolation(TrailingHealthBar, TargetHealthPercent, TrailingHealthTimer, DeltaTime);
	ProcessTrailingBarInterpolation(TrailingManaBar, TargetManaPercent, TrailingManaTimer, DeltaTime);

	if (TrailingHealthBar
		&& TrailingHealthTimer >= 999.0f
		&& PendingTrailingHealthRaiseTarget > TrailingHealthBar->GetPercent())
	{
		TrailingHealthBar->SetPercent(PendingTrailingHealthRaiseTarget);
	}

	if (TrailingManaBar
		&& TrailingManaTimer >= 999.0f
		&& PendingTrailingManaRaiseTarget > TrailingManaBar->GetPercent())
	{
		TrailingManaBar->SetPercent(PendingTrailingManaRaiseTarget);
	}
}

void UPlayerVitalsWidget::ProcessMainBarInterpolation(UProgressBar* MainBar, float TargetPercent, float DeltaTime)
{
	if (MainBar)
	{
		ExecuteBarInterpolation(MainBar, TargetPercent, MainBarInterpSpeed, DeltaTime);
	}
}

void UPlayerVitalsWidget::ProcessTrailingBarInterpolation(UProgressBar* TrailingBar, float TargetPercent, float& Timer, float DeltaTime)
{
	if (!TrailingBar) return;

	Timer -= DeltaTime;
	if (Timer <= 0.0f)
	{
		ExecuteBarInterpolation(TrailingBar, TargetPercent, TrailingBarInterpSpeed, DeltaTime);
	}

	const bool bReachedTarget = FMath::IsNearlyEqual(TrailingBar->GetPercent(), TargetPercent, 0.0025f);
	if (bReachedTarget && Timer < 999.0f)
	{
		Timer = 999.0f;
	}
}

void UPlayerVitalsWidget::ExecuteBarInterpolation(UProgressBar* TargetBar, float TargetPercent, float InterpSpeed, float DeltaTime)
{
	const float CurrentPercent = TargetBar->GetPercent();
	TargetBar->SetPercent(FMath::FInterpTo(CurrentPercent, TargetPercent, DeltaTime, InterpSpeed));
}

// ==============================================================================
// GAS Attribute Change Callbacks (Reactive UI)
// ==============================================================================

void UPlayerVitalsWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UpdateHealthDisplay();
}

void UPlayerVitalsWidget::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	UpdateHealthDisplay();
}

void UPlayerVitalsWidget::OnManaChanged(const FOnAttributeChangeData& Data)
{
	UpdateManaDisplay();
}

void UPlayerVitalsWidget::OnMaxManaChanged(const FOnAttributeChangeData& Data)
{
	UpdateManaDisplay();
}

void UPlayerVitalsWidget::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}

	HealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnHealthChanged);
	MaxHealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnMaxHealthChanged);
	ManaChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetManaAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnManaChanged);
	MaxManaChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxManaAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnMaxManaChanged);
}

void UPlayerVitalsWidget::UnbindFromAbilitySystem()
{
	if (!CachedASC)
	{
		return;
	}

	if (HealthChangedHandle.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		HealthChangedHandle.Reset();
	}

	if (MaxHealthChangedHandle.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		MaxHealthChangedHandle.Reset();
	}

	if (ManaChangedHandle.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetManaAttribute()).Remove(ManaChangedHandle);
		ManaChangedHandle.Reset();
	}

	if (MaxManaChangedHandle.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxManaAttribute()).Remove(MaxManaChangedHandle);
		MaxManaChangedHandle.Reset();
	}

	CachedASC = nullptr;
}

// ==============================================================================
// Display Updaters — Orchestration Layer (DRY)
// ==============================================================================

void UPlayerVitalsWidget::UpdateHealthDisplay()
{
	RefreshVitalDisplay(
		UCharacterAttributeSet::GetHealthAttribute(),
		UCharacterAttributeSet::GetMaxHealthAttribute(),
		TargetHealthPercent,
		TrailingHealthTimer,
		PendingTrailingHealthRaiseTarget,
		TrailingHealthBar,
		HealthText
	);
}

void UPlayerVitalsWidget::UpdateManaDisplay()
{
	RefreshVitalDisplay(
		UCharacterAttributeSet::GetManaAttribute(),
		UCharacterAttributeSet::GetMaxManaAttribute(),
		TargetManaPercent,
		TrailingManaTimer,
		PendingTrailingManaRaiseTarget,
		TrailingManaBar,
		ManaText
	);
}

void UPlayerVitalsWidget::RefreshVitalDisplay(const FGameplayAttribute& CurrentAttr, const FGameplayAttribute& MaxAttr, float& InOutTargetPercent, float& InOutTrailingTimer, float& InOutPendingTrailingRaiseTarget, UProgressBar* TrailingBar, UTextBlock* ValueText)
{
	// 1. Validation
	if (!CachedASC) return;

	// 2. Processing
	const float CurrentValue = FetchAttributeValue(CurrentAttr);
	const float MaxValue = FetchAttributeValue(MaxAttr);
	const float NewTargetPercent = CalculateTargetPercent(CurrentValue, MaxValue);

	// 3. Execution
	UpdateTrailingState(NewTargetPercent, InOutTargetPercent, InOutTrailingTimer, InOutPendingTrailingRaiseTarget, TrailingBar);
	ExecuteVitalTextUpdate(ValueText, CurrentValue, MaxValue);
	RefreshScheduledUpdateState();
}

// ==============================================================================
// Data Processing Helpers (AAA Standards)
// ==============================================================================

float UPlayerVitalsWidget::FetchAttributeValue(const FGameplayAttribute& Attribute) const
{
	return CachedASC->GetNumericAttribute(Attribute);
}

float UPlayerVitalsWidget::CalculateTargetPercent(float CurrentValue, float MaxValue) const
{
	return (MaxValue > 0.0f) ? (CurrentValue / MaxValue) : 0.0f;
}

void UPlayerVitalsWidget::UpdateTrailingState(float NewTargetPercent, float& InOutTargetPercent, float& InOutTrailingTimer, float& InOutPendingTrailingRaiseTarget, UProgressBar* TrailingBar)
{
	const float CurrentTrailingPercent = TrailingBar ? TrailingBar->GetPercent() : InOutTargetPercent;

	// A drop starts a delayed trailing animation immediately and clears any older queued raise.
	if (NewTargetPercent < InOutTargetPercent)
	{
		InOutPendingTrailingRaiseTarget = NewTargetPercent;
		InOutTrailingTimer = TrailingDelay;
	}
	else
	{
		const bool bTrailingDropInProgress = TrailingBar
			&& CurrentTrailingPercent > InOutTargetPercent
			&& InOutTrailingTimer < 999.0f;

		if (bTrailingDropInProgress)
		{
			InOutPendingTrailingRaiseTarget = FMath::Max(InOutPendingTrailingRaiseTarget, NewTargetPercent);
		}
		else
		{
			if (TrailingBar)
			{
				TrailingBar->SetPercent(NewTargetPercent);
			}

			InOutPendingTrailingRaiseTarget = NewTargetPercent;
			InOutTrailingTimer = 999.0f;
		}
	}

	InOutTargetPercent = NewTargetPercent;
}

void UPlayerVitalsWidget::ExecuteVitalTextUpdate(UTextBlock* TextBlock, float CurrentValue, float MaxValue)
{
	if (TextBlock)
	{
		TextBlock->SetText(FText::FromString(FormatVitalText(CurrentValue, MaxValue)));
	}
}

FString UPlayerVitalsWidget::FormatVitalText(float Current, float Max)
{
	return FString::Printf(TEXT("%d / %d"), FMath::TruncToInt(Current), FMath::TruncToInt(Max));
}

bool UPlayerVitalsWidget::HasActiveInterpolation() const
{
	const bool bHealthTrailingActive = TrailingHealthBar && !FMath::IsNearlyEqual(TrailingHealthBar->GetPercent(), TargetHealthPercent, KINDA_SMALL_NUMBER);
	const bool bManaTrailingActive = TrailingManaBar && !FMath::IsNearlyEqual(TrailingManaBar->GetPercent(), TargetManaPercent, KINDA_SMALL_NUMBER);
	const bool bHealthMainActive = HealthBar && !FMath::IsNearlyEqual(HealthBar->GetPercent(), TargetHealthPercent, KINDA_SMALL_NUMBER);
	const bool bManaMainActive = ManaBar && !FMath::IsNearlyEqual(ManaBar->GetPercent(), TargetManaPercent, KINDA_SMALL_NUMBER);

	return bHealthTrailingActive || bManaTrailingActive || bHealthMainActive || bManaMainActive;
}

void UPlayerVitalsWidget::RefreshScheduledUpdateState()
{
	if (HasActiveInterpolation() || HasAdditionalScheduledWork())
	{
		StartScheduledUpdates();
		return;
	}

	StopScheduledUpdates();
}

bool UPlayerVitalsWidget::HasAdditionalScheduledWork() const
{
	return false;
}

void UPlayerVitalsWidget::ProcessAdditionalScheduledWork(float DeltaTime)
{
}

void UPlayerVitalsWidget::HandleScheduledUpdate()
{
	ProcessAllInterpolations(VitalsWidgetUpdateInterval);
	ProcessAdditionalScheduledWork(VitalsWidgetUpdateInterval);
	RefreshScheduledUpdateState();
}

void UPlayerVitalsWidget::StartScheduledUpdates()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(ScheduledUpdateTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		ScheduledUpdateTimerHandle,
		this,
		&UPlayerVitalsWidget::HandleScheduledUpdate,
		VitalsWidgetUpdateInterval,
		true);
}

void UPlayerVitalsWidget::StopScheduledUpdates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScheduledUpdateTimerHandle);
	}
}
