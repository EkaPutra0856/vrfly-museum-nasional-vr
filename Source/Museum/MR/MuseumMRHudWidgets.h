#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MR/MuseumMRWidget.h"
#include "MuseumMRHudWidgets.generated.h"

class UTexture2D;

// ====== HUD bar ====== //

/** Everything the curved HUD bar shows (storyboard frame 1). */
struct FMuseumHudBarView
{
	/** "NAVIGASI", or the destination while guiding. */
	FText NavigateText;
	/** "Pilih tujuan", or the remaining distance while guiding. */
	FText NavigateDetail;
	bool bGuiding = false;
	/** One flag per tour stop, in tour order. */
	TArray<bool> Visited;
	FText Message;
	FText MessageDetail;
	bool bMapOpen = false;
	bool bEnglish = false;

	bool operator==(const FMuseumHudBarView& Other) const
	{
		return NavigateText.EqualTo(Other.NavigateText) && NavigateDetail.EqualTo(Other.NavigateDetail) && bGuiding == Other.bGuiding && Visited == Other.Visited
			&& Message.EqualTo(Other.Message) && MessageDetail.EqualTo(Other.MessageDetail) && bMapOpen == Other.bMapOpen && bEnglish == Other.bEnglish;
	}
};

/**
 * The curved HUD bar low in front of the visitor: the minimized map disc (MinimapButton), the NAVIGASI button
 * (NavigateButton), the tour progress "Tur n/6" with its dots, and a short message.
 */
UCLASS()
class UMuseumMRHudBarWidget : public UMuseumMRWidget
{
	GENERATED_BODY()

public:
	static FVector2D GetDrawSize() { return FVector2D(2300., 230.); }

	void SetView(const FMuseumHudBarView& InView);

	FSimpleDelegate OnNavigate;
	FSimpleDelegate OnMinimap;

protected:
	virtual void BuildContent() override;

private:
	UFUNCTION()
	void HandleNavigateClicked();

	UFUNCTION()
	void HandleMinimapClicked();

	FMuseumHudBarView View;
	bool bHasView = false;
};

// ====== Destination menu ====== //

/** One destination card of the menu. */
struct FMuseumMenuPlace
{
	FGameplayTag PlaceTag;
	int32 Number = 0;
	FText Name;
	FText Distance;
	TObjectPtr<UTexture2D> Thumbnail;
	bool bVisited = false;
	bool bActive = false;
};

DECLARE_DELEGATE_OneParam(FMuseumMenuSelectDelegate, const FGameplayTag& /*PlaceTag*/);

/**
 * The curved glass destination menu "Mau ke mana?" (storyboard frame 2): six cards in a 3 x 2 grid with picture, number,
 * name and distance / walking time (buttons Place<Number>), the ID / EN button (LanguageButton) and close (MenuCloseButton).
 */
UCLASS()
class UMuseumMRMenuWidget : public UMuseumMRWidget
{
	GENERATED_BODY()

public:
	static FVector2D GetDrawSize() { return FVector2D(2400., 1420.); }

	void SetPlaces(const TArray<FMuseumMenuPlace>& InPlaces, bool bInEnglish);

	FMuseumMenuSelectDelegate OnSelect;
	FSimpleDelegate OnClose;
	FSimpleDelegate OnLanguage;

protected:
	virtual void BuildContent() override;

private:
	UWidget* BuildCard(const FMuseumMenuPlace& Place);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleLanguageClicked();

	UFUNCTION()
	void HandlePlaceClicked();

	TArray<FMuseumMenuPlace> Places;
	bool bEnglish = false;
};

// ====== Map header ====== //

/** The tabletop map's header "Peta Tur · 1/6 dikunjungi" with its minimize button (MapMinimizeButton), storyboard frame 8. */
UCLASS()
class UMuseumMRMapHeaderWidget : public UMuseumMRWidget
{
	GENERATED_BODY()

public:
	static FVector2D GetDrawSize() { return FVector2D(1000., 140.); }

	void SetCount(int32 InVisited, int32 InTotal, bool bInEnglish);

	FSimpleDelegate OnMinimize;

protected:
	virtual void BuildContent() override;

private:
	UFUNCTION()
	void HandleMinimizeClicked();

	int32 Visited = -1;
	int32 Total = 0;
	bool bEnglish = false;
};

// ====== Map pop-up ====== //

/**
 * The pop-up over the tour map after a stop is pressed: "Aktifkan navigasi ke destinasi ini?", the stop's picture, number and
 * name, "Ya, pandu saya" (MapConfirmYesButton) and "Batal" (MapConfirmNoButton).
 */
UCLASS()
class UMuseumMRMapConfirmWidget : public UMuseumMRWidget
{
	GENERATED_BODY()

public:
	static FVector2D GetDrawSize() { return FVector2D(1500., 560.); }

	void SetPlace(int32 InNumber, const FText& InName, UTexture2D* InThumbnail, bool bInEnglish);

	FSimpleDelegate OnYes;
	FSimpleDelegate OnNo;

protected:
	virtual void BuildContent() override;

private:
	UFUNCTION()
	void HandleYesClicked();

	UFUNCTION()
	void HandleNoClicked();

	int32 Number = 0;
	FText Name;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Thumbnail;

	bool bEnglish = false;
};
