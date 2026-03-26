#include "UI/PlayerVitalsWidget.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AttributeSets/CharacterAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UPlayerVitalsWidget::InitializeVitals(UAbilitySystemComponent* InASC)
{
	if (!InASC) return;

	UnbindFromAbilitySystem();
	CachedASC = InASC;
	BindToAbilitySystem(CachedASC);

	// Initial display refresh with current attribute values
	UpdateHealthDisplay();
	UpdateManaDisplay();
}

void UPlayerVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	// Start trailing timers effectively infinite so they don't move initially
	TrailingHealthTimer = 999.0f;
	TrailingManaTimer = 999.0f;

	// If already initialized (InitializeVitals called before AddToViewport), force a visual refresh
	if (CachedASC)
	{
		UpdateHealthDisplay();
		UpdateManaDisplay();
	}
}

void UPlayerVitalsWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();

	Super::NativeDestruct();
}

void UPlayerVitalsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!HasActiveInterpolation())
	{
		return;
	}

	ProcessAllInterpolations(InDeltaTime);
}

void UPlayerVitalsWidget::ProcessAllInterpolations(float DeltaTime)
{
	// Orchestrate Main Bars
	ProcessMainBarInterpolation(HealthBar, TargetHealthPercent, DeltaTime);
	ProcessMainBarInterpolation(ManaBar, TargetManaPercent, DeltaTime);

	// Orchestrate Trailing Bars
	ProcessTrailingBarInterpolation(TrailingHealthBar, TargetHealthPercent, TrailingHealthTimer, DeltaTime);
	ProcessTrailingBarInterpolation(TrailingManaBar, TargetManaPercent, TrailingManaTimer, DeltaTime);
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

	InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnMaxHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetManaAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnManaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxManaAttribute()).AddUObject(this, &UPlayerVitalsWidget::OnMaxManaChanged);
}

void UPlayerVitalsWidget::UnbindFromAbilitySystem()
{
	if (!CachedASC)
	{
		return;
	}

	CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute()).RemoveAll(this);
	CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetManaAttribute()).RemoveAll(this);
	CachedASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMaxManaAttribute()).RemoveAll(this);
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
		TrailingManaBar,
		ManaText
	);
}

void UPlayerVitalsWidget::RefreshVitalDisplay(const FGameplayAttribute& CurrentAttr, const FGameplayAttribute& MaxAttr, float& InOutTargetPercent, float& InOutTrailingTimer, UProgressBar* TrailingBar, UTextBlock* ValueText)
{
	// 1. Validation
	if (!CachedASC) return;

	// 2. Processing
	const float CurrentValue = FetchAttributeValue(CurrentAttr);
	const float MaxValue = FetchAttributeValue(MaxAttr);
	const float NewTargetPercent = CalculateTargetPercent(CurrentValue, MaxValue);

	// 3. Execution
	UpdateTrailingState(NewTargetPercent, InOutTargetPercent, InOutTrailingTimer, TrailingBar);
	ExecuteVitalTextUpdate(ValueText, CurrentValue, MaxValue);
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

void UPlayerVitalsWidget::UpdateTrailingState(float NewTargetPercent, float& InOutTargetPercent, float& InOutTrailingTimer, UProgressBar* TrailingBar)
{
    // If healing or startup: snap trailing bar instantly
	if (NewTargetPercent >= InOutTargetPercent)
	{
		if (TrailingBar) TrailingBar->SetPercent(NewTargetPercent);
		InOutTrailingTimer = 999.0f; // Disable trailing animation
	}
	else
	{
		// Took damage: start delay countdown
		InOutTrailingTimer = TrailingDelay;
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
