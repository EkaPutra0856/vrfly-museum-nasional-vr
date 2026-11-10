#include "MR/MuseumMRWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

// ====== Lifecycle ====== //

bool UMuseumMRWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();

	// Built once in code: a persistent root, its content rebuilt on demand
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		BuildContent();
	}

	return bSuperInitialized;
}

void UMuseumMRWidget::Rebuild()
{
	if (!Root) return;

	// The old widgets leave the tree for good, so the new ones can take their names (HeaderButton0, CardBody, ...)
	TArray<UWidget*> Old;
	UWidgetTree::GetChildWidgets(Root, Old);
	Root->ClearChildren();
	for (UWidget* Widget : Old) Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	BuildContent();
}

// ====== Helpers ====== //

UTextBlock* UMuseumMRWidget::MakeText(const FText& Text, const MuseumMR::EFace Face, const int32 Size, const FLinearColor& Color, const bool bWrap, const FName Name)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Block->SetText(Text);
	Block->SetFont(MuseumMR::Font(Face, Size));
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetAutoWrapText(bWrap);
	Block->SetVisibility(ESlateVisibility::HitTestInvisible);

	return Block;
}

UBorder* UMuseumMRWidget::MakeBox(const FSlateBrush& Brush, const FMargin& InPadding, const FName Name)
{
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Box->SetBrush(Brush);
	Box->SetPadding(InPadding);
	Box->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	return Box;
}

UButton* UMuseumMRWidget::MakeButton(const FName Name, const FLinearColor& Fill, const float Radius, const FMargin& InPadding, const FLinearColor& Outline)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	const FLinearColor& Cyan = MuseumMR::Cyan();
	FButtonStyle Style;
	Style.SetNormal(MuseumMR::RoundedBrush(Fill, Radius, Outline, Outline.A > 0.f ? 2.f : 0.f));
	Style.SetHovered(MuseumMR::RoundedBrush(Fill + Cyan * FLinearColor(0.12f, 0.12f, 0.12f, 0.f), Radius, Cyan, 3.f));
	Style.SetPressed(MuseumMR::RoundedBrush(Cyan * FLinearColor(0.35f, 0.35f, 0.35f, 0.75f), Radius, Cyan, 3.f));
	Style.SetDisabled(MuseumMR::RoundedBrush(Fill, Radius));
	Style.SetNormalPadding(InPadding);
	Style.SetPressedPadding(InPadding);
	Button->SetStyle(Style);

	return Button;
}

UVerticalBox* UMuseumMRWidget::MakeColumn()
{
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	return Column;
}

UHorizontalBox* UMuseumMRWidget::MakeRow()
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	return Row;
}

USizeBox* UMuseumMRWidget::MakeSize(const float Width, const float Height)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (Width > 0.f) Box->SetWidthOverride(Width);
	if (Height > 0.f) Box->SetHeightOverride(Height);
	Box->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	return Box;
}

UImage* UMuseumMRWidget::MakeImage(const FSlateBrush& Brush)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Image->SetBrush(Brush);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);

	return Image;
}

UWidget* UMuseumMRWidget::MakePlayMark(const float Height, const FLinearColor& Color)
{
	// The right half of a square turned 45 degrees, centred on the box's left edge and clipped by the box
	USizeBox* Box = MakeSize(Height * 0.5f, Height);
	Box->SetClipping(EWidgetClipping::ClipToBounds);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	Box->AddChild(Canvas);

	const float Side = Height / UE_SQRT_2;
	FSlateBrush Brush;
	Brush.TintColor = FSlateColor(Color);
	Brush.ImageSize = FVector2D(Side, Side);
	UImage* Square = MakeImage(Brush);
	Square->SetRenderTransformAngle(45.f);
	Square->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* SquareSlot = Canvas->AddChildToCanvas(Square);
	SquareSlot->SetPosition(FVector2D(-Side * 0.5f, (Height - Side) * 0.5f));
	SquareSlot->SetSize(FVector2D(Side, Side));

	return Box;
}

UWidget* UMuseumMRWidget::MakeFittedText(const FText& Text, const MuseumMR::EFace Face, const int32 Size, const FLinearColor& Color)
{
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetStretchDirection(EStretchDirection::DownOnly);
	Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UScaleBoxSlot* FitSlot = Cast<UScaleBoxSlot>(Fit->AddChild(MakeText(Text, Face, Size, Color)))) FitSlot->SetHorizontalAlignment(HAlign_Left);

	return Fit;
}

UImage* UMuseumMRWidget::MakeDivider(const FLinearColor& Color, const float Thickness)
{
	FSlateBrush Brush;
	Brush.TintColor = FSlateColor(Color);
	Brush.ImageSize = FVector2D(Thickness, Thickness);
	UImage* Line = MakeImage(Brush);
	Line->SetDesiredSizeOverride(FVector2D(1., Thickness));

	return Line;
}
