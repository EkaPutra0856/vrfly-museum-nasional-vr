#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Language/MuseumLanguageSubsystem.h"
#include "MR/MuseumMRInfoCardWidget.h"
#include "MuseumExhibit.generated.h"

class AMuseumExhibit;
class APlayerController;
class UAudioComponent;
class UBoxComponent;
class UFileMediaSource;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMediaPlayer;
class UMediaTexture;
class UMuseumExhibitData;
class UMuseumHologramComponent;
class UMuseumMRLabelWidget;
class UMuseumMRPointer;
class UStaticMeshComponent;
class UWidgetComponent;

/** Where an exhibit is in one visit. */
UENUM(BlueprintType)
enum class EMuseumExhibitPhase : uint8
{
	/** Nothing shows. */
	Idle,
	/** The MR detection runs over the real object (edge glow, scan wave, the "OBJEK TERDETEKSI" label). */
	Scanning,
	/** The hologram twin materializes on its pedestal; the detection look fades back to plain passthrough. */
	Appearing,
	/** Hologram and information cards are up. */
	Shown,
	/** The hologram dematerializes. */
	Hiding,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMuseumExhibitEventSignature, AMuseumExhibit*, Exhibit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMuseumExhibitPanelSignature, AMuseumExhibit*, Exhibit, int32, PanelIndex);

/** One 3D information card: a glass slab with thickness and the card widget just in front of it. */
USTRUCT()
struct FMuseumExhibitCard
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Slab;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> Widget;

	/** Pose and slab size (cm) the card moves from / to while the layout changes. */
	FTransform From;
	FTransform To;
	FVector2D FromSize = FVector2D::ZeroVector;
	FVector2D ToSize = FVector2D::ZeroVector;

	/** The view it shows, and the one it switches to halfway through the layout change (the widget fades out and in). */
	FMuseumInfoCardView Shown;
	FMuseumInfoCardView Pending;
	bool bPending = false;
	bool bSwapping = false;

	/** Pixels to world cm of the widget. */
	float FromScale = 0.1f;
	float ToScale = 0.1f;

	/** Current slab size (cm) and pixel scale. */
	FVector2D Size = FVector2D::ZeroVector;
	float Scale = 0.1f;
};

/** Where one card stands, seen from the eye. A card without an area joins the column beside the object (PanelZone). */
USTRUCT(BlueprintType)
struct FMuseumPanelSlot
{
	GENERATED_BODY()

	/** Free area for the open card in degrees (X = yaw from, Y = pitch from, Z = yaw to, W = pitch to), e.g. above or below the object. All zero = the side column. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FVector4 Area = FVector4(0., 0., 0., 0.);

	/** The point of the area the card holds on to (0..1 from yaw from / pitch from), matched by the same point of the card: the minimized bar sits there and the open card grows from it. (0.5, 0) = above an object, (0.5, 1) = below it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FVector2D Anchor = FVector2D(0.5, 0.);

	bool HasArea() const { return Area.Z > Area.X && Area.W > Area.Y; }
};

/**
 * One museum object in MR (Docs/STORYBOARD.md frames 4 to 7). On arrival the passthrough runs the detection over the real
 * object (AMuseumPassthrough reads GetDetectAmount) with a 3D "OBJEK TERDETEKSI" label and its progress ring; then the
 * hologram twin materializes bottom to top on a floating glass pedestal beside it, and the glass cards come up around it:
 * the three information cards (text, or a photo with a shorter text) and the video card. Each stands in its own place
 * (PanelSlots: above or below the object, else the column beside it in PanelZone, with a leader line to the object) and
 * is either a slim minimized bar or open; one card is open at a time, the first when the cards appear. The open video card
 * plays the object's video and opens the large 3D video screen (about 70% of the view, with a close button). Nothing but
 * that screen covers the real object or the hologram. The visit starts when the visitor enters the ViewingZone (or the demo calls BeginScan / StartPresentation)
 * and ends when the visitor leaves it.
 */
UCLASS(Blueprintable)
class AMuseumExhibit : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AMuseumExhibit();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ====== Data ====== //

	/** What this object shows and says. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Exhibit")
	TObjectPtr<UMuseumExhibitData> Data;

public:
	// ====== Activation ====== //

	/** Actor with a box component: the visit starts when the visitor stands inside it. None = only BeginScan / StartPresentation start it. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Museum|Activation")
	TObjectPtr<AActor> ViewingZone;

	/** Run the MR detection over the real object before the hologram appears (the "deteksi objek" step). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Activation")
	bool bScanFirst = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Activation", meta = (EditCondition = "bScanFirst", ClampMin = "0.1", Units = "s"))
	float ScanDuration = 3.2f;

public:
	// ====== Animation ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Animation", meta = (ClampMin = "0.1", Units = "s"))
	float RevealDuration = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Animation", meta = (ClampMin = "0.1", Units = "s"))
	float HideDuration = 0.8f;

	/** Turntable speed of the hologram; it eases to a stop while a card is open (pause while the visitor reads). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Animation", meta = (Units = "deg/s"))
	float TurntableSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Animation", meta = (ClampMin = "0", Units = "cm"))
	float FloatAmplitude = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Animation", meta = (ClampMin = "0.01"))
	float FloatFrequency = 0.22f;

public:
	// ====== Information ====== //

	/** The visitor's eye at the viewing spot (actor space): cards and labels are laid out and turned from here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (MakeEditWidget = true))
	FVector ViewOffset = FVector(-400., 0., 165.);

	/** Free area for the cards seen from the eye, in degrees (X = yaw from, Y = pitch from, Z = yaw to, W = pitch to; yaw + right, pitch + up, 0 = actor X). Keeps clear of the real object and the hologram. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information")
	FVector4 PanelZone = FVector4(14., -17., 40., 17.);

	/** Distance from the eye of the nearest card; the others stand DepthStep further each. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (ClampMin = "50", Units = "cm"))
	float NearDepth = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (ClampMin = "0", Units = "cm"))
	float DepthStep = 32.f;

	/** Most world size of one card pixel; less when the zone is small. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (ClampMin = "0.01", Units = "cm"))
	float CardScale = 0.1f;

	/** How far the column's cards turn their inner edge away (a fan opening toward the visitor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (Units = "deg"))
	float Fan = 8.f;

	/** Place of each card (the data's cards, then the video card); missing or empty slots stand in the column beside the object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information")
	TArray<FMuseumPanelSlot> PanelSlots;

	/** Card open when the cards appear (INDEX_NONE = all minimized). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information")
	int32 InitialPanel = 0;

	/** Share of the view's width the large video screen fills. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (ClampMin = "0.2", ClampMax = "1"))
	float VideoScreenShare = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (ClampMin = "50", Units = "cm"))
	float VideoScreenDistance = 150.f;

	/** Seconds after the hologram appears before the cards show (the title voice-over plays first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information", meta = (ClampMin = "0", Units = "s"))
	float PanelDelay = 0.4f;

	/** When a card's voice-over ends, open the next card (hands-free audio guide). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Information")
	bool bAutoAdvance = false;

	/** Slate pointer of this exhibit's cards; every exhibit needs its own (shared ones cancel each other's clicks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Interaction")
	int32 PointerIndex = 0;

public:
	// ====== Passthrough (demo) ====== //

	/** The recording of the real object (demo mode, Content/Movies/Plates): AMuseumPassthrough plays it while the visitor is in the ViewingZone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	TObjectPtr<UFileMediaSource> PlateVideo;

	/** Plate centre relative to the exhibit (actor space). The plate keeps the actor's rotation and faces -X, the visitor's side. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough", meta = (MakeEditWidget = true))
	FVector PlateOffset = FVector(1200., 0., 0.);

	/** Plate width and height (cm): the recording seen from the viewing spot, wider than the visitor's view so a head turn stays on it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	FVector2D PlateSize = FVector2D(4000., 2250.);

	/** Share of the plate the recording fills, centred; around it the recording shows blurred. Below 1 makes room beside a large object. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough", meta = (ClampMin = "0.3", ClampMax = "1"))
	float PlateContentScale = 1.f;

	/** World transform of the plate's centre (rotation = the actor's; scale untouched). */
	FTransform GetPlateTransform() const;

public:
	// ====== Real object (3D gallery) ====== //

	/** The real object standing in a 3D gallery level (no passthrough plate): the detection scan is drawn over its meshes. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Museum|Real Object")
	TObjectPtr<AActor> RealActor;

	/** Additive material of the scan shells over RealActor's meshes (scalars DetectAmount, DetectTime, ScanBottom, ScanTop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Real Object")
	TObjectPtr<UMaterialInterface> ScanMaterial;

	/** Height of the hologram twin (cm); 0 = the data's RealHeight x HologramScale. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Museum|Real Object", meta = (ClampMin = "0", Units = "cm"))
	float TwinHeight = 0.f;

public:
	// ====== Events ====== //

	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitEventSignature OnScanStarted;

	/** The hologram starts appearing. */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitEventSignature OnExhibitAppearStarted;

	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitEventSignature OnExhibitAppearCompleted;

	/** The information cards became visible. */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitEventSignature OnPanelShown;

	/** A card opened (its voice-over starts). */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitPanelSignature OnPanelOpened;

	/** Every card of this object was opened once in this visit. */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitEventSignature OnVisited;

	/** The visitor left the zone, or the visit was ended. */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitEventSignature OnARInteractionEnded;

	/** The hologram is gone. */
	UPROPERTY(BlueprintAssignable, Category = "Museum|Events")
	FMuseumExhibitEventSignature OnExhibitHidden;

public:
	// ====== Components ====== //

	/** The real object's bounds in the world (matched to the footage): the detection frames it, the cards keep clear of it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UBoxComponent> RealObject;

	/** Where the hologram twin floats; place it beside or above the real object. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<USceneComponent> HologramRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UStaticMeshComponent> ArtifactMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UMuseumHologramComponent> Hologram;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UBoxComponent> ArtifactClickBounds;

	/** "OBJEK TERDETEKSI · name" with its progress ring, above the real object while the detection runs. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UWidgetComponent> DetectLabel;

	/** "Hologram · name" under the pedestal. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UWidgetComponent> HologramLabel;

	/** The pointer on the cards (VR laser, else mouse or view centre). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UMuseumMRPointer> Pointer;

	/** The audio guide: title and card voice-overs, heard in the ears (2D). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Museum|Components")
	TObjectPtr<UAudioComponent> VoiceOver;

public:
	// ====== API ====== //

	/** Starts the visit with the detection (or straight with the hologram when bScanFirst is off). */
	UFUNCTION(BlueprintCallable, Category = "Museum|Exhibit")
	void BeginScan();

	/** Skips the detection: the hologram appears now. */
	UFUNCTION(BlueprintCallable, Category = "Museum|Exhibit")
	void StartPresentation();

	/** Ends the visit: cards away, voice-over stops, the hologram dematerializes. */
	UFUNCTION(BlueprintCallable, Category = "Museum|Exhibit")
	void EndPresentation();

	/** Opens one card (the others minimize) and reads it, or plays the video card; an index out of range minimizes all. */
	UFUNCTION(BlueprintCallable, Category = "Museum|Information")
	void OpenPanel(int32 Index);

	/** Opens the card, or closes it when it is the open one. */
	UFUNCTION(BlueprintCallable, Category = "Museum|Information")
	void TogglePanel(int32 Index);

	/** Plays the open card's voice-over, or stops it while it plays (the Suara button). */
	UFUNCTION(BlueprintCallable, Category = "Museum|Information")
	void ToggleVoiceOver();

	UFUNCTION(BlueprintPure, Category = "Museum|Information")
	int32 GetOpenPanel() const { return OpenPanelIndex; }

	/** Index of the video card (after the data's cards), INDEX_NONE without a video. */
	UFUNCTION(BlueprintPure, Category = "Museum|Information")
	int32 GetVideoPanel() const;

	/** Plays or pauses the video (card and screen). */
	UFUNCTION(BlueprintCallable, Category = "Museum|Information")
	void ToggleVideo();

	/** Opens the large 3D video screen in front of the visitor (the video card opens with it). */
	UFUNCTION(BlueprintCallable, Category = "Museum|Information")
	void OpenVideoScreen();

	UFUNCTION(BlueprintCallable, Category = "Museum|Information")
	void CloseVideoScreen();

	UFUNCTION(BlueprintPure, Category = "Museum|Information")
	bool IsVideoScreenOpen() const { return bScreenOpen; }

	UFUNCTION(BlueprintPure, Category = "Museum|Information")
	bool IsInformationShown() const { return bPanelVisible; }

	/** True while the open card's voice-over plays. */
	UFUNCTION(BlueprintPure, Category = "Museum|Information")
	bool IsVoiceOverPlaying() const;

	UFUNCTION(BlueprintPure, Category = "Museum|Information")
	bool WasPanelHeard(int32 Index) const { return HeardPanels.IsValidIndex(Index) && HeardPanels[Index]; }

	UFUNCTION(BlueprintPure, Category = "Museum|Exhibit")
	EMuseumExhibitPhase GetPhase() const { return Phase; }

	/** 0..1 through the detection. */
	UFUNCTION(BlueprintPure, Category = "Museum|Exhibit")
	float GetScanProgress() const;

	/** Strength of the MR detection look on the passthrough (0 = plain passthrough, 1 = full detection). */
	UFUNCTION(BlueprintPure, Category = "Museum|Exhibit")
	float GetDetectAmount() const { return DetectAmount; }

	/** Seconds since the detection started (drives the scan wave). */
	float GetDetectTime() const { return DetectTime; }

	UFUNCTION(BlueprintPure, Category = "Museum|Activation")
	bool IsZoneActive() const { return bZoneActive; }

	/** True from the start of the visit (detection or hologram) until it ends. */
	UFUNCTION(BlueprintPure, Category = "Museum|Activation")
	bool HasPresentationStarted() const { return Phase != EMuseumExhibitPhase::Idle && Phase != EMuseumExhibitPhase::Hiding; }

	/** The language of the texts and voice-overs. */
	EMuseumLanguage GetLanguage() const;

	/** The card widget components and the video screen (the demo finds buttons and the scrolled text on them). */
	TArray<UWidgetComponent*> GetCardWidgets() const;

	/** The cards' bounds seen from the eye, in world space (the demo looks at them). */
	FBox GetCardBounds() const;

	/** World point of the visitor's eye at the viewing spot. */
	FVector GetViewPoint() const;

	UFUNCTION(BlueprintCallable, Category = "Museum|Interaction")
	void HandleMousePressed();

	UFUNCTION(BlueprintCallable, Category = "Museum|Interaction")
	void HandleMouseReleased();

private:
	// ====== Setup ====== //

	void ApplyData();
	void InitializeInput();
	void CreateCards();
	void CreateVideo();
	void CreateDecorations();
	static UWidgetComponent* MakeWidgetComponent(AActor* Owner, USceneComponent* Parent, TSubclassOf<UUserWidget> Class, FName Name);

private:
	// ====== Visit ====== //

	bool IsVisitorInZone() const;
	void UpdateActivation();
	void UpdateScan(float DeltaSeconds);
	void UpdatePanelDelay(float DeltaSeconds);
	void SetPanelVisible(bool bVisible);
	void HandleHologramAppeared();
	void HandleHologramDisappeared();
	void HandleLanguageChanged(EMuseumLanguage Language);

	UFUNCTION()
	void HandleVoiceOverFinished();

	/** Plays a clip on the guide channel (stops the previous one). */
	void PlayVoiceOver(USoundBase* Sound);

	/** Stops the guide's voice; both log a VOCUE line (Tools/Demo/make_vo_track.py builds the demo video's audio from them). */
	void StopVoiceOver();

	/** Restarts the open card's voice-over (after a language change). */
	void PlayOpenPanelVoiceOver();

private:
	// ====== Cards ====== //

	/** The view of each card for the current state. */
	FMuseumInfoCardView MakeView(int32 Index) const;

	EMuseumInfoCardKind GetKind(int32 Index) const;
	EMuseumInfoCardMode GetMode(int32 Index) const;

	/** The card just opened: heard and read (or the video plays), reported. */
	void HandlePanelOpened();

	/** Shows the current state: a new layout (bLayout) animates there, else the texts update in place. */
	void RefreshCards(bool bLayout);

	/** Target pose, slab size and pixel scale of every card for the current open card. */
	void ComputeLayout(TArray<FTransform>& OutPoses, TArray<FVector2D>& OutSizes, TArray<float>& OutScales) const;

	void UpdateCards(float DeltaSeconds);
	void ApplyCard(FMuseumExhibitCard& Card, float Visibility);

	/** +1 when the cards stand right of the real object (seen from the eye), -1 left. */
	float GetCardSide() const;

	/** The column card the leader line starts from (INDEX_NONE when every card has its own place). */
	int32 GetLeaderCard() const;
	void UpdateLeader();

	void HandleCardToggle(int32 Index);
	void HandleCardVoice();
	void HandleCardLanguage();

	/** Video: the screen's pose in front of the visitor, its fade, the screen's view, stopping it all. */
	void PlaceVideoScreen();
	void UpdateVideoScreen(float DeltaSeconds);
	void RefreshVideoScreen();
	void StopVideo();

private:
	// ====== Animation ====== //

	void UpdateAnimation(float DeltaSeconds);
	void UpdateDetection(float DeltaSeconds);
	void CreateScanShells();
	void UpdateScanShells();
	void UpdatePedestal(float DeltaSeconds);
	void FaceViewer(USceneComponent* Component) const;

private:
	// ====== Pointer ====== //

	void UpdatePointer();
	void HandleMouseScrolled(float Value);

private:
	// ====== State ====== //

	UPROPERTY(Transient)
	TArray<FMuseumExhibitCard> Cards;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Leader;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LeaderDot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Pedestal;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PedestalRing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BuildRing;

	/** Copies of RealActor's meshes wearing ScanMaterial, shown while the detection runs. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ScanShells;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScanInstance;

	/** The video card's source (held: the data's soft pointer alone lets it be collected), player and texture (also on the screen); the screen's glass slab and widget. */
	UPROPERTY(Transient)
	TObjectPtr<UFileMediaSource> VideoSource;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> VideoPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> VideoTexture;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ScreenRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ScreenSlab;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> ScreenWidget;

	TWeakObjectPtr<APlayerController> LocalController;

	EMuseumExhibitPhase Phase = EMuseumExhibitPhase::Idle;
	bool bZoneActive = false;
	bool bPanelVisible = false;
	bool bVisitReported = false;
	int32 OpenPanelIndex = INDEX_NONE;
	TArray<bool> HeardPanels;

	float ScanElapsed = 0.f;
	float AppearElapsed = 0.f;
	float PanelDelayRemaining = -1.f;
	float FloatTime = 0.f;
	float TurntableYaw = 0.f;
	float TurntableRate = 1.f;

	bool bVideoWanted = false;
	bool bScreenOpen = false;
	float ScreenFade = 0.f;
	FVector2D ScreenSize = FVector2D::ZeroVector;

	float DetectAmount = 0.f;
	float DetectTime = 0.f;
	bool bDetected = false;
	float PedestalFade = 0.f;

	/** 0..1 through the current layout change; cards fade (1 = all shown) and stagger in. */
	float LayoutAlpha = 1.f;
	float CardsFade = 0.f;
	float CardsTime = 0.f;

	/** Pedestal top and radius under the hologram (HologramRoot space). */
	float PedestalTop = 0.f;
	float PedestalRadius = 20.f;
	float HologramHeight = 50.f;

	FVector RestHologramLocation = FVector::ZeroVector;
	FDelegateHandle LanguageHandle;
};
