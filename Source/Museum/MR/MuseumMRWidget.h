#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MR/MuseumMRStyle.h"
#include "MuseumMRWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UOverlay;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * Base of the MR layer's widgets, built in code (no Widget Blueprint). The content sits in a persistent root overlay and
 * is rebuilt by Rebuild() whenever the layout changes (a card opening, a menu filling), so the widget component keeps its
 * Slate widget. Widgets the demo finds by name (buttons, the scrolled text) get stable names.
 */
UCLASS(Abstract)
class UMuseumMRWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual bool Initialize() override;

	/** Clears the content and builds it again (BuildContent). */
	void Rebuild();

protected:
	/** Fills Root. */
	virtual void BuildContent() {}

	// ====== Helpers ====== //

	UTextBlock* MakeText(const FText& Text, MuseumMR::EFace Face, int32 Size, const FLinearColor& Color, bool bWrap = false, FName Name = NAME_None);

	UBorder* MakeBox(const FSlateBrush& Brush, const FMargin& InPadding, FName Name = NAME_None);

	/** A button with a rounded glass look: Fill normally, a cyan glow outline when the laser hovers it. */
	UButton* MakeButton(FName Name, const FLinearColor& Fill, float Radius, const FMargin& InPadding, const FLinearColor& Outline = FLinearColor::Transparent);

	UVerticalBox* MakeColumn();
	UHorizontalBox* MakeRow();
	USizeBox* MakeSize(float Width, float Height);
	UImage* MakeImage(const FSlateBrush& Brush);

	/** A thin horizontal line. */
	UImage* MakeDivider(const FLinearColor& Color, float Thickness = 2.f);

	/** A right-pointing play triangle Height pixels tall (drawn: the UI fonts have no ▶ glyph). */
	UWidget* MakePlayMark(float Height, const FLinearColor& Color);

	/** Text that shrinks to fit its slot instead of being cut. */
	UWidget* MakeFittedText(const FText& Text, MuseumMR::EFace Face, int32 Size, const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> Root;
};
