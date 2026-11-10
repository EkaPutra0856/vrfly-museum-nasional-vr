// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PathGuide/MuseumPathGuideWidgetController.h"

#include "Algo/StableSort.h"
#include "Engine/Texture2D.h"
#include "PathGuide/Routing/PathGuideFormat.h"
#include "Player/MuseumPlayerController.h"
#include "UI/PathGuide/MuseumPlaceCardsAsset.h"

// ====== Setup ====== //

void UMuseumPathGuideWidgetController::BindCallbacksToDependencies()
{
	Super::BindCallbacksToDependencies();

	MuseumPlayer = CastChecked<AMuseumPlayerController>(PlayerController);

	// Navigasi button
	OnGuidanceChanged.AddDynamic(this, &UMuseumPathGuideWidgetController::HandleGuidanceChanged);

	// Active card
	PathGuide->OnStateChanged.AddUObject(this, &UMuseumPathGuideWidgetController::HandleActivePlaceChanged);

	// Menu: measure distances on every open
	MuseumPlayer->OnDestinationMenuChanged.AddUObject(this, &UMuseumPathGuideWidgetController::HandleMenuOpenChanged);

	// Tour language
	if (UMuseumLanguageSubsystem* Language = UMuseumLanguageSubsystem::Get(MuseumPlayer)) LanguageHandle = Language->OnLanguageChanged.AddUObject(this, &UMuseumPathGuideWidgetController::HandleLanguageChanged);
}

void UMuseumPathGuideWidgetController::BroadcastInitialValues()
{
	checkf(PlaceCards, TEXT("The Museum Path Guide widget controller has no PlaceCards asset set"));

	// Thumbnails, loaded once
	for (const auto& Card : PlaceCards->Cards) Thumbnails.Add(Card.PlaceTag, Card.Thumbnail.LoadSynchronous());

	// The plugin broadcasts the guidance, which builds the button
	Super::BroadcastInitialValues();

	// Initial cards
	RefreshPlaceViews(false);

	OnPlacesChanged.Broadcast(PlaceViews);
	OnMenuOpenChanged.Broadcast(MuseumPlayer->IsDestinationMenuOpen());
	OnLanguageChanged.Broadcast(GetLanguageCode());
}

// ====== API ====== //

void UMuseumPathGuideWidgetController::ToggleMenu()
{
	MuseumPlayer->SetDestinationMenuOpen(!MuseumPlayer->IsDestinationMenuOpen());
}

void UMuseumPathGuideWidgetController::CloseMenu()
{
	MuseumPlayer->SetDestinationMenuOpen(false);
}

void UMuseumPathGuideWidgetController::SelectPlace(const FGameplayTag PlaceTag)
{
	MuseumPlayer->SetDestinationMenuOpen(false);
	PathGuide->StartGuidance(PlaceTag);
}

void UMuseumPathGuideWidgetController::ToggleLanguage()
{
	if (UMuseumLanguageSubsystem* Language = UMuseumLanguageSubsystem::Get(MuseumPlayer)) Language->ToggleLanguage();
}

FText UMuseumPathGuideWidgetController::GetLanguageCode() const
{
	return UMuseumLanguageSubsystem::GetLanguageCode(GetLanguage());
}

// ====== Helpers ====== //

void UMuseumPathGuideWidgetController::RefreshPlaceViews(const bool bWithDistances)
{
	PlaceViews.Reset();

	const bool bGuiding = PathGuide->GetState() == EPathGuideState::Guiding;
	const TArray<FPathGuidePlace> Places = PathGuide->GetAvailablePlaces();

	// Measure every place in one go: the routes share the visitor-side work
	TArray<float> Lengths;
	if (bWithDistances)
	{
		TArray<FGameplayTag> Tags;
		for (const auto& Place : Places) Tags.Add(Place.PlaceTag);
		PathGuide->EstimateRouteLengths(Tags, Lengths);
	}

	// One card per place; a place without a card shows only its name
	for (int32 Index = 0; Index < Places.Num(); ++Index)
	{
		const FPathGuidePlace& Place = Places[Index];
		FMuseumPlaceCardView& View = PlaceViews.AddDefaulted_GetRef();
		View.PlaceTag = Place.PlaceTag;
		View.DisplayName = GetPlaceName(Place.PlaceTag, Place.DisplayName);
		View.bIsActiveDestination = bGuiding && PathGuide->GetActivePlaceTag() == Place.PlaceTag;

		if (const FMuseumPlaceCard* Card = PlaceCards->FindCard(Place.PlaceTag))
		{
			View.Number = Card->Number;
			View.NumberText = FText::AsNumber(Card->Number);
			View.Description = Card->Description.Get(GetLanguage());
			View.Thumbnail = Thumbnails.FindRef(Place.PlaceTag);
		}

		// Distance and ETA, or "unreachable" when there is no route
		if (!bWithDistances) continue;

		View.bIsReachable = Lengths[Index] >= 0.f;
		if (View.bIsReachable) View.DistanceText = PathGuide::Format::DistanceAndEta(Lengths[Index], GetUIConfig());
	}

	// List order is the badge number
	Algo::StableSortBy(PlaceViews, &FMuseumPlaceCardView::Number);
}

FText UMuseumPathGuideWidgetController::GetPlaceName(const FGameplayTag& PlaceTag, const FText& Fallback) const
{
	const FMuseumPlaceCard* Card = PlaceCards ? PlaceCards->FindCard(PlaceTag) : nullptr;
	const FText& Name = Card ? Card->Name.Get(GetLanguage()) : FText::GetEmpty();

	return Name.IsEmpty() ? Fallback : Name;
}

EMuseumLanguage UMuseumPathGuideWidgetController::GetLanguage() const
{
	return UMuseumLanguageSubsystem::GetCurrent(MuseumPlayer);
}

// ====== Handlers ====== //

void UMuseumPathGuideWidgetController::HandleGuidanceChanged(const FPathGuideGuidanceView& Guidance)
{
	// Texts straight from the guidance, the place named in the tour language
	LastGuidance = Guidance;
	FMuseumNavigateButtonView View;
	View.DistanceText = Guidance.DistanceText;
	View.bIsGuiding = Guidance.bIsGuiding;

	// Guiding: "Menuju: {Place}". Idle: the plain Navigasi button
	if (Guidance.bIsGuiding)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Place"), GetPlaceName(PathGuide->GetActivePlaceTag(), Guidance.PlaceName));
		View.ButtonText = FText::Format(GuidingButtonFormat.Get(GetLanguage()), Arguments);
	}
	else View.ButtonText = NavigateButtonText.Get(GetLanguage());

	OnNavigateButtonChanged.Broadcast(View);
}

void UMuseumPathGuideWidgetController::HandleActivePlaceChanged(const EPathGuideState NewState, const FGameplayTag& PlaceTag)
{
	for (auto& View : PlaceViews) View.bIsActiveDestination = NewState == EPathGuideState::Guiding && View.PlaceTag == PlaceTag;
	OnPlacesChanged.Broadcast(PlaceViews);
}

void UMuseumPathGuideWidgetController::HandleMenuOpenChanged(const bool bOpen)
{
	// Measure distances on every open
	if (bOpen)
	{
		RefreshPlaceViews(true);
		OnPlacesChanged.Broadcast(PlaceViews);
	}

	OnMenuOpenChanged.Broadcast(bOpen);
}

void UMuseumPathGuideWidgetController::HandleLanguageChanged(EMuseumLanguage)
{
	// Cards (with distances while the list is open) and the button, then the toggle's code
	RefreshPlaceViews(MuseumPlayer->IsDestinationMenuOpen());
	OnPlacesChanged.Broadcast(PlaceViews);
	HandleGuidanceChanged(LastGuidance);
	OnLanguageChanged.Broadcast(GetLanguageCode());
}