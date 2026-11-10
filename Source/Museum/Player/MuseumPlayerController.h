// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "PathGuide/PathGuideTypes.h"
#include "MuseumPlayerController.generated.h"

class AMuseumExhibit;
class UInputAction;
class UInputMappingContext;
class UWzPathGuideComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDestinationMenuChanged, bool /*bOpen*/);

/** Owns walking and Path Guide input (Enhanced Input, assets set on the Blueprint child), the Path Guide model, the destination menu state and the input mode. */
UCLASS(Blueprintable)
class AMuseumPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AMuseumPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

protected:
	// ====== Walking Input ====== //

	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	TObjectPtr<UInputAction> SprintAction;

	/** Skips the dialogue line: the 1st press completes the text, the next goes to the next line. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	TObjectPtr<UInputAction> DialogueSkipAction;

	/** Optional: switches the tour language (ID / EN). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|Input")
	TObjectPtr<UInputAction> ToggleLanguageAction;

protected:
	// ====== Path Guide ====== //

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PathGuide")
	TObjectPtr<UWzPathGuideComponent> PathGuide;

	/** Mapping context with the Toggle / Close / Show Cursor actions (N / Esc / Alt). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|PathGuideInput")
	TObjectPtr<UInputMappingContext> PathGuideMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|PathGuideInput")
	int32 PathGuideMappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|PathGuideInput")
	TObjectPtr<UInputAction> ToggleMenuAction;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|PathGuideInput")
	TObjectPtr<UInputAction> CloseMenuAction;

	/** Held to reveal the mouse cursor. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|PathGuideInput")
	TObjectPtr<UInputAction> ShowCursorAction;

public:
	FORCEINLINE UWzPathGuideComponent* GetPathGuide() const { return PathGuide; }

public:
	// ====== Destination Menu ====== //

	/** The "Mau ke mana?" list opened or closed. */
	FOnDestinationMenuChanged OnDestinationMenuChanged;

private:
	bool bDestinationMenuOpen = false;

public:
	/** Opens or closes the list: applies the input mode, then tells the UI. */
	void SetDestinationMenuOpen(bool bOpen);

	FORCEINLINE bool IsDestinationMenuOpen() const { return bDestinationMenuOpen; }

private:
	// ====== Narrative ====== //

	/** Sends the narrative the places the visitor already stands in: its tasks only see places entered after they listen. */
	void HandleNarrativeListening();

	/** Tells the narrative the visitor chose a destination (the Path Guide started guiding). */
	void HandlePathGuideStateChanged(EPathGuideState NewState, const FGameplayTag& PlaceTag);

private:
	// ====== Exhibits ====== //

	/** An object's hologram shows: the cursor works its panel. */
	UFUNCTION()
	void HandleExhibitAppearStarted(AMuseumExhibit* Exhibit);

	/** Every card of the object was heard: its visit reaches the narrative (tour progress). */
	UFUNCTION()
	void HandleExhibitVisited(AMuseumExhibit* Exhibit);

	/** The visit ended (left the zone, or ended by the tour): back to the game input mode. */
	UFUNCTION()
	void HandleExhibitEnded(AMuseumExhibit* Exhibit);

	/** An object's panel is up: the cursor works it, and look follows the mouse only while the right button is held. */
	bool bARViewActive = false;

private:
	// ====== Input Mode ====== //

	/** Alt held: the cursor is revealed without the list. */
	bool bCursorRequested = false;

	/** Mirror the ignore states so the reference-counted SetIgnore*Input only runs on a change. */
	bool bLookIgnored = false;
	bool bMoveIgnored = false;

	/** Applies the cursor, look / move ignore and input mode that match the menu and Alt state. */
	void ApplyInputMode();

	void SetCursorRequested(bool bRequested);
	void SetCursorVisible(bool bVisible);
	void SetLookIgnored(bool bIgnored);
	void SetMoveIgnored(bool bIgnored);
};