// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MuseumVRComponent.generated.h"

class AMuseumMRHud;
class APawn;
class APlayerController;
class UButton;
class UCameraComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UMotionControllerComponent;
class UUserWidget;
class UWidget;
class UWidgetComponent;
class UWidgetInteractionComponent;
class UXRDeviceVisualizationComponent;
class UMuseumVRDemoDirector;
class UMuseumVRPanelHost;
struct FInputActionValue;

/**
 * Everything VR, added to the local PlayerController by AMuseumVRGameMode, while the Museum's own pawn, controller and HUD
 * Blueprints stay untouched (other Blueprints cast to them). Does nothing without a headset, so desktop play is unchanged.
 *  - Rig: puts the pawn's camera on the headset (tracking origin at the capsule's feet), shows both controllers, adds a
 *    right-hand laser that clicks world widgets.
 *  - Input: head-relative walk, snap turn, trigger click (laser on a widget, otherwise a left click for the gaze-pointed
 *    AR exhibits). The Museum's own actions (sprint, dialogue skip, destination menu) are mapped in the VR context.
 *  - Panels: screen widgets (Path Guide, narrative) move onto a world panel that lazily follows the head.
 *  - Performance: console variables set only while the headset renders, restored on end play.
 */
UCLASS(Blueprintable, ClassGroup = (Museum), meta = (BlueprintSpawnableComponent))
class MUSEUM_API UMuseumVRComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UMuseumVRComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ====== Input ====== //

protected:
	/** VR-only actions plus the Museum's own actions on controller buttons. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input")
	TObjectPtr<UInputMappingContext> VRMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input")
	int32 VRMappingPriority = 2;

	/** Left thumbstick (Axis2D): walk where the head looks. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Right thumbstick X (Axis1D): snap turn. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input")
	TObjectPtr<UInputAction> SnapTurnAction;

	/**
	 * Right trigger. Tap: click what the laser points at (a VR panel directly, otherwise a left click that the AR
	 * exhibits trace along the laser). Hold and sweep the laser up / down: scroll.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input")
	TObjectPtr<UInputAction> SelectAction;

	/** Right A. Tap: open / close the destination list (Navigasi). Hold and sweep the laser up / down: scroll. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input")
	TObjectPtr<UInputAction> MenuAction;

	/** Right thumbstick Y (Axis1D): scrolls the widget under the laser too. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input")
	TObjectPtr<UInputAction> ScrollAction;

	/** Wheel notches per second at full stick. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input", meta = (ClampMin = "0.1"))
	float ScrollSpeed = 8.f;

	/** Laser sweep per wheel notch while scrolling by hold. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input", meta = (ClampMin = "0.1", Units = "deg"))
	float DegreesPerNotch = 2.f;

	/** A trigger hold becomes a scroll (no click) once the laser moved this far. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input", meta = (ClampMin = "0.1", Units = "deg"))
	float DragThreshold = 3.f;

	/** An A press longer than this is a scroll hold, not a menu tap. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input", meta = (ClampMin = "0.05", Units = "s"))
	float HoldTime = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input", meta = (ClampMin = "5", ClampMax = "90", Units = "deg"))
	float SnapTurnAngle = 45.f;

	/** Stick deflection that fires one snap turn; it re-arms when the stick returns to the centre. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Input", meta = (ClampMin = "0.1", ClampMax = "1"))
	float SnapTurnThreshold = 0.6f;

private:
	void BindInput();
	void OnMove(const FInputActionValue& Value);
	void OnSnapTurn(const FInputActionValue& Value);
	void OnSelectStarted();
	void OnSelectCompleted();
	void OnMenuStarted();
	void OnMenuCompleted();
	void OnScroll(const FInputActionValue& Value);
	/** Hold-to-scroll: wheel notches from the laser's pitch change. */
	void UpdateHoldScroll(float DeltaTime);
	/** Click what the laser points at (press and release). */
	void Click();
	/** A keyboard key, pressed and released, through the controller (the desktop's own keys, e.g. P / X of the extended video). */
	void SimulateKey(const FKey& Key) const;
	/** Wheel notches on what the laser points at. */
	void Scroll(float Notches);
	/**
	 * Aims the laser (custom hit) at the nearest VR panel with a button or scroll box under it. The panels are stacked
	 * full-screen layouts, so the plain first-hit trace would stop on a transparent area of the front one.
	 */
	void UpdatePointerTarget();
	/** True when a widget is under the laser at the given point of a panel that takes clicks or scrolls. */
	static bool HasInteractiveWidgetAt(UWidgetComponent* Panel, const FVector& WorldLocation);
	void SimulateLeftClick(EInputEvent Event) const;

	/** The MR HUD (bar, menu, tour map) of this player, if any. */
	AMuseumMRHud* GetMRHud() const;

public:
	/** True when the laser is over a widget of a VR panel (not an AR exhibit's, which handles its own clicks). */
	bool IsPointingAtVRPanel() const;

	FORCEINLINE UInputMappingContext* GetVRMappingContext() const { return VRMappingContext; }
	FORCEINLINE UInputAction* GetMoveAction() const { return MoveAction; }
	FORCEINLINE UInputAction* GetSnapTurnAction() const { return SnapTurnAction; }
	FORCEINLINE UInputAction* GetSelectAction() const { return SelectAction; }
	FORCEINLINE UInputAction* GetMenuAction() const { return MenuAction; }
	FORCEINLINE UInputAction* GetScrollAction() const { return ScrollAction; }

private:
	float GetLaserPitch() const;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> VRInput;

	bool bSnapTurnArmed = true;
	bool bTriggerHeld = false;
	bool bMenuHeld = false;
	bool bScrolling = false;
	/** The trigger went down on the tour map's tray: held, it spins the map (no click on release). */
	bool bMapDragging = false;
	float HeldTime = 0.f;
	float StartPitch = 0.f;
	float LastPitch = 0.f;
	float ScrollRemainder = 0.f;

	// ====== Rig ====== //

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Rig", meta = (ClampMin = "10", Units = "cm"))
	float PointerDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Rig")
	FLinearColor LaserColor { 0.35f, 0.75f, 1.f };

	/** The laser shows only while it points at a panel (VR or AR widget) or a hologram, and this long after (bridges the gaps between buttons).
	 *  While the visitor navigates (destination menu or map in front open) it stays on. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Rig", meta = (ClampMin = "0", Units = "s"))
	float LaserLingerTime = 0.15f;

	/** Laser length while navigating and pointing between the cards (it ends earlier on what is in the way). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Rig", meta = (ClampMin = "0", Units = "cm"))
	float NavigationLaserLength = 300.f;

private:
	void HandleNewPawn(APawn* Pawn);
	void BuildRig(APawn* Pawn);

	/** Laser on demand: from the hand to the VR panel, AR panel or hologram it points at (VRHit: the VR panel hit, if any); hidden elsewhere. */
	void UpdateLaserVisibility(const FHitResult& VRHit, const FVector& Start, const FVector& End);

	float LaserLinger = 0.f;
	float LaserFade = 0.f;
	float LaserDotFade = 0.f;
	FVector LaserEnd = FVector::ZeroVector;

	/** The laser beam and the dot where it touches (glow meshes on the pawn, no collision). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LaserBeam;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LaserDot;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetInteractionComponent> Pointer;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> HeadCamera;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> LeftGrip;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> RightGrip;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> RightAim;

public:
	/** The laser's ray for the controller's player, when VR is active; mouse-driven systems (AR exhibits) point with it. */
	static bool GetPointerRay(const APlayerController* Controller, FVector& OutStart, FVector& OutDirection);

	FORCEINLINE USceneComponent* GetVROrigin() const { return VROrigin; }
	FORCEINLINE UWidgetInteractionComponent* GetPointer() const { return Pointer; }
	FORCEINLINE UMotionControllerComponent* GetLeftGrip() const { return LeftGrip; }
	FORCEINLINE UMotionControllerComponent* GetRightGrip() const { return RightGrip; }
	FORCEINLINE UMotionControllerComponent* GetRightAim() const { return RightAim; }

	// ====== Panels ====== //

protected:
	/** Screen widgets of these classes leave the screen and go onto a VR panel (default: every user widget, e.g. Path Guide, narrative, extended media). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel")
	TArray<TSubclassOf<UUserWidget>> RelayWidgetClasses;

	/** Widgets inside the relayed widgets to collapse by name (screen-projected markers, e.g. WaypointPin). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel")
	TArray<FName> HiddenWidgetNames;

	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (ClampMin = "0.05", Units = "s"))
	float RelayInterval = 0.25f;

	/** Widget resolution of the panel (the screen layout is drawn at this size). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel")
	FIntPoint PanelDrawSize { 1920, 1080 };

	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (ClampMin = "30", Units = "cm"))
	float PanelDistance = 120.f;

	/** World width of the panel; bigger = bigger text. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (ClampMin = "30", Units = "cm"))
	float PanelWidth = 170.f;

	/** Height of the panel centre relative to the eyes. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (Units = "cm"))
	float PanelHeightOffset = -5.f;

	/** The panel re-centres when the head turns further than this from it ... */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	float FollowAngle = 35.f;

	/** ... or when the head moves further than this from where the panel settled. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (ClampMin = "0", Units = "cm"))
	float FollowDistance = 40.f;

	/** How fast the panel glides back in front (interp speed). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (ClampMin = "0.1"))
	float FollowSpeed = 3.f;

	/** Widget material with depth test off, so near geometry never cuts the panel (M_ARWidget_OnTop). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel")
	TSoftObjectPtr<UMaterialInterface> PanelMaterial;

	/** The extended video widget (class name contains this): it has no buttons, so a laser click plays it, the next closes it. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel")
	FString MediaExtendWidgetName = TEXT("MediaExtend");

	/** Seconds the extended video stays on the screen before it moves to a panel (its own opening step must finish on the screen). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Panel", meta = (ClampMin = "0", Units = "s"))
	float MediaExtendRelayDelay = 1.8f;

private:
	/** When a screen widget was first seen in the viewport (for delayed relays). */
	TMap<TWeakObjectPtr<UUserWidget>, double> FirstSeenInViewport;

protected:

private:
	/** True for a panel hosting the extended video. */
	bool IsMediaExtendPanel(const UWidgetComponent* Panel) const;

	/** The extended video got its play click; the next click closes it. */
	bool bMediaExtendPlayed = false;

protected:

private:
	void RelayScreenWidgets();
	void HideScreenOnlyWidgets(UUserWidget* Widget) const;
	void UpdatePanels(float DeltaTime);
	bool GetHeadView(FVector& OutLocation, FRotator& OutYaw) const;

	/** The HUD elements on the panels, in view angles (FMuseumViewClutter source while VR is active). */
	void QueryClutter(const APlayerController* Controller, TArray<struct FMuseumViewRect>& OutRects) const;

	/** Panels live on their own actor: the laser ignores its owner's components, and the AR exhibits' trace ignores this actor (ARIgnoreTrace tag). */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PanelActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> Panels;

	/** One host per panel (same index); empty once the game removed (closed) its widget. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMuseumVRPanelHost>> Hosts;

public:
	FORCEINLINE const TArray<TObjectPtr<UWidgetComponent>>& GetPanels() const { return Panels; }
	FORCEINLINE const TArray<TObjectPtr<UMuseumVRPanelHost>>& GetHosts() const { return Hosts; }

private:
	FTimerHandle RelayTimer;
	FVector AnchorLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
	bool bHasTarget = false;
	bool bFollowing = false;

	// ====== Hover ====== //

protected:
	/** A button under the laser grows to this scale ... */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Hover", meta = (ClampMin = "1", ClampMax = "1.5"))
	float HoverScale = 1.06f;

	/** ... and gets a thin glow outline in this colour. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Hover")
	FLinearColor HoverGlowColor { 0.45f, 0.85f, 1.f };

	/** How fast the hover grows and glows in (interp speed). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Hover", meta = (ClampMin = "0.1"))
	float HoverSpeed = 14.f;

private:
	void UpdateHover(float DeltaTime);
	/** Puts the hovered button back as it was and hides its glow. */
	void ClearHover();
	/** The enabled button the laser hovers inside the widget (nested user widgets included), or null. */
	static UButton* FindHoveredButton(const UUserWidget* Widget);

	TWeakObjectPtr<UButton> HoveredButton;
	TWeakObjectPtr<UMuseumVRPanelHost> HoveredHost;
	FVector2D HoveredBaseScale { 1.f, 1.f };
	FVector2D HoveredBasePivot { 0.5f, 0.5f };
	float HoverAlpha = 0.f;

	// ====== Prompts ====== //

protected:
	/** Keyboard hints in any UI text, replaced by the controller buttons while VR is active (e.g. "[N]" -> "[A]"). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Prompts")
	TMap<FString, FString> PromptReplacements
	{
		{ TEXT("[N]"), TEXT("[A]") },
		{ TEXT("N / Esc"), TEXT("A / B") },
		{ TEXT("[Esc]"), TEXT("[B]") },
		{ TEXT("[Spasi]"), TEXT("[Trigger]") },
		{ TEXT("Spasi"), TEXT("Trigger") },
		{ TEXT("[Alt]"), TEXT("[Trigger]") },
	};

	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Prompts", meta = (ClampMin = "0.02", Units = "s"))
	float PromptInterval = 0.1f;

private:
	void ReplacePrompts();
	void ReplacePromptsIn(UUserWidget* Widget);
	void ReplacePromptText(UWidget* Child) const;

	float PromptTimer = 0.f;

	// ====== Performance ====== //

protected:
	/** Set only while the headset renders, restored on end play. Defaults trade Lumen GI and Virtual Shadow Maps for a VR frame rate; VFX stay as is. */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR|Performance")
	TMap<FString, FString> VRConsoleVariables
	{
		{ TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("0") },
		{ TEXT("r.ReflectionMethod"), TEXT("2") },
		{ TEXT("r.Shadow.Virtual.Enable"), TEXT("0") },
		{ TEXT("r.Shadow.CSM.MaxCascades"), TEXT("2") },
		{ TEXT("r.Streaming.PoolSize"), TEXT("2000") },
		{ TEXT("vr.PixelDensity"), TEXT("0.9") },
	};

private:
	void ApplyConsoleVariables();
	void RestoreConsoleVariables();

	TMap<FString, FString> SavedConsoleVariables;

	// ====== Demo ====== //

	/**
	 * Dev autopilot (museum.vr.Demo 1 or -MuseumVRDemo, never in Shipping): the VR layer runs without a headset and
	 * UMuseumVRDemoDirector drives the head, the hands and the controller keys from a script. Off by default.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UMuseumVRDemoDirector> Demo;

	bool bDemo = false;

public:
	FORCEINLINE bool IsDemo() const { return bDemo; }

	/** Demo poses relative to the tracking origin (the pawn's feet): head (camera), both grips and the right aim (laser). */
	void SetDemoPoses(const FTransform& Head, const FTransform& Left, const FTransform& Right, const FTransform& Aim) const;

private:
	// ====== State ====== //

	/** True when a headset renders this play session; everything above stays off otherwise. */
	bool bVRActive = false;

public:
	APlayerController* GetController() const;
};
