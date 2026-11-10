#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MuseumMRMinimap.generated.h"

class APlayerController;
class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;
class UWidgetComponent;

/** Where the tabletop map stands. */
enum class EMuseumMinimapMode : uint8
{
	/** Folded into the HUD bar's disc. */
	Hidden,
	/** Beside the view on the left while guiding (storyboard frame 3). */
	Side,
	/** In front, low, like a model on a table (storyboard frame 8). */
	Front,
};

/** What the map needs of one tour stop. */
struct FMuseumMinimapStopSpec
{
	int32 Number = 0;
	/** The viewing spot (the Path Guide destination). */
	FVector Spot = FVector::ZeroVector;
	/** The object itself, its mesh for the miniature and its turn. */
	FVector Object = FVector::ZeroVector;
	TObjectPtr<UStaticMesh> Mesh;
	float Yaw = 0.f;
};

/** One tour stop on the map: a small plinth with the exhibit's miniature, its number, a halo; it can be pointed at and pressed. */
USTRUCT()
struct FMuseumMinimapStop
{
	GENERATED_BODY()

	int32 Number = 0;
	FVector World = FVector::ZeroVector;

	/** Everything of the stop hangs here, so hover and press scale it as one. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Plinth;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Miniature;

	/** Soft light under the plinth, up while pointed at. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Halo;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CheckShort;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CheckLong;

	/** What the pointer hits. */
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> Hit;

	float Hover = 0.f;
	float Press = 0.f;
};

DECLARE_DELEGATE_OneParam(FMuseumMinimapStopDelegate, int32 /*Number*/);

/**
 * The tour map as a tabletop diorama of the gallery (storyboard frame 8, owner's reference M1): a dark glass tray with the hall's
 * low glass walls, one small plinth per stop with a miniature of its exhibit and a numbered halo (green check once visited), the
 * guided route glowing and the visitor's pulsing dot. The model is aligned with the museum, so the route on the table points
 * where the real one goes. A header "Peta Tur · n/6 dikunjungi" with its minimize button floats above.
 * In front, each stop can be pointed at (it grows and glows) and pressed: a pop-up asks "Aktifkan navigasi ke destinasi ini?".
 * The table opens landscape (the hall's long side across the view, the tour's start nearest the visitor); pressing and dragging
 * on it spins it a full 360 degrees and tilts it a little (up / down and sideways, a few degrees only). The visitor shows as
 * "Anda di sini" with an arrow of the way they face; a newly guided route draws itself from the visitor to the stop.
 * Owned and placed by AMuseumMRHud.
 */
UCLASS(NotPlaceable)
class AMuseumMRMinimap : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AMuseumMRMinimap();

protected:
	virtual void BeginPlay() override;

public:
	// ====== API ====== //

	/** Stops (any order) and the start of the tour. */
	void Build(const TArray<FMuseumMinimapStopSpec>& InStops, const FVector& Start);

	/**
	 * Places and animates the map. ViewYaw is the HUD's lazy view direction; Visited is per stop in tour order;
	 * Route the guided route (world, empty when not guiding); ActiveNumber the stop guided to (0 = none).
	 * Player: whose pointer may point at the stops (in front only).
	 */
	void UpdateMap(float DeltaSeconds, EMuseumMinimapMode Mode, const FVector& Eye, float ViewYaw, const FVector& Visitor, float VisitorYaw,
		const TArray<bool>& Visited, const TArray<FVector>& Route, int32 ActiveNumber, bool bEnglish, const APlayerController* Player);

	UWidgetComponent* GetHeader() const { return Header; }
	UWidgetComponent* GetConfirm() const { return Confirm; }

	/** The stop under the pointer (0 = none). */
	int32 GetHoveredNumber() const { return HoveredNumber; }

	/** The stop's place on the map in the world (for the demo's pointer; 0 = the tray's centre); false when there is no such stop. */
	bool GetStopLocation(int32 Number, FVector& OutLocation) const;

	/** Presses a stop (its pop animation) and asks to guide there. */
	void PressStop(int32 Number, const FText& Name, UTexture2D* Thumbnail, bool bEnglish);

	/** The pop-up asking to guide to a stop is open. */
	bool IsConfirmOpen() const { return ConfirmNumber != 0; }

	void CloseConfirm();

	/** The trigger went down: on the tray in front (not on a stop) it grabs the table to spin / tilt it. True when it did. */
	bool BeginDrag(const APlayerController* Player);

	void EndDrag();

	bool IsDragging() const { return bDragging; }

	/** The header's "–" was pressed. */
	FSimpleDelegate OnMinimize;

	/** The pop-up's "Ya" was pressed for this stop. */
	FMuseumMinimapStopDelegate OnConfirm;

	/** 0 = folded away, 1 = fully shown. */
	float GetShown() const { return Shown; }

private:
	FVector ToModel(const FVector& World) const;
	void SetAlpha(float Alpha);
	void UpdateHover(float DeltaSeconds, bool bCanPoint, const APlayerController* Player);

	UPROPERTY(VisibleAnywhere, Category = "Museum|Map")
	TObjectPtr<USceneComponent> Model;

	UPROPERTY(VisibleAnywhere, Category = "Museum|Map")
	TObjectPtr<UWidgetComponent> Header;

	/** "Aktifkan navigasi ke destinasi ini?" with Ya / Batal. */
	UPROPERTY(VisibleAnywhere, Category = "Museum|Map")
	TObjectPtr<UWidgetComponent> Confirm;

	/** The glass tray, its glowing rim and the hall's low walls. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Tray;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Rims;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Walls;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> RouteSegments;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> VisitorDot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> VisitorPulse;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> StartMark;

	/** "Anda di sini": an arrow the way the visitor faces and its label. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> VisitorArrow;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> VisitorLabel;

	/** The bright tip of the route while it draws itself. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RouteHead;

	UPROPERTY(Transient)
	TArray<FMuseumMinimapStop> Stops;

	FVector Centre = FVector::ZeroVector;
	FVector StartWorld = FVector::ZeroVector;
	/** Half size of the tray in the model (cm, museum axes). */
	FVector2D TrayHalf = FVector2D(28.f, 14.f);
	float MapScale = 0.01f;

	EMuseumMinimapMode CurrentMode = EMuseumMinimapMode::Hidden;
	float Shown = 0.f;
	float Time = 0.f;
	FTransform Placement;
	bool bPlaced = false;

	int32 HoveredNumber = 0;
	int32 ConfirmNumber = 0;
	float ConfirmShown = 0.f;

	/** Table turn (degrees): the landscape start, the visitor's spin (unbounded) and tilts (clamped), as shown (eased). */
	float BaseYaw = 0.f;
	float SpinYaw = 0.f;
	float TiltPitch = 0.f;
	float TiltRoll = 0.f;
	float ShownSpin = 0.f;
	float ShownPitch = 0.f;
	float ShownRoll = 0.f;

	bool bDragging = false;
	TWeakObjectPtr<const APlayerController> DragPlayer;
	FRotator DragStartRay = FRotator::ZeroRotator;
	float DragStartSpin = 0.f;
	float DragStartPitch = 0.f;

	/** The route drawing itself: for which stop, how far (0..1). */
	int32 RouteNumber = 0;
	float RouteReveal = 0.f;

	/** The language "Anda di sini" is written in (-1 = not yet). */
	int8 VisitorLabelEnglish = -1;
};
