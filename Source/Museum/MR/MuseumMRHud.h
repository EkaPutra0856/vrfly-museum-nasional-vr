#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "UI/PathGuide/MuseumPathGuideWidgetController.h"
#include "MuseumMRHud.generated.h"

class AMuseumExhibit;
class AMuseumMRMinimap;
class AMuseumMRWaypoints;
class APlayerController;
class UMuseumMRPointer;
class UWidgetComponent;

/**
 * The visitor's MR interface outside the exhibits (Docs/STORYBOARD.md frames 1, 2, 3 and 8), all in 3D in front of the eyes:
 * - the curved HUD bar low in front, following the head lazily: map disc, NAVIGASI, "Tur n/6", a short message;
 * - the curved glass destination menu "Mau ke mana?" (3 x 2 cards) where the visitor looked when it opened;
 * - the tabletop tour map (AMuseumMRMinimap): left of the view while guiding, in front when the disc is pressed; there its
 *   stops can be pressed, and after the pop-up's "Ya" the visitor is guided to that one;
 * - the dotted floor path and the destination pin while guiding (AMuseumMRWaypoints).
 * Driven by the Path Guide widget controller (places, menu, guidance, language). Everything folds away while an exhibit
 * presents itself. Spawned by AMuseumHUD.
 */
UCLASS(NotPlaceable)
class AMuseumMRHud : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AMuseumMRHud();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ====== API ====== //

	/** Binds the Path Guide UI controller (call right after spawning). */
	void Setup(APlayerController* InPlayer, UMuseumPathGuideWidgetController* InController);

	/** The visitor is choosing where to go: the destination menu or the tabletop map in front is open. */
	bool IsNavigationOpen() const { return bMenuOpen || bMapFront; }

	/** The trigger went down (held, VR) or the mouse: on the map's tray in front it grabs the table to spin it. True when it did. */
	bool BeginMapDrag();

	void EndMapDrag();

	/** Slate pointer of the HUD's widgets (each MR owner has its own). */
	UPROPERTY(EditAnywhere, Category = "Museum|MR")
	int32 PointerIndex = 5;

public:
	// ====== Components ====== //

	UPROPERTY(VisibleAnywhere, Category = "Museum|MR")
	TObjectPtr<UWidgetComponent> Bar;

	UPROPERTY(VisibleAnywhere, Category = "Museum|MR")
	TObjectPtr<UWidgetComponent> Menu;

	UPROPERTY(VisibleAnywhere, Category = "Museum|MR")
	TObjectPtr<UMuseumMRPointer> Pointer;

private:
	// ====== Controller events ====== //

	UFUNCTION()
	void HandlePlacesChanged(const TArray<FMuseumPlaceCardView>& Places);

	UFUNCTION()
	void HandleMenuOpenChanged(bool bOpen);

	UFUNCTION()
	void HandleNavigateButtonChanged(const FMuseumNavigateButtonView& Button);

	UFUNCTION()
	void HandleLanguageChanged(const FText& LanguageCode);

	UFUNCTION()
	void HandleExhibitVisited(AMuseumExhibit* Exhibit);

private:
	// ====== Widget events ====== //

	void HandleNavigate();
	void HandleMinimap();
	void HandleMapMinimize();
	void HandleMapConfirm(int32 Number);
	void HandleSelect(const FGameplayTag& PlaceTag);
	void HandleMenuClose();
	void HandleMenuLanguage();

private:
	// ====== Input ====== //

	void HandlePressed();
	void HandleReleased();
	void HandleScrolled(float Value);

private:
	// ====== Helpers ====== //

	void RefreshBar();
	void RefreshMenu();
	bool IsPresenting() const;
	bool IsGuiding() const;
	bool IsEnglish() const;
	TArray<bool> GetVisited() const;
	int32 GetActiveNumber() const;
	const AMuseumExhibit* FindExhibit(int32 Number) const;
	FText GetActiveName() const;
	/** Direction (world yaw) for the map beside the exhibit the visitor stands at (in its cards' zone, by the object); false away from exhibits. */
	bool GetFreeYaw(float& OutYaw) const;
	void BuildMap();

	/** A curved widget at a distance and pitch from the eye, facing it (cylinder geometry: the arc's middle at Distance). */
	static void PlaceCurved(UWidgetComponent* Widget, const FVector& Eye, float Yaw, float Pitch, float Distance, float ArcDegrees);
	static void SetShown(UWidgetComponent* Widget, float Alpha);

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	TObjectPtr<UMuseumPathGuideWidgetController> Controller;

	UPROPERTY(Transient)
	TObjectPtr<AMuseumMRMinimap> Minimap;

	UPROPERTY(Transient)
	TObjectPtr<AMuseumMRWaypoints> Waypoints;

	/** The tour's exhibits in tour order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AMuseumExhibit>> Exhibits;

	TWeakObjectPtr<APlayerController> Player;
	TArray<FMuseumPlaceCardView> PlaceViews;
	FMuseumNavigateButtonView NavigateView;
	TSet<FGameplayTag> VisitedPlaces;

	bool bMenuOpen = false;
	bool bMapFront = false;
	bool bSideDismissed = false;
	/** After "Ya" on the map: it stays in front this long (s) so the route draws itself, then goes beside the view. */
	float MapCloseTimer = 0.f;
	bool bWasGuiding = false;

	float FollowYaw = 0.f;
	bool bFollowing = false;
	float MenuYaw = 0.f;
	FVector MenuEye = FVector::ZeroVector;
	float BarShown = 0.f;
	float MenuShown = 0.f;
	float Time = 0.f;
};
