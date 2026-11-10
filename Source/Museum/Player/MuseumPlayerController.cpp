// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MuseumPlayerController.h"

#include "Exhibit/MuseumExhibit.h"
#include "Exhibit/MuseumExhibitData.h"
#include "Language/MuseumLanguageSubsystem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/MuseumCharacter.h"
#include "Dialogue/WzDialogueSubsystem.h"
#include "Director/WzNarrativeSubsystem.h"

// ====== Lifecycle ====== //

AMuseumPlayerController::AMuseumPlayerController()
{
	PathGuide = CreateDefaultSubobject<UWzPathGuideComponent>(TEXT("PathGuide"));
}

void AMuseumPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Places the visitor walks into (guided or not) reach the narrative as events tagged with the place.
	PathGuide->OnPlaceReached.AddWeakLambda(this, [this](const FGameplayTag& Place)
	{
		GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->SendEvent(Place);
	});

	// A task that starts listening while the visitor already stands in its place still counts it.
	GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->OnListening.AddUObject(this, &AMuseumPlayerController::HandleNarrativeListening);

	// Choosing a destination in the list starts the guidance.
	PathGuide->OnStateChanged.AddUObject(this, &AMuseumPlayerController::HandlePathGuideStateChanged);

	// Arriving at whichever destination the visitor chose in the list.
	PathGuide->OnArrived.AddWeakLambda(this, [this](const FGameplayTag&)
	{
		GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->SendEvent(FGameplayTag::RequestGameplayTag(TEXT("Narrative.Event.Museum.NavigationArrived")));
	});

	// Every placed museum object: its panel takes the cursor, and a heard object reaches the narrative.
	for (TActorIterator<AMuseumExhibit> It(GetWorld()); It; ++It)
	{
		It->OnExhibitAppearStarted.AddDynamic(this, &AMuseumPlayerController::HandleExhibitAppearStarted);
		It->OnVisited.AddDynamic(this, &AMuseumPlayerController::HandleExhibitVisited);
		It->OnARInteractionEnded.AddDynamic(this, &AMuseumPlayerController::HandleExhibitEnded);
	}

	// Mapping contexts: walking, then Path Guide.
	checkf(DefaultMappingContext && PathGuideMappingContext, TEXT("The mapping contexts are not set on the player controller"));
	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	Subsystem->AddMappingContext(DefaultMappingContext, MappingPriority);
	Subsystem->AddMappingContext(PathGuideMappingContext, PathGuideMappingPriority);

	// Start in the game input mode.
	ApplyInputMode();
}

void AMuseumPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Leave the input as the game expects it: list closed, cursor hidden, look free.
	SetCursorRequested(false);
	SetDestinationMenuOpen(false);

	Super::EndPlay(EndPlayReason);
}

void AMuseumPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	checkf(MoveAction && LookAction && JumpAction && SprintAction && DialogueSkipAction && ToggleMenuAction && CloseMenuAction && ShowCursorAction, TEXT("The input actions are not set on the player controller"));
	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(InputComponent);

	// Move and look.
	Input->BindActionValueLambda(MoveAction, ETriggerEvent::Triggered, [this](const FInputActionValue& Value)
	{
		if (AMuseumCharacter* Visitor = GetPawn<AMuseumCharacter>()) Visitor->Move(Value.Get<FVector2D>());
	});
	Input->BindActionValueLambda(LookAction, ETriggerEvent::Triggered, [this](const FInputActionValue& Value)
	{
		if (AMuseumCharacter* Visitor = GetPawn<AMuseumCharacter>()) Visitor->Look(Value.Get<FVector2D>());
	});

	// Jump: a locked player (dialogue) does not jump; Space is also the dialogue Skip key.
	Input->BindActionInstanceLambda(JumpAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		if (AMuseumCharacter* Visitor = GetPawn<AMuseumCharacter>(); Visitor && !IsMoveInputIgnored()) Visitor->Jump();
	});
	Input->BindActionInstanceLambda(JumpAction, ETriggerEvent::Completed, [this](const FInputActionInstance&)
	{
		if (AMuseumCharacter* Visitor = GetPawn<AMuseumCharacter>()) Visitor->StopJumping();
	});

	// Sprint.
	Input->BindActionInstanceLambda(SprintAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		if (AMuseumCharacter* Visitor = GetPawn<AMuseumCharacter>()) Visitor->SetSprinting(true);
	});
	Input->BindActionInstanceLambda(SprintAction, ETriggerEvent::Completed, [this](const FInputActionInstance&)
	{
		if (AMuseumCharacter* Visitor = GetPawn<AMuseumCharacter>()) Visitor->SetSprinting(false);
	});

	// Dialogue skip.
	Input->BindActionInstanceLambda(DialogueSkipAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		GetLocalPlayer()->GetSubsystem<UWzDialogueSubsystem>()->Skip();
	});

	// Tour language (optional key).
	if (ToggleLanguageAction)
	{
		Input->BindActionInstanceLambda(ToggleLanguageAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
		{
			if (UMuseumLanguageSubsystem* Language = UMuseumLanguageSubsystem::Get(this)) Language->ToggleLanguage();
		});
	}

	// Path Guide menu.
	Input->BindActionInstanceLambda(ToggleMenuAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		SetDestinationMenuOpen(!bDestinationMenuOpen);
	});
	Input->BindActionInstanceLambda(CloseMenuAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		SetDestinationMenuOpen(false);
	});

	// Path Guide cursor: held to reveal it.
	Input->BindActionInstanceLambda(ShowCursorAction, ETriggerEvent::Started, [this](const FInputActionInstance&)
	{
		SetCursorRequested(true);
	});
	Input->BindActionInstanceLambda(ShowCursorAction, ETriggerEvent::Completed, [this](const FInputActionInstance&)
	{
		SetCursorRequested(false);
	});
	Input->BindActionInstanceLambda(ShowCursorAction, ETriggerEvent::Canceled, [this](const FInputActionInstance&)
	{
		SetCursorRequested(false);
	});
}

void AMuseumPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// AR view: the cursor is free to click the panels, holding the right button looks around
	if (bARViewActive && !bDestinationMenuOpen && !bCursorRequested) SetLookIgnored(!IsInputKeyDown(EKeys::RightMouseButton));
}

// ====== Narrative ====== //

void AMuseumPlayerController::HandleNarrativeListening()
{
	UWzNarrativeSubsystem* Narrative = GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>();
	for (const auto& Place : PathGuide->GetPlacesInside()) Narrative->SendEvent(Place);
}

void AMuseumPlayerController::HandlePathGuideStateChanged(const EPathGuideState NewState, const FGameplayTag&)
{
	if (NewState != EPathGuideState::Guiding) return;

	GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->SendEvent(FGameplayTag::RequestGameplayTag(TEXT("Narrative.Event.Museum.NavigationStarted")));
}

// ====== Exhibits ====== //

void AMuseumPlayerController::HandleExhibitAppearStarted(AMuseumExhibit* Exhibit)
{
	// The panel needs the cursor
	bARViewActive = true;
	ApplyInputMode();
}

void AMuseumPlayerController::HandleExhibitVisited(AMuseumExhibit* Exhibit)
{
	if (Exhibit && Exhibit->Data && Exhibit->Data->VisitedEvent.IsValid()) GetLocalPlayer()->GetSubsystem<UWzNarrativeSubsystem>()->SendEvent(Exhibit->Data->VisitedEvent);
}

void AMuseumPlayerController::HandleExhibitEnded(AMuseumExhibit* Exhibit)
{
	bARViewActive = false;
	ApplyInputMode();
}

// ====== Destination Menu ====== //

void AMuseumPlayerController::SetDestinationMenuOpen(const bool bOpen)
{
	if (bDestinationMenuOpen == bOpen) return;

	bDestinationMenuOpen = bOpen;
	ApplyInputMode();
	OnDestinationMenuChanged.Broadcast(bOpen);
}

// ====== Input Mode ====== //

void AMuseumPlayerController::ApplyInputMode()
{
	// The list or Alt reveals the cursor. The game keeps the keys, so N / Esc still reach this controller.
	if (bDestinationMenuOpen || bCursorRequested)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		SetCursorVisible(true);
		SetLookIgnored(true);
	}
	else if (bARViewActive)
	{
		// The cursor stays visible; PlayerTick frees look while the right button is held (the cursor hides during that capture)
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(true);
		SetInputMode(Mode);
		SetCursorVisible(true);
		SetLookIgnored(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		SetCursorVisible(false);
		SetLookIgnored(false);
	}

	// The open list also stops walking.
	SetMoveIgnored(bDestinationMenuOpen);
}

void AMuseumPlayerController::SetCursorRequested(const bool bRequested)
{
	if (bCursorRequested == bRequested) return;

	bCursorRequested = bRequested;
	ApplyInputMode();
}

void AMuseumPlayerController::SetCursorVisible(const bool bVisible)
{
	bShowMouseCursor = bVisible;
	bEnableClickEvents = bVisible;
	bEnableMouseOverEvents = bVisible;
}

void AMuseumPlayerController::SetLookIgnored(const bool bIgnored)
{
	if (bIgnored == bLookIgnored) return;

	SetIgnoreLookInput(bIgnored);
	bLookIgnored = bIgnored;
}

void AMuseumPlayerController::SetMoveIgnored(const bool bIgnored)
{
	if (bIgnored == bMoveIgnored) return;

	SetIgnoreMoveInput(bIgnored);
	bMoveIgnored = bIgnored;
}
