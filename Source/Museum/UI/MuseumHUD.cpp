// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MuseumHUD.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MR/MuseumMRHud.h"
#include "UI/PathGuide/MuseumPathGuideWidgetController.h"
#include "UI/WzNarrativeWidgetController.h"
#include "UI/MuseumQuestTrackerPeek.h"
#include "WzUILibrary.h"

// ====== Lifecycle ====== //

void AMuseumHUD::BeginPlay()
{
	Super::BeginPlay();

	checkf(PathGuideWidgetClass && PathGuideWidgetControllerClass, TEXT("MuseumHUD has no Path Guide widget or widget controller class set"));
	checkf(NarrativeWidgetClass && NarrativeWidgetControllerClass, TEXT("MuseumHUD has no narrative widget or widget controller class set"));

	// Path Guide: widget and controller, wired together by the UI system.
	const auto [Widget, Controller] = UWzUILibrary::CreateWzWidget(PlayerOwner, PathGuideWidgetClass, PathGuideWidgetControllerClass, { PlayerOwner, PlayerOwner->PlayerState });
	PathGuideWidget = Widget;
	PathGuideWidgetController = Controller;
	if (bShowScreenWidgets) PathGuideWidget->AddToViewport();

	// Narrative: the same way.
	const auto [NarrativeView, NarrativeController] = UWzUILibrary::CreateWzWidget(PlayerOwner, NarrativeWidgetClass, NarrativeWidgetControllerClass, { PlayerOwner, PlayerOwner->PlayerState });
	NarrativeWidget = NarrativeView;
	NarrativeWidgetController = NarrativeController;
	if (bShowScreenWidgets) NarrativeWidget->AddToViewport();

	// The quest tracker peeks, then folds out of the way.
	TrackerPeek = NewObject<UMuseumQuestTrackerPeek>(this);
	TrackerPeek->Setup(NarrativeWidget, NarrativeWidgetController);

	// The Path Guide in 3D, in front of the eyes
	FActorSpawnParameters Params;
	Params.Owner = this;
	MRHud = GetWorld()->SpawnActor<AMuseumMRHud>(Params);
	if (MRHud) MRHud->Setup(PlayerOwner, PathGuideWidgetController);
}

void AMuseumHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PathGuideWidget->RemoveFromParent();
	NarrativeWidget->RemoveFromParent();
	if (MRHud) MRHud->Destroy();

	Super::EndPlay(EndPlayReason);
}
