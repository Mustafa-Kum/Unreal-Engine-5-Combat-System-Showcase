#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "PlayerVitalsWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;

/**
 * AAA HUD Widget: Displays HP and Mana bars using GAS attribute change delegates.
 * Reactive design — no Tick needed. Updates only when attribute values actually change.
 */
UCLASS(Abstract)
class WOWCLONE_API UPlayerVitalsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call from HeroCharacter::BeginPlay to bind GAS attribute delegates */
	UFUNCTION(BlueprintCallable, Category = "Vitals UI")
	void InitializeVitals(UAbilitySystemComponent* InASC);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- BindWidget: Must match names in Widget Blueprint ---
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TrailingHealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ManaBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TrailingManaBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ManaText;

	// --- Animation Config ---
	UPROPERTY(EditDefaultsOnly, Category = "Vitals UI|Animation")
	float TrailingDelay = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Vitals UI|Animation")
	float MainBarInterpSpeed = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vitals UI|Animation")
	float TrailingBarInterpSpeed = 3.0f;

private:
	// GAS Attribute Change Callbacks (Reactive UI — zero Tick cost)
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);
	void OnManaChanged(const FOnAttributeChangeData& Data);
	void OnMaxManaChanged(const FOnAttributeChangeData& Data);
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();

	// --- TICK ORCHESTRATION ---
	void ProcessAllInterpolations(float DeltaTime);
	void ProcessMainBarInterpolation(class UProgressBar* MainBar, float TargetPercent, float DeltaTime);
	void ProcessTrailingBarInterpolation(class UProgressBar* TrailingBar, float TargetPercent, float& Timer, float DeltaTime);
	void ExecuteBarInterpolation(class UProgressBar* TargetBar, float TargetPercent, float InterpSpeed, float DeltaTime);

	// --- VITAL UPDATE ORCHESTRATION ---
	void RefreshVitalDisplay(const struct FGameplayAttribute& CurrentAttr, const struct FGameplayAttribute& MaxAttr, float& InOutTargetPercent, float& InOutTrailingTimer, class UProgressBar* TrailingBar, class UTextBlock* ValueText);

	// High Level Dispatchers (SRP)
	void UpdateHealthDisplay();
	void UpdateManaDisplay();

	// Data Processing Helpers
	[[nodiscard]] float FetchAttributeValue(const struct FGameplayAttribute& Attribute) const;
	[[nodiscard]] float CalculateTargetPercent(float CurrentValue, float MaxValue) const;
	void UpdateTrailingState(float NewTargetPercent, float& InOutTargetPercent, float& InOutTrailingTimer, class UProgressBar* TrailingBar);
	void ExecuteVitalTextUpdate(class UTextBlock* TextBlock, float CurrentValue, float MaxValue);

	// DRY: Pure formatting helper
	[[nodiscard]] static FString FormatVitalText(float Current, float Max);
	[[nodiscard]] bool HasActiveInterpolation() const;

	// --- Internal State ---
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> CachedASC;

	float TargetHealthPercent = 1.0f;
	float TargetManaPercent = 1.0f;
	
	float TrailingHealthTimer = 0.0f;
	float TrailingManaTimer = 0.0f;
};
