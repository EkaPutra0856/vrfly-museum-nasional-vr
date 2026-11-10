// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Language/MuseumLanguageSubsystem.h"
#include "PathGuide/UI/PathGuideWidgetController.h"
#include "MuseumPathGuideWidgetController.generated.h"

class AMuseumPlayerController;
class UTexture2D;
class UMuseumPlaceCardsAsset;

/** Everything a destination card needs. All texts are ready to display. */
USTRUCT(BlueprintType)
struct FMuseumPlaceCardView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	FGameplayTag PlaceTag;

	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	int32 Number = 0;

	/** Number as text for the badge ("3"). */
	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	FText NumberText;

	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	TObjectPtr<UTexture2D> Thumbnail;

	/** "180 m · 3 mnt" measured along the walkways from where the visitor is now. Empty when unreachable. */
	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	FText DistanceText;

	/** True for the place the visitor is currently being guided to. */
	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	bool bIsActiveDestination = false;

	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	bool bIsReachable = true;
};

/** State of the HUD "Navigasi" button. */
USTRUCT(BlueprintType)
struct FMuseumNavigateButtonView
{
	GENERATED_BODY()

	/** "Navigasi" while idle, "Menuju: Arca Dwarapala" while guiding. */
	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	FText ButtonText;

	/** Remaining distance while guiding; empty while idle. */
	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	FText DistanceText;

	/** Show the distance and the Stop control instead of the key hint. */
	UPROPERTY(BlueprintReadOnly, Category = "Museum")
	bool bIsGuiding = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMuseumPlacesChangedSignature, const TArray<FMuseumPlaceCardView>&, Places);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMuseumMenuOpenChangedSignature, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMuseumNavigateButtonChangedSignature, const FMuseumNavigateButtonView&, Button);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMuseumLanguageCodeChangedSignature, const FText&, LanguageCode);

/** CONTROLLER of the Museum's Path Guide UI: the "Mau ke mana?" destination list, the "Navigasi" button and the ID / EN toggle, on top of the plugin's guidance. */
UCLASS(Blueprintable)
class UMuseumPathGuideWidgetController : public UWzPathGuideWidgetController
{
	GENERATED_BODY()

public:
	// ====== Setup ====== //

	virtual void BindCallbacksToDependencies() override;
	virtual void BroadcastInitialValues() override;

public:
	// ====== Events ====== //

	/** The destination list changed (opened, or the active destination changed). */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumPlacesChangedSignature OnPlacesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumMenuOpenChangedSignature OnMenuOpenChanged;

	/** The Navigasi button changed (idle / guiding, texts). */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumNavigateButtonChangedSignature OnNavigateButtonChanged;

	/** The tour language changed ("ID" / "EN"); the cards and the button were already re-sent. */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumLanguageCodeChangedSignature OnLanguageChanged;

protected:
	// ====== Config ====== //

	/** Number, description and thumbnail of every place in the list. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum")
	TObjectPtr<UMuseumPlaceCardsAsset> PlaceCards;

	UPROPERTY(EditDefaultsOnly, Category = "Museum")
	FMuseumText NavigateButtonText;

	/** Named argument: {Place} */
	UPROPERTY(EditDefaultsOnly, Category = "Museum")
	FMuseumText GuidingButtonFormat;

private:
	// ====== References ====== //

	UPROPERTY(Transient)
	TObjectPtr<AMuseumPlayerController> MuseumPlayer;

private:
	// ====== State ====== //

	TArray<FMuseumPlaceCardView> PlaceViews;

	/** The last guidance, so a language change can rebuild the button. */
	FPathGuideGuidanceView LastGuidance;

	FDelegateHandle LanguageHandle;

	/** Loaded once in BroadcastInitialValues, so opening the list never hits the disk. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> Thumbnails;

public:
	// ====== API ====== //

	/** Opens the destination list when closed, closes it when open. */
	UFUNCTION(BlueprintCallable, Category = "Museum")
	void ToggleMenu();

	UFUNCTION(BlueprintCallable, Category = "Museum")
	void CloseMenu();

	/** Closes the list and starts guiding to the place. */
	UFUNCTION(BlueprintCallable, Category = "Museum")
	void SelectPlace(FGameplayTag PlaceTag);

	/** Indonesian to English and back (the ID / EN button). */
	UFUNCTION(BlueprintCallable, Category = "Museum")
	void ToggleLanguage();

	/** "ID" or "EN". */
	UFUNCTION(BlueprintPure, Category = "Museum")
	FText GetLanguageCode() const;

private:
	// ====== Helpers ====== //

	void RefreshPlaceViews(bool bWithDistances);

	/** A card's name in the tour language, the place's own name without one. */
	FText GetPlaceName(const FGameplayTag& PlaceTag, const FText& Fallback) const;

	EMuseumLanguage GetLanguage() const;

private:
	// ====== Handlers ====== //

	/** Rebuilds the Navigasi button from the guidance status. */
	UFUNCTION()
	void HandleGuidanceChanged(const FPathGuideGuidanceView& Guidance);

	/** Moves the active-card highlight; the cards keep their distances. */
	void HandleActivePlaceChanged(EPathGuideState NewState, const FGameplayTag& PlaceTag);

	/** Refreshes the cards with distances on open. */
	void HandleMenuOpenChanged(bool bOpen);

	/** Re-sends the cards and the button in the new language. */
	void HandleLanguageChanged(EMuseumLanguage Language);
};