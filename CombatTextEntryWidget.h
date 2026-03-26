#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatTextEntryWidget.generated.h"

class UTextBlock;

/**
 * Single pooled combat text item.
 * Owns only its local visual state; lifetime/pooling stays in the parent overhead widget.
 */
UCLASS()
class WOWCLONE_API UCombatTextEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ActivateEntry(const FText& InText, const FLinearColor& InTextColor, const FLinearColor& InOutlineColor, int32 InOutlineSize);
	void UpdateEntryVisuals(float InOpacity, float InUniformScale);
	void DeactivateEntry();

protected:
	virtual void NativeOnInitialized() override;

private:
	void EnsureDamageTextBlock();

	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DamageTextBlock;
};
