#include "MR/MuseumMRInfoCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture.h"

#define LOCTEXT_NAMESPACE "MuseumMRInfoCard"

using namespace MuseumMR;

namespace
{
	const FVector2D MinimizedSize(1000., 170.);
	const FVector2D TextSize(1150., 1150.);
	const FVector2D ImageSize(1760., 740.);
	const FVector2D VideoSize(1280., 1080.);
	const FVector2D ScreenSize(2060., 1300.);

	/** Photo (left of the text) and video areas inside the open cards and the screen (16:9, like the recordings). */
	const FVector2D PhotoArea(960., 540.);
	const FVector2D VideoArea(1192., 670.);
	const FVector2D ScreenVideoArea(1960., 1102.);

	FText NumberText(const int32 Index)
	{
		return FText::FromString(FString::Printf(TEXT("%02d"), Index + 1));
	}

	FText VoiceLabel(const FMuseumInfoCardView& View)
	{
		return View.bVoicePlaying ? LOCTEXT("VoiceStop", "■  Suara") : (View.bEnglish ? LOCTEXT("VoicePlayEN", "▶  Audio") : LOCTEXT("VoicePlayID", "▶  Suara"));
	}

	FText PlayLabel(const bool bPlaying, const bool bEnglish)
	{
		if (bPlaying) return bEnglish ? LOCTEXT("PauseEN", "■  Pause") : LOCTEXT("PauseID", "■  Jeda");

		return bEnglish ? LOCTEXT("PlayEN", "Play") : LOCTEXT("PlayID", "Putar");
	}

	/** A texture drawn with rounded corners at a size. */
	FSlateBrush PictureBrush(UTexture* Picture, const FVector2D& Size, const float Radius)
	{
		FSlateBrush Brush = RoundedBrush(FLinearColor::White, Radius);
		Brush.SetResourceObject(Picture);
		Brush.ImageSize = Size;

		return Brush;
	}

	FLinearColor BackingColor(const float Alpha)
	{
		return FLinearColor(0.01f, 0.018f, 0.028f, Alpha);
	}

	bool SameView(const FMuseumInfoCardView& A, const FMuseumInfoCardView& B)
	{
		return A.Mode == B.Mode && A.Kind == B.Kind && A.Index == B.Index && A.bEnglish == B.bEnglish && A.bHeard == B.bHeard
			&& A.bVideoPlaying == B.bVideoPlaying && A.Picture == B.Picture && A.Title.EqualTo(B.Title);
	}
}

// ====== API ====== //

FVector2D UMuseumMRInfoCardWidget::GetDrawSize(const EMuseumInfoCardMode Mode, const EMuseumInfoCardKind Kind)
{
	if (Mode == EMuseumInfoCardMode::Minimized) return MinimizedSize;

	switch (Kind)
	{
	case EMuseumInfoCardKind::Image: return ImageSize;
	case EMuseumInfoCardKind::Video: return VideoSize;
	default: return TextSize;
	}
}

void UMuseumMRInfoCardWidget::SetView(const FMuseumInfoCardView& InView)
{
	// Same layout and texts: nothing to rebuild (the open card keeps its scroll position)
	const bool bSame = bHasView && SameView(View, InView);
	if (bSame && View.bVoicePlaying == InView.bVoicePlaying) return;

	// Only the voice state changed on the open card: relabel the button in place
	View = InView;
	HeldPicture = InView.Picture;
	bHasView = true;
	if (bSame && View.Mode == EMuseumInfoCardMode::Expanded && WidgetTree)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("VoiceLabel"))))
		{
			Label->SetText(VoiceLabel(View));

			return;
		}
	}
	Rebuild();
}

void UMuseumMRInfoCardWidget::ScrollToStart()
{
	if (Scroll) Scroll->ScrollToStart();
}

// ====== Build ====== //

void UMuseumMRInfoCardWidget::BuildContent()
{
	Scroll = nullptr;
	if (!bHasView) return;

	// A dark tint behind the content: the glass alone washes out in front of a bright plinth or wall
	UOverlaySlot* BackingSlot = Root->AddChildToOverlay(MakeBox(RoundedBrush(BackingColor(View.Mode == EMuseumInfoCardMode::Expanded ? 0.62f : 0.5f), 10.f), FMargin(0.f)));
	BackingSlot->SetHorizontalAlignment(HAlign_Fill);
	BackingSlot->SetVerticalAlignment(VAlign_Fill);

	if (View.Mode == EMuseumInfoCardMode::Expanded) BuildExpanded();
	else BuildMinimized();
}

void UMuseumMRInfoCardWidget::BuildMinimized()
{
	// The whole bar opens it
	UButton* Button = MakeButton(*FString::Printf(TEXT("HeaderButton%d"), View.Index), FLinearColor(0.f, 0.f, 0.f, 0.f), 26.f, FMargin(34.f, 18.f));
	Button->OnClicked.AddDynamic(this, &UMuseumMRInfoCardWidget::HandleToggleClicked);
	UOverlaySlot* ButtonSlot = Root->AddChildToOverlay(Button);
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	ButtonSlot->SetVerticalAlignment(VAlign_Fill);

	UHorizontalBox* Row = MakeRow();
	Button->AddChild(Row);

	// Number chip (a play mark on the video card)
	const bool bVideo = View.Kind == EMuseumInfoCardKind::Video;
	UBorder* Chip = MakeBox(RoundedBrush(bVideo ? Cyan() * FLinearColor(0.3f, 0.3f, 0.3f, 0.6f) : FLinearColor(0.f, 0.f, 0.f, 0.f), 14.f, Cyan(), 2.f), bVideo ? FMargin(22.f, 10.f, 16.f, 10.f) : FMargin(16.f, 4.f));
	Chip->AddChild(bVideo ? MakePlayMark(30.f, Cyan()) : MakeText(NumberText(View.Index), EFace::LabelBold, 34, Cyan()));
	Row->AddChildToHorizontalBox(Chip)->SetVerticalAlignment(VAlign_Center);

	// Heading over the title, cut at the bar's end
	UVerticalBox* Column = MakeColumn();
	UHorizontalBoxSlot* ColumnSlot = Row->AddChildToHorizontalBox(Column);
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
	ColumnSlot->SetPadding(FMargin(26.f, 0.f, 18.f, 0.f));
	Column->AddChildToVerticalBox(MakeFittedText(View.Heading, EFace::Label, 26, Gold()));
	Column->AddChildToVerticalBox(MakeFittedText(View.Title, EFace::Title, 42, Text()))->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	// Open mark: a glass circle with "+", "✓" once heard
	UBorder* Mark = MakeBox(RoundedBrush(GlassFill(), 32.f, View.bHeard ? Green() : Cyan(), 2.f), FMargin(18.f, 2.f));
	Mark->AddChild(MakeText(FText::FromString(View.bHeard ? TEXT("✓") : TEXT("+")), EFace::Title, 44, View.bHeard ? Green() : Cyan()));
	Row->AddChildToHorizontalBox(Mark)->SetVerticalAlignment(VAlign_Center);
}

void UMuseumMRInfoCardWidget::BuildExpanded()
{
	UVerticalBox* Column = MakeColumn();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	ColumnSlot->SetPadding(FMargin(44.f, 40.f, 44.f, 36.f));

	// Header: number and heading, then ID/EN, Suara (cards with a voice-over) and "—"
	const bool bVideo = View.Kind == EMuseumInfoCardKind::Video;
	UHorizontalBox* Header = MakeRow();
	Column->AddChildToVerticalBox(Header);
	UBorder* Chip = MakeBox(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.f), 14.f, Cyan(), 2.f), bVideo ? FMargin(22.f, 10.f, 16.f, 10.f) : FMargin(16.f, 4.f));
	Chip->AddChild(bVideo ? MakePlayMark(30.f, Cyan()) : MakeText(NumberText(View.Index), EFace::LabelBold, 34, Cyan()));
	Header->AddChildToHorizontalBox(Chip)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* HeadingSlot = Header->AddChildToHorizontalBox(MakeText(View.Heading, EFace::Label, 32, Gold(), true));
	HeadingSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HeadingSlot->SetVerticalAlignment(VAlign_Center);
	HeadingSlot->SetPadding(FMargin(22.f, 0.f, 16.f, 0.f));

	UButton* Language = MakeButton(TEXT("LanguageButton"), GlassFill(), 18.f, FMargin(20.f, 10.f), Cyan() * FLinearColor(1.f, 1.f, 1.f, 0.6f));
	Language->AddChild(MakeText(View.bEnglish ? LOCTEXT("LangEN", "ID  |  EN ●") : LOCTEXT("LangID", "● ID  |  EN"), EFace::Title, 26, Text()));
	Language->OnClicked.AddDynamic(this, &UMuseumMRInfoCardWidget::HandleLanguageClicked);
	Header->AddChildToHorizontalBox(Language)->SetVerticalAlignment(VAlign_Center);

	if (!bVideo)
	{
		UButton* Voice = MakeButton(TEXT("VoiceButton"), Cyan() * FLinearColor(0.25f, 0.25f, 0.25f, 0.6f), 18.f, FMargin(22.f, 10.f), Cyan());
		Voice->AddChild(MakeText(VoiceLabel(View), EFace::Title, 26, Text(), false, TEXT("VoiceLabel")));
		Voice->OnClicked.AddDynamic(this, &UMuseumMRInfoCardWidget::HandleVoiceClicked);
		Header->AddChildToHorizontalBox(Voice)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
	}

	UButton* Close = MakeButton(*FString::Printf(TEXT("HeaderButton%d"), View.Index), GlassFill(), 18.f, FMargin(24.f, 4.f), Cyan() * FLinearColor(1.f, 1.f, 1.f, 0.6f));
	Close->AddChild(MakeText(FText::FromString(TEXT("—")), EFace::Title, 40, Cyan()));
	Close->OnClicked.AddDynamic(this, &UMuseumMRInfoCardWidget::HandleToggleClicked);
	Header->AddChildToHorizontalBox(Close)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));

	// Video card: the video with a play mark while paused, the controls, one line under it
	if (bVideo)
	{
		UOverlay* Screen = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Screen->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Column->AddChildToVerticalBox(Screen)->SetPadding(FMargin(0.f, 28.f, 0.f, 0.f));
		USizeBox* Area = MakeSize(VideoArea.X, VideoArea.Y);
		Area->AddChild(MakeImage(View.Picture ? PictureBrush(View.Picture, VideoArea, 18.f) : RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.6f), 18.f)));
		Screen->AddChildToOverlay(Area);
		if (!View.bVideoPlaying)
		{
			UBorder* Mark = MakeBox(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.45f), 60.f, Cyan(), 3.f), FMargin(50.f, 30.f, 38.f, 30.f));
			Mark->AddChild(MakePlayMark(70.f, Text()));
			UOverlaySlot* MarkSlot = Screen->AddChildToOverlay(Mark);
			MarkSlot->SetHorizontalAlignment(HAlign_Center);
			MarkSlot->SetVerticalAlignment(VAlign_Center);
		}

		UHorizontalBox* Controls = MakeRow();
		Column->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0.f, 26.f, 0.f, 0.f));
		UButton* Play = MakeButton(TEXT("VideoPlayButton"), Cyan() * FLinearColor(0.25f, 0.25f, 0.25f, 0.6f), 18.f, FMargin(26.f, 12.f), Cyan());
		UHorizontalBox* PlayRow = MakeRow();
		if (!View.bVideoPlaying) PlayRow->AddChildToHorizontalBox(MakePlayMark(26.f, Text()))->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		PlayRow->AddChildToHorizontalBox(MakeText(PlayLabel(View.bVideoPlaying, View.bEnglish), EFace::Title, 30, Text()))->SetVerticalAlignment(VAlign_Center);
		Play->AddChild(PlayRow);
		Play->OnClicked.AddDynamic(this, &UMuseumMRInfoCardWidget::HandleVideoPlayClicked);
		Controls->AddChildToHorizontalBox(Play)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* TitleSlot = Controls->AddChildToHorizontalBox(MakeText(View.Title, EFace::Title, 34, Text(), true));
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetPadding(FMargin(24.f, 0.f, 16.f, 0.f));
		UButton* Enlarge = MakeButton(TEXT("VideoEnlargeButton"), GlassFill(), 18.f, FMargin(26.f, 12.f), Cyan());
		Enlarge->AddChild(MakeText(View.bEnglish ? LOCTEXT("EnlargeEN", "+  Enlarge") : LOCTEXT("EnlargeID", "+  Perbesar"), EFace::Title, 30, Cyan()));
		Enlarge->OnClicked.AddDynamic(this, &UMuseumMRInfoCardWidget::HandleVideoEnlargeClicked);
		Controls->AddChildToHorizontalBox(Enlarge)->SetVerticalAlignment(VAlign_Center);

		UTextBlock* Caption = MakeText(View.Body, EFace::Body, 30, MutedText(), true);
		Caption->SetLineHeightPercentage(1.2f);
		Column->AddChildToVerticalBox(Caption)->SetPadding(FMargin(0.f, 22.f, 0.f, 0.f));

		return;
	}

	// Photo card: a landscape card, the photo left and the text beside it
	const bool bImage = View.Kind == EMuseumInfoCardKind::Image && View.Picture;
	UVerticalBox* Words = Column;
	if (bImage)
	{
		UHorizontalBox* Row = MakeRow();
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row);
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RowSlot->SetPadding(FMargin(0.f, 28.f, 0.f, 0.f));
		USizeBox* Area = MakeSize(PhotoArea.X, PhotoArea.Y);
		Area->AddChild(MakeImage(PictureBrush(View.Picture, PhotoArea, 18.f)));
		Row->AddChildToHorizontalBox(Area)->SetVerticalAlignment(VAlign_Top);
		Words = MakeColumn();
		UHorizontalBoxSlot* WordsSlot = Row->AddChildToHorizontalBox(Words);
		WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		WordsSlot->SetPadding(FMargin(40.f, 0.f, 0.f, 0.f));
	}

	// Title and rule
	UTextBlock* Title = MakeText(View.Title, EFace::Title, bImage ? 50 : 62, Text(), true);
	Words->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, bImage ? 0.f : 34.f, 0.f, 16.f));
	USizeBox* Rule = MakeSize(140.f, 4.f);
	Rule->AddChild(MakeDivider(Gold(), 4.f));
	Words->AddChildToVerticalBox(Rule)->SetHorizontalAlignment(HAlign_Left);

	// The text, scrolled, with a bar that shows
	Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CardScroll"));
	Scroll->SetAlwaysShowScrollbar(true);
	Scroll->SetScrollbarThickness(FVector2D(14.f, 14.f));
	Scroll->SetScrollbarPadding(FMargin(18.f, 0.f, 0.f, 0.f));
	FScrollBarStyle Bar = Scroll->GetWidgetBarStyle();
	Bar.SetVerticalBackgroundImage(RoundedBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f), 7.f));
	Bar.SetNormalThumbImage(RoundedBrush(Cyan() * FLinearColor(1.f, 1.f, 1.f, 0.75f), 7.f));
	Bar.SetHoveredThumbImage(RoundedBrush(Cyan(), 7.f));
	Bar.SetDraggedThumbImage(RoundedBrush(Cyan(), 7.f));
	Scroll->SetWidgetBarStyle(Bar);
	UVerticalBoxSlot* ScrollSlot = Words->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ScrollSlot->SetPadding(FMargin(0.f, bImage ? 22.f : 30.f, 0.f, 16.f));
	UTextBlock* Body = MakeText(View.Body, EFace::Body, bImage ? 32 : 36, Text(), true, TEXT("CardBody"));
	Body->SetLineHeightPercentage(1.28f);
	Scroll->AddChild(Body);

	const FText Hint = View.bEnglish ? LOCTEXT("ReadEN", "Scroll to read  ·  —  to minimize") : LOCTEXT("ReadID", "Gulir untuk membaca  ·  —  untuk mengecilkan");
	Words->AddChildToVerticalBox(MakeText(Hint, EFace::Label, 26, MutedText()));
}

// ====== Handlers ====== //

void UMuseumMRInfoCardWidget::HandleToggleClicked()
{
	OnToggle.ExecuteIfBound(View.Index);
}

void UMuseumMRInfoCardWidget::HandleVoiceClicked()
{
	OnVoice.ExecuteIfBound();
}

void UMuseumMRInfoCardWidget::HandleLanguageClicked()
{
	OnLanguage.ExecuteIfBound();
}

void UMuseumMRInfoCardWidget::HandleVideoPlayClicked()
{
	OnVideoPlay.ExecuteIfBound();
}

void UMuseumMRInfoCardWidget::HandleVideoEnlargeClicked()
{
	OnVideoEnlarge.ExecuteIfBound();
}

// ====== Video screen ====== //

FVector2D UMuseumMRVideoScreenWidget::GetDrawSize()
{
	return ScreenSize;
}

void UMuseumMRVideoScreenWidget::SetView(const FMuseumVideoScreenView& InView)
{
	if (bHasView && View.bPlaying == InView.bPlaying && View.bEnglish == InView.bEnglish && View.Video == InView.Video && View.Title.EqualTo(InView.Title)) return;

	View = InView;
	bHasView = true;
	Rebuild();
}

void UMuseumMRVideoScreenWidget::BuildContent()
{
	if (!bHasView) return;

	UOverlaySlot* BackingSlot = Root->AddChildToOverlay(MakeBox(RoundedBrush(BackingColor(0.7f), 10.f), FMargin(0.f)));
	BackingSlot->SetHorizontalAlignment(HAlign_Fill);
	BackingSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = MakeColumn();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	ColumnSlot->SetPadding(FMargin(50.f, 36.f, 50.f, 40.f));

	// Header: video mark and title, play / pause, close
	UHorizontalBox* Header = MakeRow();
	Column->AddChildToVerticalBox(Header);
	UBorder* Chip = MakeBox(RoundedBrush(Cyan() * FLinearColor(0.3f, 0.3f, 0.3f, 0.6f), 14.f, Cyan(), 2.f), FMargin(18.f, 4.f));
	UHorizontalBox* ChipRow = MakeRow();
	ChipRow->AddChildToHorizontalBox(MakePlayMark(30.f, Cyan()))->SetVerticalAlignment(VAlign_Center);
	ChipRow->AddChildToHorizontalBox(MakeText(LOCTEXT("ScreenChip", "VIDEO"), EFace::LabelBold, 34, Cyan()))->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
	Chip->AddChild(ChipRow);
	Header->AddChildToHorizontalBox(Chip)->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(MakeFittedText(View.Title, EFace::Title, 50, Text()));
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(FMargin(26.f, 0.f, 20.f, 0.f));

	UButton* Play = MakeButton(TEXT("TheaterPlayButton"), Cyan() * FLinearColor(0.25f, 0.25f, 0.25f, 0.6f), 20.f, FMargin(28.f, 12.f), Cyan());
	UHorizontalBox* PlayRow = MakeRow();
	if (!View.bPlaying) PlayRow->AddChildToHorizontalBox(MakePlayMark(30.f, Text()))->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
	PlayRow->AddChildToHorizontalBox(MakeText(PlayLabel(View.bPlaying, View.bEnglish), EFace::Title, 34, Text()))->SetVerticalAlignment(VAlign_Center);
	Play->AddChild(PlayRow);
	Play->OnClicked.AddDynamic(this, &UMuseumMRVideoScreenWidget::HandlePlayClicked);
	Header->AddChildToHorizontalBox(Play)->SetVerticalAlignment(VAlign_Center);

	UButton* Close = MakeButton(TEXT("TheaterCloseButton"), FLinearColor(0.35f, 0.05f, 0.05f, 0.7f), 20.f, FMargin(30.f, 12.f), FLinearColor(1.f, 0.45f, 0.4f, 1.f));
	Close->AddChild(MakeText(View.bEnglish ? LOCTEXT("CloseEN", "×  Close") : LOCTEXT("CloseID", "×  Tutup"), EFace::Title, 34, Text()));
	Close->OnClicked.AddDynamic(this, &UMuseumMRVideoScreenWidget::HandleCloseClicked);
	Header->AddChildToHorizontalBox(Close)->SetPadding(FMargin(18.f, 0.f, 0.f, 0.f));

	// The video, a play mark while paused
	UOverlay* Screen = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Screen->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Column->AddChildToVerticalBox(Screen)->SetPadding(FMargin(0.f, 30.f, 0.f, 0.f));
	USizeBox* Area = MakeSize(ScreenVideoArea.X, ScreenVideoArea.Y);
	Area->AddChild(MakeImage(View.Video ? PictureBrush(View.Video, ScreenVideoArea, 22.f) : RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.6f), 22.f)));
	Screen->AddChildToOverlay(Area);
	if (!View.bPlaying)
	{
		UBorder* Mark = MakeBox(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.45f), 90.f, Cyan(), 4.f), FMargin(80.f, 50.f, 62.f, 50.f));
		Mark->AddChild(MakePlayMark(120.f, Text()));
		UOverlaySlot* MarkSlot = Screen->AddChildToOverlay(Mark);
		MarkSlot->SetHorizontalAlignment(HAlign_Center);
		MarkSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UMuseumMRVideoScreenWidget::HandlePlayClicked()
{
	OnPlay.ExecuteIfBound();
}

void UMuseumMRVideoScreenWidget::HandleCloseClicked()
{
	OnClose.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE
