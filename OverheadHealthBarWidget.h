#pragma once

#include "CoreMinimal.h"
#include "UI/PlayerVitalsWidget.h"
#include "OverheadHealthBarWidget.generated.h"

class UAbilitySystemComponent;
class UCanvasPanel;
class UCombatTextEntryWidget;

/**
 * World-space variation of PlayerVitalsWidget.
 * Reuses the exact same health interpolation and trailing logic as the player HUD,
 * while allowing overhead widgets to omit mana-related bindings entirely.
 * Owns transient combat text presentation for the target it is attached to.
 */
UCLASS()
class WOWCLONE_API UOverheadHealthBarWidget : public UPlayerVitalsWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vitals UI")
	void InitializeHealthBar(UAbilitySystemComponent* InASC);

	void ShowDamageText(float DamageAmount, bool bIsCriticalHit);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FDamageTextEntry
	{
		TObjectPtr<UCombatTextEntryWidget> TextWidget;
		FVector2D StartPosition = FVector2D::ZeroVector;
		FVector2D EndPosition = FVector2D::ZeroVector;
		float ElapsedTime = 0.0f;
		bool bIsCriticalHit = false;
	};

	[[nodiscard]] bool CanDisplayDamageText(float DamageAmount) const;
	[[nodiscard]] float GetTotalDamageTextLifetime() const;
	[[nodiscard]] FVector2D ResolveDamageTextSpawnOrigin() const;
	void InitializeCombatTextPool();
	void CleanupCombatTextPool();
	[[nodiscard]] UCombatTextEntryWidget* AcquireCombatTextEntryWidget();
	void ReleaseCombatTextEntryWidget(UCombatTextEntryWidget* EntryWidget);
	void UpdateDamageTextEntries(float DeltaTime);
	void RemoveDamageTextEntryAt(int32 EntryIndex);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> DamageTextLayer;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text")
	TSubclassOf<UCombatTextEntryWidget> CombatTextEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text", meta = (ClampMin = "0"))
	int32 InitialCombatTextPoolSize = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text")
	float DamageTextRiseDuration = 0.28f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text")
	float DamageTextFadeDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text")
	float DamageTextRiseDistance = 36.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text")
	float DamageTextHorizontalSpread = 18.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text")
	FLinearColor DamageTextColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text")
	FLinearColor DamageTextOutlineColor = FLinearColor::Black;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text", meta = (ClampMin = "0"))
	int32 DamageTextOutlineSize = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text|Critical")
	FLinearColor CriticalDamageTextColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text|Critical")
	float CriticalDamageTextSpawnHeightOffset = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text|Critical", meta = (ClampMin = "1.0"))
	float CriticalDamageTextPeakScale = 1.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat Text|Critical", meta = (ClampMin = "0.01"))
	float CriticalDamageTextScaleDuration = 0.08f;

	TArray<FDamageTextEntry> ActiveDamageTextEntries;
	TArray<TObjectPtr<UCombatTextEntryWidget>> InactiveDamageTextEntries;
	bool bCombatTextPoolInitialized = false;
};
