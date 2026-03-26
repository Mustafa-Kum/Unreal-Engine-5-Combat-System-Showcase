#include "UI/OverheadHealthBarWidget.h"
#include "UI/CombatTextEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"

void UOverheadHealthBarWidget::InitializeHealthBar(UAbilitySystemComponent* InASC)
{
	InitializeVitals(InASC);
}

void UOverheadHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeCombatTextPool();
}

void UOverheadHealthBarWidget::ShowDamageText(float DamageAmount, bool bIsCriticalHit)
{
	InitializeCombatTextPool();

	if (!CanDisplayDamageText(DamageAmount))
	{
		return;
	}

	UCombatTextEntryWidget* DamageTextWidget = AcquireCombatTextEntryWidget();
	if (!DamageTextWidget)
	{
		return;
	}

	DamageTextWidget->ActivateEntry(
		FText::AsNumber(FMath::RoundToInt(DamageAmount)),
		bIsCriticalHit ? CriticalDamageTextColor : DamageTextColor,
		DamageTextOutlineColor,
		DamageTextOutlineSize);

	UCanvasPanelSlot* DamageTextSlot = Cast<UCanvasPanelSlot>(DamageTextWidget->Slot);
	if (!DamageTextSlot)
	{
		ReleaseCombatTextEntryWidget(DamageTextWidget);
		return;
	}

	const float SpawnHeightOffset = bIsCriticalHit ? -CriticalDamageTextSpawnHeightOffset : 0.0f;
	const FVector2D SpawnOrigin = ResolveDamageTextSpawnOrigin();
	const FVector2D StartPosition = SpawnOrigin + FVector2D(FMath::FRandRange(-DamageTextHorizontalSpread, DamageTextHorizontalSpread), SpawnHeightOffset);

	DamageTextSlot->SetAutoSize(true);
	DamageTextSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	DamageTextSlot->SetPosition(StartPosition);

	FDamageTextEntry& DamageTextEntry = ActiveDamageTextEntries.AddDefaulted_GetRef();
	DamageTextEntry.TextWidget = DamageTextWidget;
	DamageTextEntry.StartPosition = StartPosition;
	DamageTextEntry.EndPosition = bIsCriticalHit ? StartPosition : (StartPosition + FVector2D(0.0f, -DamageTextRiseDistance));
	DamageTextEntry.bIsCriticalHit = bIsCriticalHit;
}

void UOverheadHealthBarWidget::NativeDestruct()
{
	CleanupCombatTextPool();

	Super::NativeDestruct();
}

void UOverheadHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!ActiveDamageTextEntries.IsEmpty())
	{
		UpdateDamageTextEntries(InDeltaTime);
	}
}

bool UOverheadHealthBarWidget::CanDisplayDamageText(float DamageAmount) const
{
	return DamageAmount > 0.0f && DamageTextLayer;
}

float UOverheadHealthBarWidget::GetTotalDamageTextLifetime() const
{
	return FMath::Max(0.0f, DamageTextRiseDuration) + FMath::Max(KINDA_SMALL_NUMBER, DamageTextFadeDuration);
}

FVector2D UOverheadHealthBarWidget::ResolveDamageTextSpawnOrigin() const
{
	if (HealthBar && DamageTextLayer)
	{
		const FGeometry& HealthBarGeometry = HealthBar->GetCachedGeometry();
		const FGeometry& DamageTextLayerGeometry = DamageTextLayer->GetCachedGeometry();
		const FVector2D HealthBarCenter = HealthBarGeometry.LocalToAbsolute(HealthBarGeometry.GetLocalSize() * 0.5f);
		return DamageTextLayerGeometry.AbsoluteToLocal(HealthBarCenter);
	}

	return FVector2D::ZeroVector;
}

void UOverheadHealthBarWidget::InitializeCombatTextPool()
{
	if (bCombatTextPoolInitialized || !DamageTextLayer)
	{
		return;
	}

	bCombatTextPoolInitialized = true;

	if (!CombatTextEntryWidgetClass)
	{
		CombatTextEntryWidgetClass = UCombatTextEntryWidget::StaticClass();
	}

	ActiveDamageTextEntries.Reserve(InitialCombatTextPoolSize);
	InactiveDamageTextEntries.Reserve(InitialCombatTextPoolSize);
	for (int32 PoolIndex = 0; PoolIndex < InitialCombatTextPoolSize; ++PoolIndex)
	{
		if (UCombatTextEntryWidget* EntryWidget = AcquireCombatTextEntryWidget())
		{
			ReleaseCombatTextEntryWidget(EntryWidget);
		}
	}
}

void UOverheadHealthBarWidget::CleanupCombatTextPool()
{
	for (FDamageTextEntry& Entry : ActiveDamageTextEntries)
	{
		if (Entry.TextWidget)
		{
			Entry.TextWidget->RemoveFromParent();
		}
	}

	for (UCombatTextEntryWidget* EntryWidget : InactiveDamageTextEntries)
	{
		if (EntryWidget)
		{
			EntryWidget->RemoveFromParent();
		}
	}

	ActiveDamageTextEntries.Reset();
	InactiveDamageTextEntries.Reset();
	bCombatTextPoolInitialized = false;
}

UCombatTextEntryWidget* UOverheadHealthBarWidget::AcquireCombatTextEntryWidget()
{
	while (!InactiveDamageTextEntries.IsEmpty())
	{
		if (UCombatTextEntryWidget* EntryWidget = InactiveDamageTextEntries.Pop())
		{
			return EntryWidget;
		}
	}

	if (!CombatTextEntryWidgetClass || !DamageTextLayer)
	{
		return nullptr;
	}

	UCombatTextEntryWidget* EntryWidget = WidgetTree ? WidgetTree->ConstructWidget<UCombatTextEntryWidget>(CombatTextEntryWidgetClass) : nullptr;
	if (!EntryWidget)
	{
		return nullptr;
	}

	UCanvasPanelSlot* EntrySlot = DamageTextLayer->AddChildToCanvas(EntryWidget);
	if (!EntrySlot)
	{
		return nullptr;
	}

	EntrySlot->SetAutoSize(true);
	EntrySlot->SetAlignment(FVector2D(0.5f, 0.5f));
	EntryWidget->DeactivateEntry();

	return EntryWidget;
}

void UOverheadHealthBarWidget::ReleaseCombatTextEntryWidget(UCombatTextEntryWidget* EntryWidget)
{
	if (!EntryWidget)
	{
		return;
	}

	EntryWidget->DeactivateEntry();
	InactiveDamageTextEntries.Add(EntryWidget);
}

void UOverheadHealthBarWidget::UpdateDamageTextEntries(float DeltaTime)
{
	const float SafeRiseDuration = FMath::Max(0.0f, DamageTextRiseDuration);
	const float SafeFadeDuration = FMath::Max(KINDA_SMALL_NUMBER, DamageTextFadeDuration);
	const float TotalLifetime = GetTotalDamageTextLifetime();

	for (int32 EntryIndex = ActiveDamageTextEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		FDamageTextEntry& Entry = ActiveDamageTextEntries[EntryIndex];
		UCombatTextEntryWidget* DamageTextWidget = Entry.TextWidget;
		UCanvasPanelSlot* DamageTextSlot = DamageTextWidget ? Cast<UCanvasPanelSlot>(DamageTextWidget->Slot) : nullptr;
		if (!DamageTextWidget || !DamageTextSlot)
		{
			RemoveDamageTextEntryAt(EntryIndex);
			continue;
		}

		Entry.ElapsedTime += DeltaTime;

		const float RiseAlpha = SafeRiseDuration > 0.0f ? FMath::Clamp(Entry.ElapsedTime / SafeRiseDuration, 0.0f, 1.0f) : 1.0f;
		DamageTextSlot->SetPosition(FMath::Lerp(Entry.StartPosition, Entry.EndPosition, RiseAlpha));

		float Opacity = 1.0f;
		if (Entry.ElapsedTime > SafeRiseDuration)
		{
			const float FadeAlpha = FMath::Clamp((Entry.ElapsedTime - SafeRiseDuration) / SafeFadeDuration, 0.0f, 1.0f);
			Opacity = 1.0f - FadeAlpha;
		}

		if (Entry.bIsCriticalHit)
		{
			const float CriticalScaleAlpha = FMath::Clamp(Entry.ElapsedTime / FMath::Max(KINDA_SMALL_NUMBER, CriticalDamageTextScaleDuration), 0.0f, 1.0f);
			const float CriticalScale = FMath::Lerp(1.0f, CriticalDamageTextPeakScale, CriticalScaleAlpha);
			DamageTextWidget->UpdateEntryVisuals(Opacity, CriticalScale);
		}
		else
		{
			DamageTextWidget->UpdateEntryVisuals(Opacity, 1.0f);
		}

		if (Entry.ElapsedTime >= TotalLifetime)
		{
			RemoveDamageTextEntryAt(EntryIndex);
		}
	}
}

void UOverheadHealthBarWidget::RemoveDamageTextEntryAt(int32 EntryIndex)
{
	if (!ActiveDamageTextEntries.IsValidIndex(EntryIndex))
	{
		return;
	}

	ReleaseCombatTextEntryWidget(ActiveDamageTextEntries[EntryIndex].TextWidget);
	ActiveDamageTextEntries.RemoveAtSwap(EntryIndex);
}

