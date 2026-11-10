#pragma once

#include "CoreMinimal.h"
#include "MR/MuseumMRWidget.h"
#include "MuseumMRLabelWidget.generated.h"

class USizeBox;
class UTextBlock;

/**
 * A floating 3D label of the MR layer: a glass plate with a small caps kicker over a title, optionally led by a circular
 * progress ring (the detection label "OBJEK TERDETEKSI · Arca Dwarapala 100%", storyboard frame 4). Also the hologram tag
 * and the waypoint label ("Arca Dwarapala · 18 m"). Never takes the pointer.
 */
UCLASS()
class UMuseumMRLabelWidget : public UMuseumMRWidget
{
	GENERATED_BODY()

public:
	// ====== API ====== //

	/** Kicker (small caps line), title, and the ring's progress 0..1 (negative = no ring). */
	void SetLabel(const FText& Kicker, const FText& Title, float Progress = -1.f, const FLinearColor& Accent = MuseumMR::Cyan());

	/** Text sizes: the detection label is large, tags are small. */
	void SetScale(int32 InKickerSize, int32 InTitleSize);

protected:
	// ====== Build ====== //

	virtual void BuildContent() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FText KickerText;
	FText TitleText;
	float Progress = -1.f;
	FLinearColor Accent = MuseumMR::Cyan();
	int32 KickerSize = 30;
	int32 TitleSize = 44;
	bool bHasRing = false;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RingSpace;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Kicker;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Percent;
};
