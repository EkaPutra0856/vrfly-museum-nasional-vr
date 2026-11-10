#pragma once

#include "CoreMinimal.h"
#include "MR/MuseumMRWidget.h"
#include "MuseumMRInfoCardWidget.generated.h"

class UScrollBox;
class UTexture;

/** How one information card shows. */
UENUM()
enum class EMuseumInfoCardMode : uint8
{
	/** A slim glass bar: number, heading, title and "+" (the whole bar opens it). */
	Minimized,
	/** The open card: its photo or video, the text in a scroll box with a visible bar, Suara and ID/EN buttons, "—". */
	Expanded,
};

/** What one card holds. */
UENUM()
enum class EMuseumInfoCardKind : uint8
{
	/** The whole text. */
	Text,
	/** A supporting photo above a shorter text. */
	Image,
	/** The object's video, with play / pause and "Perbesar" (the large 3D screen). */
	Video,
};

/** Everything one card shows. */
struct FMuseumInfoCardView
{
	int32 Index = 0;
	EMuseumInfoCardMode Mode = EMuseumInfoCardMode::Minimized;
	EMuseumInfoCardKind Kind = EMuseumInfoCardKind::Text;
	FText Heading;
	FText Title;
	FText Body;
	/** The photo (Image) or the media texture (Video). */
	UTexture* Picture = nullptr;
	bool bHeard = false;
	bool bVoicePlaying = false;
	bool bVideoPlaying = false;
	bool bEnglish = false;
};

DECLARE_DELEGATE_OneParam(FMuseumInfoCardIndexDelegate, int32 /*Index*/);

/**
 * One information card of an exhibit, drawn on a world widget in front of a 3D glass slab (AMuseumExhibit). The whole
 * minimized bar is one button (HeaderButton<N>) that opens it; the open card closes with its "—" button (also
 * HeaderButton<N>), reads its text through VoiceButton and switches the tour language with LanguageButton. The text
 * widget is named CardBody. A video card has VideoPlayButton and VideoEnlargeButton instead.
 */
UCLASS()
class UMuseumMRInfoCardWidget : public UMuseumMRWidget
{
	GENERATED_BODY()

public:
	// ====== API ====== //

	/** Widget pixels of a card (the exhibit sizes the component and its slab from these). */
	static FVector2D GetDrawSize(EMuseumInfoCardMode Mode, EMuseumInfoCardKind Kind);

	/** Shows the view; rebuilds only when something changed, the voice button alone is relabelled in place. */
	void SetView(const FMuseumInfoCardView& InView);

	/** Scrolls the open card's text back to its start. */
	void ScrollToStart();

	FMuseumInfoCardIndexDelegate OnToggle;
	FSimpleDelegate OnVoice;
	FSimpleDelegate OnLanguage;
	FSimpleDelegate OnVideoPlay;
	FSimpleDelegate OnVideoEnlarge;

protected:
	// ====== Build ====== //

	virtual void BuildContent() override;

private:
	void BuildMinimized();
	void BuildExpanded();

	UFUNCTION()
	void HandleToggleClicked();

	UFUNCTION()
	void HandleVoiceClicked();

	UFUNCTION()
	void HandleLanguageClicked();

	UFUNCTION()
	void HandleVideoPlayClicked();

	UFUNCTION()
	void HandleVideoEnlargeClicked();

	FMuseumInfoCardView View;
	bool bHasView = false;

	/** Keeps View.Picture alive: the photo is loaded on demand and the view holds it only as a plain pointer (the brush too). */
	UPROPERTY(Transient)
	TObjectPtr<UTexture> HeldPicture;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Scroll;
};

/** Everything the large video screen shows. */
struct FMuseumVideoScreenView
{
	FText Title;
	UTexture* Video = nullptr;
	bool bPlaying = false;
	bool bEnglish = false;
};

/**
 * The large 3D video screen (about 70% of the view) opened by a video card's "Perbesar": the video, its title,
 * TheaterPlayButton and the close button TheaterCloseButton.
 */
UCLASS()
class UMuseumMRVideoScreenWidget : public UMuseumMRWidget
{
	GENERATED_BODY()

public:
	// ====== API ====== //

	static FVector2D GetDrawSize();

	void SetView(const FMuseumVideoScreenView& InView);

	FSimpleDelegate OnPlay;
	FSimpleDelegate OnClose;

protected:
	// ====== Build ====== //

	virtual void BuildContent() override;

private:
	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleCloseClicked();

	FMuseumVideoScreenView View;
	bool bHasView = false;
};
