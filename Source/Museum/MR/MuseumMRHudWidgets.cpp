#include "MR/MuseumMRHudWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "MuseumMRHud"

using namespace MuseumMR;

namespace
{
	FLinearColor WithAlpha(const FLinearColor& Color, const float Alpha)
	{
		return Color.CopyWithNewOpacity(Alpha);
	}
}

// ====== HUD bar ====== //

void UMuseumMRHudBarWidget::SetView(const FMuseumHudBarView& InView)
{
	if (bHasView && View == InView) return;

	View = InView;
	bHasView = true;
	Rebuild();
}

void UMuseumMRHudBarWidget::BuildContent()
{
	if (!bHasView) return;

	// One glass pill across the curve
	UBorder* Bar = MakeBox(RoundedBrush(GlassFill(), 110.f, WithAlpha(Cyan(), 0.55f), 2.5f), FMargin(18.f, 14.f, 50.f, 14.f));
	Root->AddChildToOverlay(Bar);
	UHorizontalBox* Row = MakeRow();
	Bar->AddChild(Row);

	// The minimized map: a disc with the tour count
	UButton* Map = MakeButton(TEXT("MinimapButton"), View.bMapOpen ? WithAlpha(Cyan(), 0.3f) : FLinearColor(0.01f, 0.03f, 0.04f, 0.9f), 100.f, FMargin(0.f), Cyan());
	Map->OnClicked.AddDynamic(this, &UMuseumMRHudBarWidget::HandleMinimapClicked);
	USizeBox* MapSize = MakeSize(200.f, 200.f);
	MapSize->AddChild(Map);
	UVerticalBox* MapColumn = MakeColumn();
	Map->AddChild(MapColumn);
	int32 Seen = 0;
	for (const bool bVisited : View.Visited) Seen += bVisited ? 1 : 0;
	UTextBlock* MapLabel = MakeText(View.bEnglish ? LOCTEXT("MapEN", "MAP") : LOCTEXT("MapID", "PETA"), EFace::LabelBold, 44, Cyan());
	MapColumn->AddChildToVerticalBox(MapLabel)->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* MapCount = MakeText(FText::FromString(FString::Printf(TEXT("%d/%d"), Seen, View.Visited.Num())), EFace::LabelBold, 34, Text());
	MapColumn->AddChildToVerticalBox(MapCount)->SetHorizontalAlignment(HAlign_Center);
	Row->AddChildToHorizontalBox(MapSize)->SetVerticalAlignment(VAlign_Center);

	// NAVIGASI
	UButton* Navigate = MakeButton(TEXT("NavigateButton"), View.bGuiding ? WithAlpha(Cyan(), 0.22f) : WithAlpha(Cyan(), 0.16f), 70.f, FMargin(46.f, 16.f), Cyan());
	Navigate->OnClicked.AddDynamic(this, &UMuseumMRHudBarWidget::HandleNavigateClicked);
	UHorizontalBox* NavigateRow = MakeRow();
	Navigate->AddChild(NavigateRow);
	NavigateRow->AddChildToHorizontalBox(MakeText(FText::FromString(TEXT("→")), EFace::Title, 54, Cyan()))->SetVerticalAlignment(VAlign_Center);
	UVerticalBox* NavigateTexts = MakeColumn();
	UHorizontalBoxSlot* TextsSlot = NavigateRow->AddChildToHorizontalBox(NavigateTexts);
	TextsSlot->SetVerticalAlignment(VAlign_Center);
	TextsSlot->SetPadding(FMargin(22.f, 0.f, 0.f, 0.f));
	NavigateTexts->AddChildToVerticalBox(MakeText(View.NavigateText, EFace::LabelBold, 50, Text()));
	NavigateTexts->AddChildToVerticalBox(MakeText(View.NavigateDetail, EFace::Medium, 28, View.bGuiding ? Cyan() : MutedText()));
	UHorizontalBoxSlot* NavigateSlot = Row->AddChildToHorizontalBox(Navigate);
	NavigateSlot->SetVerticalAlignment(VAlign_Center);
	NavigateSlot->SetPadding(FMargin(28.f, 0.f, 0.f, 0.f));

	// Tour progress: "Tur n/6" over one dot per stop
	int32 Count = 0;
	for (const bool bVisited : View.Visited) Count += bVisited ? 1 : 0;
	UVerticalBox* Tour = MakeColumn();
	UHorizontalBoxSlot* TourSlot = Row->AddChildToHorizontalBox(Tour);
	TourSlot->SetVerticalAlignment(VAlign_Center);
	TourSlot->SetPadding(FMargin(46.f, 0.f, 0.f, 0.f));
	const FText TourText = FText::Format(View.bEnglish ? LOCTEXT("TourEN", "Tour {0}/{1}") : LOCTEXT("TourID", "Tur {0}/{1}"), Count, View.Visited.Num());
	Tour->AddChildToVerticalBox(MakeText(TourText, EFace::LabelBold, 46, Text()));
	UHorizontalBox* Dots = MakeRow();
	Tour->AddChildToVerticalBox(Dots)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	for (const bool bVisited : View.Visited)
	{
		USizeBox* Dot = MakeSize(24.f, 24.f);
		Dot->AddChild(MakeImage(RoundedBrush(bVisited ? Green() : FLinearColor(0.f, 0.f, 0.f, 0.3f), 12.f, bVisited ? Green() : WithAlpha(Cyan(), 0.8f), 2.5f)));
		Dots->AddChildToHorizontalBox(Dot)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}

	// Separator, then the message
	USizeBox* Rule = MakeSize(3.f, 130.f);
	Rule->AddChild(MakeImage(RoundedBrush(WithAlpha(Cyan(), 0.35f), 1.f)));
	UHorizontalBoxSlot* RuleSlot = Row->AddChildToHorizontalBox(Rule);
	RuleSlot->SetVerticalAlignment(VAlign_Center);
	RuleSlot->SetPadding(FMargin(46.f, 0.f, 46.f, 0.f));

	// The message shrinks to the room left, so a long exhibit name ("Menuju Maket Miniatur Arsitektur Masjid At-Thohir") stays whole
	UScaleBox* MessageFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	MessageFit->SetStretch(EStretch::ScaleToFit);
	MessageFit->SetStretchDirection(EStretchDirection::DownOnly);
	UHorizontalBoxSlot* MessageSlot = Row->AddChildToHorizontalBox(MessageFit);
	MessageSlot->SetVerticalAlignment(VAlign_Center);
	MessageSlot->SetHorizontalAlignment(HAlign_Left);
	MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Message = MakeColumn();
	MessageFit->AddChild(Message);
	Message->AddChildToVerticalBox(MakeText(View.Message, EFace::Title, 44, Text()));
	Message->AddChildToVerticalBox(MakeText(View.MessageDetail, EFace::Body, 28, MutedText()))->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
}

void UMuseumMRHudBarWidget::HandleNavigateClicked()
{
	OnNavigate.ExecuteIfBound();
}

void UMuseumMRHudBarWidget::HandleMinimapClicked()
{
	OnMinimap.ExecuteIfBound();
}

// ====== Destination menu ====== //

void UMuseumMRMenuWidget::SetPlaces(const TArray<FMuseumMenuPlace>& InPlaces, const bool bInEnglish)
{
	Places = InPlaces;
	bEnglish = bInEnglish;
	Rebuild();
}

void UMuseumMRMenuWidget::BuildContent()
{
	UBorder* Panel = MakeBox(RoundedBrush(GlassFill(), 54.f, WithAlpha(Cyan(), 0.7f), 3.f), FMargin(64.f, 48.f));
	Root->AddChildToOverlay(Panel);
	UVerticalBox* Column = MakeColumn();
	Panel->AddChild(Column);

	// Header: title, then language and close
	UHorizontalBox* Header = MakeRow();
	Column->AddChildToVerticalBox(Header);
	UVerticalBox* Titles = MakeColumn();
	UHorizontalBoxSlot* TitlesSlot = Header->AddChildToHorizontalBox(Titles);
	TitlesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitlesSlot->SetVerticalAlignment(VAlign_Center);
	Titles->AddChildToVerticalBox(MakeText(bEnglish ? LOCTEXT("WhereEN", "Where to?") : LOCTEXT("WhereID", "Mau ke mana?"), EFace::Title, 76, Text()));
	const FText Sub = FText::Format(bEnglish ? LOCTEXT("SubEN", "Choose a collection · {0} destinations · point and press") : LOCTEXT("SubID", "Pilih koleksi · {0} tujuan · arahkan lalu tekan"), Places.Num());
	Titles->AddChildToVerticalBox(MakeText(Sub, EFace::Body, 32, MutedText()))->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	UButton* Language = MakeButton(TEXT("LanguageButton"), GlassFill(), 26.f, FMargin(30.f, 16.f), WithAlpha(Cyan(), 0.7f));
	Language->AddChild(MakeText(bEnglish ? LOCTEXT("LangEN", "ID  |  EN ●") : LOCTEXT("LangID", "● ID  |  EN"), EFace::Title, 32, Text()));
	Language->OnClicked.AddDynamic(this, &UMuseumMRMenuWidget::HandleLanguageClicked);
	Header->AddChildToHorizontalBox(Language)->SetVerticalAlignment(VAlign_Center);

	UButton* Close = MakeButton(TEXT("MenuCloseButton"), GlassFill(), 40.f, FMargin(30.f, 8.f), WithAlpha(Cyan(), 0.7f));
	Close->AddChild(MakeText(FText::FromString(TEXT("×")), EFace::Title, 52, Cyan()));
	Close->OnClicked.AddDynamic(this, &UMuseumMRMenuWidget::HandleCloseClicked);
	UHorizontalBoxSlot* CloseSlot = Header->AddChildToHorizontalBox(Close);
	CloseSlot->SetVerticalAlignment(VAlign_Center);
	CloseSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));

	// 3 x 2 cards
	for (int32 RowIndex = 0; RowIndex * 3 < Places.Num(); ++RowIndex)
	{
		UHorizontalBox* Row = MakeRow();
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row);
		RowSlot->SetPadding(FMargin(0.f, RowIndex == 0 ? 40.f : 32.f, 0.f, 0.f));
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		for (int32 Index = RowIndex * 3; Index < FMath::Min(Places.Num(), RowIndex * 3 + 3); ++Index)
		{
			UHorizontalBoxSlot* CardSlot = Row->AddChildToHorizontalBox(BuildCard(Places[Index]));
			CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CardSlot->SetPadding(FMargin(Index % 3 == 0 ? 0.f : 18.f, 0.f, Index % 3 == 2 ? 0.f : 18.f, 0.f));
		}
	}
}

UWidget* UMuseumMRMenuWidget::BuildCard(const FMuseumMenuPlace& Place)
{
	const FLinearColor Edge = Place.bActive ? Cyan() : WithAlpha(Cyan(), 0.35f);
	UButton* Card = MakeButton(*FString::Printf(TEXT("Place%d"), Place.Number), FLinearColor(0.02f, 0.05f, 0.07f, 0.85f), 30.f, FMargin(0.f), Edge);
	Card->OnClicked.AddDynamic(this, &UMuseumMRMenuWidget::HandlePlaceClicked);
	UVerticalBox* Column = MakeColumn();
	Card->AddChild(Column);

	// Picture, filled and cropped, with the number badge
	UOverlay* Picture = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Picture->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	USizeBox* PictureSize = MakeSize(0.f, 330.f);
	PictureSize->AddChild(Picture);
	Column->AddChildToVerticalBox(PictureSize)->SetPadding(FMargin(10.f, 10.f, 10.f, 0.f));
	if (Place.Thumbnail)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Place.Thumbnail);
		Brush.ImageSize = FVector2D(Place.Thumbnail->GetSizeX(), Place.Thumbnail->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::Image;
		UScaleBox* Crop = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		Crop->SetStretch(EStretch::ScaleToFill);
		Crop->SetVisibility(ESlateVisibility::HitTestInvisible);
		Crop->AddChild(MakeImage(Brush));
		UOverlaySlot* CropSlot = Picture->AddChildToOverlay(Crop);
		CropSlot->SetHorizontalAlignment(HAlign_Fill);
		CropSlot->SetVerticalAlignment(VAlign_Fill);
	}
	else
	{
		Picture->AddChildToOverlay(MakeImage(RoundedBrush(FLinearColor(0.03f, 0.08f, 0.1f, 1.f), 20.f)));
	}
	UBorder* Badge = MakeBox(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.7f), 34.f, Cyan(), 2.5f), FMargin(22.f, 4.f));
	Badge->AddChild(MakeText(FText::FromString(FString::Printf(TEXT("%02d"), Place.Number)), EFace::LabelBold, 40, Cyan()));
	UOverlaySlot* BadgeSlot = Picture->AddChildToOverlay(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Left);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(FMargin(18.f));
	if (Place.bVisited)
	{
		UBorder* Check = MakeBox(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.7f), 34.f, Green(), 2.5f), FMargin(20.f, 4.f));
		Check->AddChild(MakeText(bEnglish ? LOCTEXT("VisitedEN", "✓ Visited") : LOCTEXT("VisitedID", "✓ Dikunjungi"), EFace::Title, 30, Green()));
		UOverlaySlot* CheckSlot = Picture->AddChildToOverlay(Check);
		CheckSlot->SetHorizontalAlignment(HAlign_Right);
		CheckSlot->SetVerticalAlignment(VAlign_Top);
		CheckSlot->SetPadding(FMargin(18.f));
	}

	// Name and distance
	UVerticalBox* Texts = MakeColumn();
	Column->AddChildToVerticalBox(Texts)->SetPadding(FMargin(30.f, 20.f, 30.f, 24.f));
	UTextBlock* Name = MakeText(Place.Name, EFace::Title, 44, Text());
	Name->SetAutoWrapText(true);
	Texts->AddChildToVerticalBox(Name);
	const FText Distance = Place.bActive ? FText::Format(bEnglish ? LOCTEXT("ActiveEN", "● Guiding · {0}") : LOCTEXT("ActiveID", "● Dipandu · {0}"), Place.Distance)
		: Place.Distance.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("Distance", "{0}"), Place.Distance);
	Texts->AddChildToVerticalBox(MakeText(Distance, EFace::Medium, 30, Place.bActive ? Cyan() : MutedText()))->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

	return Card;
}

void UMuseumMRMenuWidget::HandleCloseClicked()
{
	OnClose.ExecuteIfBound();
}

void UMuseumMRMenuWidget::HandleLanguageClicked()
{
	OnLanguage.ExecuteIfBound();
}

void UMuseumMRMenuWidget::HandlePlaceClicked()
{
	// The button under the pointer is the one clicked
	for (const FMuseumMenuPlace& Place : Places)
	{
		const UButton* Button = WidgetTree ? Cast<UButton>(WidgetTree->FindWidget(*FString::Printf(TEXT("Place%d"), Place.Number))) : nullptr;
		if (Button && (Button->IsHovered() || Button->IsPressed()))
		{
			OnSelect.ExecuteIfBound(Place.PlaceTag);

			return;
		}
	}
}

// ====== Map header ====== //

void UMuseumMRMapHeaderWidget::SetCount(const int32 InVisited, const int32 InTotal, const bool bInEnglish)
{
	if (InVisited == Visited && InTotal == Total && bInEnglish == bEnglish) return;

	Visited = InVisited;
	Total = InTotal;
	bEnglish = bInEnglish;
	Rebuild();
}

void UMuseumMRMapHeaderWidget::BuildContent()
{
	UBorder* Bar = MakeBox(RoundedBrush(GlassFill(), 70.f, WithAlpha(Cyan(), 0.6f), 2.5f), FMargin(46.f, 12.f, 14.f, 12.f));
	Root->AddChildToOverlay(Bar);
	UHorizontalBox* Row = MakeRow();
	Bar->AddChild(Row);

	Row->AddChildToHorizontalBox(MakeText(bEnglish ? LOCTEXT("MapTitleEN", "TOUR MAP") : LOCTEXT("MapTitleID", "PETA TUR"), EFace::LabelBold, 50, Cyan()))->SetVerticalAlignment(VAlign_Center);
	const FText Count = FText::Format(bEnglish ? LOCTEXT("CountEN", "  ·  {0}/{1} visited") : LOCTEXT("CountID", "  ·  {0}/{1} dikunjungi"), FMath::Max(Visited, 0), Total);
	UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(MakeText(Count, EFace::Medium, 40, Text()));
	CountSlot->SetVerticalAlignment(VAlign_Center);
	CountSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UButton* Minimize = MakeButton(TEXT("MapMinimizeButton"), GlassFill(), 50.f, FMargin(34.f, 0.f), Cyan());
	Minimize->AddChild(MakeText(FText::FromString(TEXT("–")), EFace::LabelBold, 70, Cyan()));
	Minimize->OnClicked.AddDynamic(this, &UMuseumMRMapHeaderWidget::HandleMinimizeClicked);
	Row->AddChildToHorizontalBox(Minimize)->SetVerticalAlignment(VAlign_Center);
}

void UMuseumMRMapHeaderWidget::HandleMinimizeClicked()
{
	OnMinimize.ExecuteIfBound();
}

// ====== Map pop-up ====== //

void UMuseumMRMapConfirmWidget::SetPlace(const int32 InNumber, const FText& InName, UTexture2D* InThumbnail, const bool bInEnglish)
{
	Number = InNumber;
	Name = InName;
	Thumbnail = InThumbnail;
	bEnglish = bInEnglish;
	Rebuild();
}

void UMuseumMRMapConfirmWidget::BuildContent()
{
	if (Number == 0) return;

	UBorder* Panel = MakeBox(RoundedBrush(GlassFill(), 48.f, WithAlpha(Cyan(), 0.8f), 3.f), FMargin(36.f));
	Root->AddChildToOverlay(Panel);
	UHorizontalBox* Row = MakeRow();
	Panel->AddChild(Row);

	// The stop's picture with its number
	UOverlay* Picture = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Picture->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	USizeBox* PictureSize = MakeSize(400.f, 0.f);
	PictureSize->AddChild(Picture);
	UHorizontalBoxSlot* PictureSlot = Row->AddChildToHorizontalBox(PictureSize);
	PictureSlot->SetVerticalAlignment(VAlign_Fill);
	if (Thumbnail)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Thumbnail);
		Brush.ImageSize = FVector2D(Thumbnail->GetSizeX(), Thumbnail->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::Image;
		UScaleBox* Crop = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		Crop->SetStretch(EStretch::ScaleToFill);
		Crop->SetVisibility(ESlateVisibility::HitTestInvisible);
		Crop->AddChild(MakeImage(Brush));
		UOverlaySlot* CropSlot = Picture->AddChildToOverlay(Crop);
		CropSlot->SetHorizontalAlignment(HAlign_Fill);
		CropSlot->SetVerticalAlignment(VAlign_Fill);
	}
	else
	{
		Picture->AddChildToOverlay(MakeImage(RoundedBrush(FLinearColor(0.03f, 0.08f, 0.1f, 1.f), 24.f)));
	}
	UBorder* Badge = MakeBox(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.7f), 34.f, Cyan(), 2.5f), FMargin(22.f, 4.f));
	Badge->AddChild(MakeText(FText::FromString(FString::Printf(TEXT("%02d"), Number)), EFace::LabelBold, 40, Cyan()));
	UOverlaySlot* BadgeSlot = Picture->AddChildToOverlay(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Left);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(FMargin(16.f));

	// The question, the stop, the answers
	UVerticalBox* Column = MakeColumn();
	UHorizontalBoxSlot* ColumnSlot = Row->AddChildToHorizontalBox(Column);
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnSlot->SetPadding(FMargin(40.f, 6.f, 8.f, 6.f));
	Column->AddChildToVerticalBox(MakeText(bEnglish ? LOCTEXT("ConfirmKickerEN", "NAVIGATION") : LOCTEXT("ConfirmKickerID", "NAVIGASI"), EFace::LabelBold, 30, Cyan()));
	UTextBlock* Question = MakeText(bEnglish ? LOCTEXT("ConfirmEN", "Start navigation to this destination?") : LOCTEXT("ConfirmID", "Aktifkan navigasi ke destinasi ini?"), EFace::Title, 50, Text(), true);
	Column->AddChildToVerticalBox(Question)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	UTextBlock* Place = MakeText(Name, EFace::Medium, 38, Gold(), true);
	Column->AddChildToVerticalBox(Place)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));

	UVerticalBoxSlot* SpacerSlot = Column->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBox* Answers = MakeRow();
	Column->AddChildToVerticalBox(Answers);
	UButton* Yes = MakeButton(TEXT("MapConfirmYesButton"), WithAlpha(Cyan(), 0.28f), 44.f, FMargin(40.f, 16.f), Cyan());
	Yes->AddChild(MakeText(bEnglish ? LOCTEXT("YesEN", "→  Yes, guide me") : LOCTEXT("YesID", "→  Ya, pandu saya"), EFace::LabelBold, 42, Text()));
	Yes->OnClicked.AddDynamic(this, &UMuseumMRMapConfirmWidget::HandleYesClicked);
	Answers->AddChildToHorizontalBox(Yes)->SetVerticalAlignment(VAlign_Center);
	UButton* No = MakeButton(TEXT("MapConfirmNoButton"), GlassFill(), 44.f, FMargin(40.f, 16.f), WithAlpha(Cyan(), 0.6f));
	No->AddChild(MakeText(bEnglish ? LOCTEXT("NoEN", "Cancel") : LOCTEXT("NoID", "Batal"), EFace::LabelBold, 42, MutedText()));
	No->OnClicked.AddDynamic(this, &UMuseumMRMapConfirmWidget::HandleNoClicked);
	UHorizontalBoxSlot* NoSlot = Answers->AddChildToHorizontalBox(No);
	NoSlot->SetVerticalAlignment(VAlign_Center);
	NoSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
}

void UMuseumMRMapConfirmWidget::HandleYesClicked()
{
	OnYes.ExecuteIfBound();
}

void UMuseumMRMapConfirmWidget::HandleNoClicked()
{
	OnNo.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
