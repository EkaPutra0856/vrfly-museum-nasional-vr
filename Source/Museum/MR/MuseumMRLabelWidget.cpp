#include "MR/MuseumMRLabelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Rendering/DrawElements.h"

using namespace MuseumMR;

namespace
{
	constexpr float RingSize = 190.f;
	constexpr float RingThickness = 10.f;
	constexpr int32 RingSegments = 64;
}

// ====== API ====== //

void UMuseumMRLabelWidget::SetLabel(const FText& InKicker, const FText& InTitle, const float InProgress, const FLinearColor& InAccent)
{
	const bool bRing = InProgress >= 0.f;
	const bool bLayout = bRing != bHasRing || !Kicker || !InAccent.Equals(Accent);
	KickerText = InKicker;
	TitleText = InTitle;
	Progress = InProgress;
	Accent = InAccent;
	bHasRing = bRing;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bLayout)
	{
		Rebuild();

		return;
	}
	Kicker->SetText(KickerText);
	Kicker->SetVisibility(KickerText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Title->SetText(TitleText);
	if (Percent) Percent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(Progress, 0.f, 1.f) * 100.f))));
}

void UMuseumMRLabelWidget::SetScale(const int32 InKickerSize, const int32 InTitleSize)
{
	KickerSize = InKickerSize;
	TitleSize = InTitleSize;
	Rebuild();
}

// ====== Build ====== //

void UMuseumMRLabelWidget::BuildContent()
{
	RingSpace = nullptr;
	Percent = nullptr;

	// A glass plate hugging its content, centred in the draw area
	UBorder* Plate = MakeBox(RoundedBrush(GlassFill(), 22.f, Accent * FLinearColor(1.f, 1.f, 1.f, 0.85f), 3.f), FMargin(bHasRing ? 26.f : 34.f, 18.f, 40.f, 18.f));
	UOverlaySlot* PlateSlot = Root->AddChildToOverlay(Plate);
	PlateSlot->SetHorizontalAlignment(HAlign_Center);
	PlateSlot->SetVerticalAlignment(VAlign_Center);

	UHorizontalBox* Row = MakeRow();
	Plate->AddChild(Row);

	// The progress ring (drawn in NativePaint) with its percent inside
	if (bHasRing)
	{
		RingSpace = MakeSize(RingSize, RingSize);
		Percent = MakeText(FText::GetEmpty(), EFace::LabelBold, 46, Text());
		Percent->SetJustification(ETextJustify::Center);
		if (USizeBoxSlot* PercentSlot = Cast<USizeBoxSlot>(RingSpace->AddChild(Percent)))
		{
			PercentSlot->SetHorizontalAlignment(HAlign_Center);
			PercentSlot->SetVerticalAlignment(VAlign_Center);
		}
		UHorizontalBoxSlot* RingSlot = Row->AddChildToHorizontalBox(RingSpace);
		RingSlot->SetVerticalAlignment(VAlign_Center);
		RingSlot->SetPadding(FMargin(0.f, 0.f, 30.f, 0.f));
	}

	UVerticalBox* Texts = MakeColumn();
	Row->AddChildToHorizontalBox(Texts)->SetVerticalAlignment(VAlign_Center);
	Kicker = MakeText(KickerText, EFace::LabelBold, KickerSize, Accent);
	Texts->AddChildToVerticalBox(Kicker);
	Kicker->SetVisibility(KickerText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	Title = MakeText(TitleText, EFace::Title, TitleSize, Text());
	Texts->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));

	if (Percent) Percent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(Progress, 0.f, 1.f) * 100.f))));
}

int32 UMuseumMRLabelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!bHasRing || !RingSpace) return LayerId;

	// Track and progress arc, clockwise from the top, in the ring space's own geometry
	const FGeometry& Space = RingSpace->GetCachedGeometry();
	const FVector2D Size = Space.GetLocalSize();
	if (Size.IsNearlyZero()) return LayerId;

	const FVector2D Centre = Size * 0.5;
	const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f - RingThickness;
	const float Tint = InWidgetStyle.GetColorAndOpacityTint().A;
	auto Arc = [&](const float Fraction, const FLinearColor& Color, const int32 Layer)
	{
		const int32 Count = FMath::Max(2, FMath::CeilToInt(RingSegments * Fraction) + 1);
		TArray<FVector2D> Points;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = -HALF_PI + TWO_PI * Fraction * Index / (Count - 1);
			Points.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Space.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color.CopyWithNewOpacity(Color.A * Tint), true, RingThickness);
	};
	Arc(1.f, FLinearColor(1.f, 1.f, 1.f, 0.14f), LayerId + 1);
	if (Progress > 0.f) Arc(FMath::Clamp(Progress, 0.f, 1.f), Accent, LayerId + 2);

	return LayerId + 2;
}
