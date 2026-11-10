// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MuseumHUD.generated.h"

class AMuseumMRHud;
class UWzNarrativeWidgetController;
class UWzWidget;
class UMuseumPathGuideWidgetController;
class UMuseumQuestTrackerPeek;

/**
 * The game's HUD. Creates the Path Guide and narrative widget controllers through CreateWzWidget, and shows the Path Guide
 * as the 3D MR interface (AMuseumMRHud: curved HUD bar, destination menu, tabletop map, floor waypoints) instead of the flat
 * widget Blueprints, which stay out of the viewport. Make a Blueprint child, set its widget classes and use it as the game mode's HUD.
 */
UCLASS(Blueprintable)
class AMuseumHUD : public AHUD
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The 3D MR interface (null until BeginPlay). */
	AMuseumMRHud* GetMRHud() const { return MRHud; }

private:
	// ====== Path Guide ====== //

	/** The Path Guide widget Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "PathGuide")
	TSubclassOf<UWzWidget> PathGuideWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UWzWidget> PathGuideWidget;

	UPROPERTY(EditDefaultsOnly, Category = "PathGuide")
	TSubclassOf<UMuseumPathGuideWidgetController> PathGuideWidgetControllerClass;

	UPROPERTY(Transient)
	TObjectPtr<UMuseumPathGuideWidgetController> PathGuideWidgetController;

	/** The 3D MR interface driven by the Path Guide controller. */
	UPROPERTY(Transient)
	TObjectPtr<AMuseumMRHud> MRHud;

	/** Also show the flat Path Guide and narrative widgets on the screen (off: the MR interface only). */
	UPROPERTY(EditDefaultsOnly, Category = "PathGuide")
	bool bShowScreenWidgets = false;

public:
	UMuseumPathGuideWidgetController* GetPathGuideWidgetController() const { return PathGuideWidgetController; }

private:

private:
	// ====== Narrative ====== //

	/** The narrative overlay Blueprint: dialogue, objective tracker and quest toast. */
	UPROPERTY(EditDefaultsOnly, Category = "Narrative")
	TSubclassOf<UWzWidget> NarrativeWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UWzWidget> NarrativeWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Narrative")
	TSubclassOf<UWzNarrativeWidgetController> NarrativeWidgetControllerClass;

	UPROPERTY(Transient)
	TObjectPtr<UWzNarrativeWidgetController> NarrativeWidgetController;

	/** Folds the quest tracker into a small button after its intro (and after each new objective). */
	UPROPERTY(Transient)
	TObjectPtr<UMuseumQuestTrackerPeek> TrackerPeek;
};
