#include "UI/CombatTextEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"

void UCombatTextEntryWidget::ActivateEntry(const FText& InText, const FLinearColor& InTextColor, const FLinearColor& InOutlineColor, int32 InOutlineSize)
{
	EnsureDamageTextBlock();
	if (!DamageTextBlock)
	{
		return;
	}

	DamageTextBlock->SetText(InText);
	DamageTextBlock->SetColorAndOpacity(FSlateColor(InTextColor));

	FSlateFontInfo FontInfo = DamageTextBlock->GetFont();
	FontInfo.OutlineSettings.OutlineSize = InOutlineSize;
	FontInfo.OutlineSettings.OutlineColor = InOutlineColor;
	DamageTextBlock->SetFont(FontInfo);

	SetRenderOpacity(1.0f);
	SetRenderScale(FVector2D(1.0f, 1.0f));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCombatTextEntryWidget::UpdateEntryVisuals(float InOpacity, float InUniformScale)
{
	SetRenderOpacity(InOpacity);
	SetRenderScale(FVector2D(InUniformScale, InUniformScale));
}

void UCombatTextEntryWidget::DeactivateEntry()
{
	if (DamageTextBlock)
	{
		DamageTextBlock->SetText(FText::GetEmpty());
	}

	SetRenderOpacity(1.0f);
	SetRenderScale(FVector2D(1.0f, 1.0f));
	SetVisibility(ESlateVisibility::Collapsed);
}

void UCombatTextEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureDamageTextBlock();
	DeactivateEntry();
}

void UCombatTextEntryWidget::EnsureDamageTextBlock()
{
	if (DamageTextBlock || !WidgetTree)
	{
		return;
	}

	if (UWidget* ExistingTextWidget = WidgetTree->FindWidget(TEXT("DamageTextBlock")))
	{
		DamageTextBlock = Cast<UTextBlock>(ExistingTextWidget);
	}
	else
	{
		DamageTextBlock = Cast<UTextBlock>(WidgetTree->RootWidget);
	}

	if (DamageTextBlock)
	{
		DamageTextBlock->SetJustification(ETextJustify::Center);
		DamageTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	if (WidgetTree->RootWidget)
	{
		return;
	}

	DamageTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DamageTextBlock"));
	if (!DamageTextBlock)
	{
		return;
	}

	DamageTextBlock->SetJustification(ETextJustify::Center);
	DamageTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = DamageTextBlock;
}

