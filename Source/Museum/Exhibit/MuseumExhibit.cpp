#include "Exhibit/MuseumExhibit.h"

#include "Museum.h"

#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Exhibit/MuseumExhibitData.h"
#include "FileMediaSource.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Hologram/MuseumHologramComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MR/MuseumMRLabelWidget.h"
#include "MR/MuseumMRPointer.h"
#include "MR/MuseumMRStyle.h"
#include "Sound/SoundBase.h"

#define LOCTEXT_NAMESPACE "MuseumExhibit"

using namespace MuseumMR;

namespace
{
	constexpr float ClickBoundsPadding = 4.f;

	/** How fast the turntable eases to a stop (and back) when a card opens or closes. */
	constexpr float TurntableEaseSpeed = 2.5f;

	/** Cards: slab thickness, the widget's gap in front of it, the angle between two column cards, timings. */
	constexpr float SlabThickness = 3.f;
	constexpr float WidgetGap = 1.6f;
	constexpr float CardGap = 1.4f;
	constexpr float LayoutSeconds = 0.55f;
	constexpr float CardStagger = 0.14f;
	constexpr float CardAppearSeconds = 0.45f;
	constexpr float CardHideSeconds = 0.3f;

	/** Video screen: fade in / out, how far it rises while it comes. */
	constexpr float ScreenSeconds = 0.35f;
	constexpr float ScreenRise = 6.f;

	/** Detection: ramp up, fade back to plain passthrough while the twin appears (share of the reveal). */
	constexpr float DetectRampSeconds = 0.6f;
	constexpr float DetectFadeShare = 0.7f;

	/** Labels: distance of the detection label from the eye, pixel scales. The plates hug their text, centred: the draw
	 *  areas are wide enough for the longest name ("Maket Miniatur Arsitektur Masjid At-Thohir"), else its end is cut. */
	constexpr float DetectLabelDistance = 300.f;
	const FVector2D DetectLabelSize(2400., 320.);
	constexpr float DetectLabelScale = 0.085f;
	const FVector2D HologramLabelSize(1600., 160.);
	constexpr float HologramLabelScale = 0.07f;

	/** World widgets the demo and the VR pointer may click. */
	const FName InteractiveTag(TEXT("MuseumMRInteractive"));

	float Deg(const double Radians) { return FMath::RadiansToDegrees(static_cast<float>(Radians)); }
	float TanDeg(const float Degrees) { return FMath::Tan(FMath::DegreesToRadians(Degrees)); }
}

// ====== Lifecycle ====== //

AMuseumExhibit::AMuseumExhibit()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ExhibitAnchor"));

	RealObject = CreateDefaultSubobject<UBoxComponent>(TEXT("RealObject"));
	RealObject->SetupAttachment(RootComponent);
	RealObject->SetBoxExtent(FVector(30., 30., 50.));
	RealObject->SetRelativeLocation(FVector(0., 0., 50.));
	RealObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RealObject->SetHiddenInGame(true);
	RealObject->ShapeColor = FColor(255, 170, 40);

	HologramRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HologramRoot"));
	HologramRoot->SetupAttachment(RootComponent);
	HologramRoot->SetRelativeLocation(FVector(0., -70., 0.));

	ArtifactMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtifactMesh"));
	ArtifactMesh->SetupAttachment(HologramRoot);
	ArtifactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtifactMesh->SetCastShadow(false);

	Hologram = CreateDefaultSubobject<UMuseumHologramComponent>(TEXT("Hologram"));

	ArtifactClickBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ArtifactClickBounds"));
	ArtifactClickBounds->SetupAttachment(HologramRoot);
	ArtifactClickBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtifactClickBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArtifactClickBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ArtifactClickBounds->SetGenerateOverlapEvents(false);
	ArtifactClickBounds->SetHiddenInGame(true);

	auto MakeLabel = [this](const TCHAR* Name, USceneComponent* Parent, const FVector2D& Size, const float Scale)
	{
		UWidgetComponent* Label = CreateDefaultSubobject<UWidgetComponent>(Name);
		Label->SetupAttachment(Parent);
		Label->SetWidgetSpace(EWidgetSpace::World);
		Label->SetWidgetClass(UMuseumMRLabelWidget::StaticClass());
		Label->SetDrawSize(Size);
		Label->SetPivot(FVector2D(.5, .5));
		Label->SetTwoSided(false);
		Label->SetBlendMode(EWidgetBlendMode::Transparent);
		Label->SetCastShadow(false);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetCanEverAffectNavigation(false);
		Label->SetRelativeScale3D(FVector(Scale));
		Label->SetTranslucentSortPriority(2);

		return Label;
	};
	DetectLabel = MakeLabel(TEXT("DetectLabel"), RootComponent, DetectLabelSize, DetectLabelScale);
	HologramLabel = MakeLabel(TEXT("HologramLabel"), HologramRoot, HologramLabelSize, HologramLabelScale);

	Pointer = CreateDefaultSubobject<UMuseumMRPointer>(TEXT("Pointer"));
	Pointer->SetupAttachment(RootComponent);

	VoiceOver = CreateDefaultSubobject<UAudioComponent>(TEXT("VoiceOver"));
	VoiceOver->SetupAttachment(RootComponent);
	VoiceOver->bAutoActivate = false;
	VoiceOver->bAllowSpatialization = false;
	VoiceOver->bIsUISound = true;
}

void AMuseumExhibit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Editor preview: the mesh at its size
	ApplyData();
}

void AMuseumExhibit::BeginPlay()
{
	Super::BeginPlay();
	ApplyData();

	LocalController = UGameplayStatics::GetPlayerController(this, 0);
	InitializeInput();
	Pointer->PointerIndex = PointerIndex;
	RestHologramLocation = HologramRoot->GetRelativeLocation();

	// The hologram look dresses the artifact; it reports back when the reveal ends either way
	if (Data && Data->HologramProfile) Hologram->Profile = Data->HologramProfile;
	Hologram->SetTarget(ArtifactMesh);
	Hologram->OnAppeared.AddUObject(this, &AMuseumExhibit::HandleHologramAppeared);
	Hologram->OnDisappeared.AddUObject(this, &AMuseumExhibit::HandleHologramDisappeared);

	VoiceOver->OnAudioFinished.AddDynamic(this, &AMuseumExhibit::HandleVoiceOverFinished);

	// Texts and voice-overs follow the tour language
	if (UMuseumLanguageSubsystem* Language = UMuseumLanguageSubsystem::Get(this))
	{
		LanguageHandle = Language->OnLanguageChanged.AddUObject(this, &AMuseumExhibit::HandleLanguageChanged);
	}

	DetectLabel->InitWidget();
	HologramLabel->InitWidget();
	if (UMuseumMRLabelWidget* Label = Cast<UMuseumMRLabelWidget>(DetectLabel->GetUserWidgetObject())) Label->SetScale(40, 60);
	if (UMuseumMRLabelWidget* Label = Cast<UMuseumMRLabelWidget>(HologramLabel->GetUserWidgetObject())) Label->SetScale(26, 38);
	CreateDecorations();
	CreateVideo();
	CreateCards();
	CreateScanShells();

	HologramRoot->SetVisibility(false, true);
	DetectLabel->SetVisibility(false);
}

void AMuseumExhibit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMuseumLanguageSubsystem* Language = UMuseumLanguageSubsystem::Get(this)) Language->OnLanguageChanged.Remove(LanguageHandle);

	HandleMouseReleased();
	if (APlayerController* PC = LocalController.Get()) DisableInput(PC);

	Super::EndPlay(EndPlayReason);
}

void AMuseumExhibit::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateActivation();
	UpdateScan(DeltaSeconds);
	UpdatePanelDelay(DeltaSeconds);
	UpdateDetection(DeltaSeconds);
	UpdateAnimation(DeltaSeconds);
	UpdatePedestal(DeltaSeconds);
	UpdateCards(DeltaSeconds);
	UpdateVideoScreen(DeltaSeconds);
	UpdatePointer();
}

// ====== Setup ====== //

void AMuseumExhibit::ApplyData()
{
	UStaticMesh* Mesh = Data ? Data->Mesh.LoadSynchronous() : nullptr;
	ArtifactMesh->SetStaticMesh(Mesh);
	if (!Mesh) return;

	// Real size, then the twin's share of it
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const float MeshHeight = (Bounds.Origin.Z + Bounds.BoxExtent.Z) - (Bounds.Origin.Z - Bounds.BoxExtent.Z);
	const float RealScale = Data->RealHeight > 0.f && MeshHeight > KINDA_SMALL_NUMBER ? Data->RealHeight / MeshHeight : 1.f;
	const float TwinScale = TwinHeight > 0.f && MeshHeight > KINDA_SMALL_NUMBER ? TwinHeight / MeshHeight : RealScale * Data->HologramScale;
	ArtifactMesh->SetRelativeScale3D(FVector(TwinScale));

	// Click bounds around the scaled mesh
	const FBoxSphereBounds Local = ArtifactMesh->CalcBounds(ArtifactMesh->GetRelativeTransform());
	ArtifactClickBounds->SetRelativeLocation(Local.Origin);
	ArtifactClickBounds->SetBoxExtent(Local.BoxExtent + FVector(ClickBoundsPadding));
}

void AMuseumExhibit::InitializeInput()
{
	APlayerController* PC = LocalController.Get();
	if (!PC) return;

	// Mouse (or the VR trigger, which arrives as a left click): never consumed, the game keeps its keys
	EnableInput(PC);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMuseumExhibit::HandleMousePressed).bConsumeInput = false;
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMuseumExhibit::HandleMouseReleased).bConsumeInput = false;

	FInputAxisKeyBinding& WheelBinding = InputComponent->BindAxisKey(EKeys::MouseWheelAxis);
	WheelBinding.bConsumeInput = false;
	WheelBinding.AxisDelegate.BindDelegate(this, &AMuseumExhibit::HandleMouseScrolled);
}

UWidgetComponent* AMuseumExhibit::MakeWidgetComponent(AActor* Owner, USceneComponent* Parent, const TSubclassOf<UUserWidget> Class, const FName Name)
{
	UWidgetComponent* Widget = NewObject<UWidgetComponent>(Owner, Name);
	Widget->SetupAttachment(Parent);
	Widget->SetWidgetSpace(EWidgetSpace::World);
	Widget->SetWidgetClass(Class);
	Widget->SetPivot(FVector2D(.5, .5));
	Widget->SetTwoSided(false);
	Widget->SetBlendMode(EWidgetBlendMode::Transparent);
	Widget->SetCastShadow(false);
	Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Widget->SetCanEverAffectNavigation(false);
	Widget->SetCollisionResponseToAllChannels(ECR_Ignore);
	Widget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Widget->SetTranslucentSortPriority(1);
	Widget->ComponentTags.Add(InteractiveTag);
	Widget->RegisterComponent();
	Owner->AddInstanceComponent(Widget);
	Widget->InitWidget();

	return Widget;
}

void AMuseumExhibit::CreateCards()
{
	const int32 Count = (Data ? Data->Panels.Num() : 0) + (VideoPlayer ? 1 : 0);
	Cards.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FMuseumExhibitCard& Card = Cards[Index];
		Card.Root = NewObject<USceneComponent>(this, *FString::Printf(TEXT("Card%d"), Index));
		Card.Root->SetupAttachment(RootComponent);
		Card.Root->RegisterComponent();
		AddInstanceComponent(Card.Root);

		// The glass slab with its edge glow, the card widget floating just in front
		Card.Slab = MakeMesh(this, Card.Root, EShape::Cube, EMaterial::Glass, *FString::Printf(TEXT("CardSlab%d"), Index));
		SetColor(Card.Slab, Cyan());
		Card.Widget = MakeWidgetComponent(this, Card.Root, UMuseumMRInfoCardWidget::StaticClass(), *FString::Printf(TEXT("CardWidget%d"), Index));
		Card.Widget->SetRelativeLocation(FVector(SlabThickness * 0.5f + WidgetGap, 0., 0.));

		if (UMuseumMRInfoCardWidget* Widget = Cast<UMuseumMRInfoCardWidget>(Card.Widget->GetUserWidgetObject()))
		{
			Widget->OnToggle.BindUObject(this, &AMuseumExhibit::HandleCardToggle);
			Widget->OnVoice.BindUObject(this, &AMuseumExhibit::HandleCardVoice);
			Widget->OnLanguage.BindUObject(this, &AMuseumExhibit::HandleCardLanguage);
			Widget->OnVideoPlay.BindUObject(this, &AMuseumExhibit::ToggleVideo);
			Widget->OnVideoEnlarge.BindUObject(this, &AMuseumExhibit::OpenVideoScreen);
		}
		Card.Root->SetVisibility(false, true);
	}
}

void AMuseumExhibit::CreateVideo()
{
	VideoSource = Data ? Data->MediaVideo.LoadSynchronous() : nullptr;
	if (!VideoSource) return;

	// One player for the card and the screen, looping, silent (the guide's voice stays the only sound)
	VideoPlayer = NewObject<UMediaPlayer>(this, TEXT("VideoPlayer"));
	VideoPlayer->SetLooping(true);
	VideoPlayer->PlayOnOpen = true;
	VideoTexture = NewObject<UMediaTexture>(this, TEXT("VideoTexture"));
	VideoTexture->AutoClear = true;
	VideoTexture->ClearColor = FLinearColor::Black;
	VideoTexture->SetMediaPlayer(VideoPlayer);
	VideoTexture->UpdateResource();

	// The large screen: a glass slab and the screen widget in front of it, placed when it opens
	ScreenRoot = NewObject<USceneComponent>(this, TEXT("VideoScreen"));
	ScreenRoot->SetupAttachment(RootComponent);
	ScreenRoot->RegisterComponent();
	AddInstanceComponent(ScreenRoot);
	ScreenSlab = MakeMesh(this, ScreenRoot, EShape::Cube, EMaterial::Glass, TEXT("VideoScreenSlab"));
	SetColor(ScreenSlab, Cyan());
	ScreenWidget = MakeWidgetComponent(this, ScreenRoot, UMuseumMRVideoScreenWidget::StaticClass(), TEXT("VideoScreenWidget"));
	ScreenWidget->SetDrawSize(UMuseumMRVideoScreenWidget::GetDrawSize());
	ScreenWidget->SetRelativeLocation(FVector(SlabThickness * 0.5f + WidgetGap, 0., 0.));
	ScreenWidget->SetTranslucentSortPriority(3);
	if (UMuseumMRVideoScreenWidget* Widget = Cast<UMuseumMRVideoScreenWidget>(ScreenWidget->GetUserWidgetObject()))
	{
		Widget->OnPlay.BindUObject(this, &AMuseumExhibit::ToggleVideo);
		Widget->OnClose.BindUObject(this, &AMuseumExhibit::CloseVideoScreen);
	}
	ScreenRoot->SetVisibility(false, true);
}

void AMuseumExhibit::CreateDecorations()
{
	// Leader line from the cards to a glowing dot on the real object
	Leader = MakeMesh(this, RootComponent, EShape::Cube, EMaterial::Glow, TEXT("Leader"));
	SetColor(Leader, Cyan(), 4.f);
	LeaderDot = MakeMesh(this, RootComponent, EShape::Sphere, EMaterial::Glow, TEXT("LeaderDot"));
	SetColor(LeaderDot, Cyan(), 8.f);
	LeaderDot->SetRelativeScale3D(FVector(0.025));
	SetFade(Leader, 0.f);
	SetFade(LeaderDot, 0.f);

	// The floating glass pedestal under the twin, its glowing rim and the ring that climbs with the reveal
	const FBoxSphereBounds Local = ArtifactMesh->CalcBounds(ArtifactMesh->GetRelativeTransform());
	HologramHeight = FMath::Max(10.f, static_cast<float>(Local.BoxExtent.Z * 2.));
	PedestalRadius = FMath::Clamp(static_cast<float>(FVector2D(Local.BoxExtent.X, Local.BoxExtent.Y).Size()) * 0.85f, 8.f, 60.f);
	PedestalTop = static_cast<float>(Local.Origin.Z - Local.BoxExtent.Z) - 0.5f;
	const FVector2D Centre(Local.Origin.X, Local.Origin.Y);

	Pedestal = MakeMesh(this, HologramRoot, EShape::Cylinder, EMaterial::Glass, TEXT("Pedestal"));
	SetColor(Pedestal, Cyan());
	Pedestal->SetRelativeLocation(FVector(Centre.X, Centre.Y, PedestalTop - 1.5f));
	Pedestal->SetRelativeScale3D(FVector(PedestalRadius * 2.f / 100.f, PedestalRadius * 2.f / 100.f, 3.f / 100.f));

	PedestalRing = MakeMesh(this, HologramRoot, EShape::Plane, EMaterial::Ring, TEXT("PedestalRing"));
	SetColor(PedestalRing, Cyan(), 6.f);
	PedestalRing->SetRelativeLocation(FVector(Centre.X, Centre.Y, PedestalTop + 0.3f));
	PedestalRing->SetRelativeScale3D(FVector(PedestalRadius * 2.5f / 100.f));

	BuildRing = MakeMesh(this, HologramRoot, EShape::Plane, EMaterial::Ring, TEXT("BuildRing"));
	SetColor(BuildRing, Cyan(), 10.f);
	BuildRing->SetRelativeScale3D(FVector(PedestalRadius * 2.3f / 100.f));
	SetScalar(BuildRing, TEXT("Width"), 0.06f);

	SetFade(Pedestal, 0.f);
	SetFade(PedestalRing, 0.f);
	SetFade(BuildRing, 0.f);

	// The hologram's tag under the pedestal
	HologramLabel->SetRelativeLocation(FVector(Centre.X, Centre.Y, PedestalTop - 10.f));

	// The detection label above the real object, else below it, else (a tall object) in the cards' still empty zone, seen from the eye
	const FVector Eye = ViewOffset;
	const FVector Box = RealObject->GetRelativeLocation();
	const FVector Extent = RealObject->GetScaledBoxExtent();
	const FVector Top(Box.X - Extent.X, Box.Y, Box.Z + Extent.Z);
	const FVector Bottom(Box.X - Extent.X, Box.Y, Box.Z - Extent.Z);
	const float Horizontal = static_cast<float>(FVector2D(Top.X - Eye.X, Top.Y - Eye.Y).Size());
	float Yaw = Deg(FMath::Atan2(Top.Y - Eye.Y, Top.X - Eye.X));
	const float HalfLabel = Deg(FMath::Atan2(DetectLabelSize.Y * DetectLabelScale * 0.5, DetectLabelDistance));
	constexpr float ViewPitch = 22.f;
	float Pitch = Deg(FMath::Atan2(Top.Z - Eye.Z, Horizontal)) + 2.f + HalfLabel;
	if (Pitch + HalfLabel > ViewPitch) Pitch = Deg(FMath::Atan2(Bottom.Z - Eye.Z, Horizontal)) - 2.f - HalfLabel;
	if (Pitch - HalfLabel < -ViewPitch)
	{
		Yaw = (PanelZone.X + PanelZone.Z) * 0.5f;
		Pitch = 6.f;
	}
	DetectLabel->SetRelativeLocation(Eye + FRotator(Pitch, Yaw, 0.f).Vector() * DetectLabelDistance);
	DetectLabel->SetRelativeRotation(FRotator(0.f, Yaw + 180.f, 0.f));
}

// ====== API ====== //

void AMuseumExhibit::BeginScan()
{
	if (Phase != EMuseumExhibitPhase::Idle && Phase != EMuseumExhibitPhase::Hiding) return;

	if (!bScanFirst)
	{
		StartPresentation();

		return;
	}

	// A fresh visit: the passthrough runs the detection, the label locks on
	HeardPanels.Init(false, Data ? Data->Panels.Num() : 0);
	bVisitReported = false;
	Phase = EMuseumExhibitPhase::Scanning;
	ScanElapsed = 0.f;
	DetectTime = 0.f;
	bDetected = true;
	OnScanStarted.Broadcast(this);
}

void AMuseumExhibit::StartPresentation()
{
	if (Phase == EMuseumExhibitPhase::Appearing || Phase == EMuseumExhibitPhase::Shown) return;

	// Started without a scan: still a fresh visit, without the detection look
	if (Phase != EMuseumExhibitPhase::Scanning)
	{
		HeardPanels.Init(false, Data ? Data->Panels.Num() : 0);
		bVisitReported = false;
		bDetected = false;
	}

	// Phase first: a hologram without a profile reports the appear at once
	Phase = EMuseumExhibitPhase::Appearing;
	AppearElapsed = 0.f;
	FloatTime = 0.f;
	TurntableRate = 1.f;
	HologramRoot->SetVisibility(true, true);
	OnExhibitAppearStarted.Broadcast(this);

	// The title is read while the twin materializes
	if (Data) PlayVoiceOver(Data->TitleVoiceOver.Load(GetLanguage()));
	Hologram->Appear(RevealDuration);
}

void AMuseumExhibit::EndPresentation()
{
	if (Phase == EMuseumExhibitPhase::Idle || Phase == EMuseumExhibitPhase::Hiding) return;

	// Cards and guide first
	HandleMouseReleased();
	SetPanelVisible(false);
	OpenPanelIndex = INDEX_NONE;
	PanelDelayRemaining = -1.f;
	StopVoiceOver();
	StopVideo();

	// Still scanning: nothing materialized yet
	if (Phase == EMuseumExhibitPhase::Scanning)
	{
		Phase = EMuseumExhibitPhase::Idle;
		OnARInteractionEnded.Broadcast(this);
		OnExhibitHidden.Broadcast(this);

		return;
	}

	Phase = EMuseumExhibitPhase::Hiding;
	OnARInteractionEnded.Broadcast(this);
	Hologram->Disappear(HideDuration, false);
}

void AMuseumExhibit::OpenPanel(const int32 Index)
{
	if (!bPanelVisible || !Data) return;

	// One card open at a time; out of range minimizes them all
	const int32 NewIndex = Cards.IsValidIndex(Index) ? Index : INDEX_NONE;
	if (NewIndex == OpenPanelIndex) return;

	// Leaving the video card pauses it and closes its screen
	if (OpenPanelIndex != INDEX_NONE && OpenPanelIndex == GetVideoPanel())
	{
		bVideoWanted = false;
		if (VideoPlayer) VideoPlayer->Pause();
		CloseVideoScreen();
	}

	OpenPanelIndex = NewIndex;
	if (OpenPanelIndex == INDEX_NONE)
	{
		StopVoiceOver();
		RefreshCards(true);

		return;
	}

	HandlePanelOpened();
	RefreshCards(true);
}

void AMuseumExhibit::HandlePanelOpened()
{
	// The video card plays (the guide is quiet), an information card is read
	if (OpenPanelIndex == GetVideoPanel())
	{
		StopVoiceOver();
		bVideoWanted = true;
		if (VideoPlayer->GetUrl().IsEmpty() || VideoPlayer->IsClosed()) VideoPlayer->OpenSource(VideoSource);
		else VideoPlayer->Play();
	}
	else
	{
		PlayOpenPanelVoiceOver();
		if (HeardPanels.IsValidIndex(OpenPanelIndex)) HeardPanels[OpenPanelIndex] = true;
	}
	OnPanelOpened.Broadcast(this, OpenPanelIndex);

	// Every information card heard once: the visit counts
	if (!bVisitReported && HeardPanels.Num() > 0 && !HeardPanels.Contains(false))
	{
		bVisitReported = true;
		OnVisited.Broadcast(this);
	}
}

void AMuseumExhibit::TogglePanel(const int32 Index)
{
	OpenPanel(Index == OpenPanelIndex ? INDEX_NONE : Index);
}

void AMuseumExhibit::ToggleVoiceOver()
{
	if (OpenPanelIndex == INDEX_NONE) return;

	if (VoiceOver->IsPlaying()) StopVoiceOver();
	else PlayOpenPanelVoiceOver();
	RefreshCards(false);
}

bool AMuseumExhibit::IsVoiceOverPlaying() const
{
	return OpenPanelIndex != INDEX_NONE && VoiceOver->IsPlaying();
}

int32 AMuseumExhibit::GetVideoPanel() const
{
	return VideoPlayer && Data ? Data->Panels.Num() : INDEX_NONE;
}

void AMuseumExhibit::ToggleVideo()
{
	if (!VideoPlayer || !bPanelVisible) return;

	// Not open yet: opening the card plays it
	if (OpenPanelIndex != GetVideoPanel())
	{
		OpenPanel(GetVideoPanel());

		return;
	}

	bVideoWanted = !bVideoWanted;
	if (!bVideoWanted) VideoPlayer->Pause();
	else if (VideoPlayer->IsClosed()) VideoPlayer->OpenSource(VideoSource);
	else VideoPlayer->Play();
	RefreshCards(false);
	RefreshVideoScreen();
}

void AMuseumExhibit::OpenVideoScreen()
{
	if (!VideoPlayer || !ScreenRoot || !bPanelVisible || bScreenOpen) return;

	// The video card opens (and plays) with it
	if (OpenPanelIndex != GetVideoPanel()) OpenPanel(GetVideoPanel());
	if (!bVideoWanted) ToggleVideo();

	bScreenOpen = true;
	PlaceVideoScreen();
	RefreshVideoScreen();
}

void AMuseumExhibit::CloseVideoScreen()
{
	bScreenOpen = false;
}

void AMuseumExhibit::StopVideo()
{
	bVideoWanted = false;
	bScreenOpen = false;
	if (VideoPlayer) VideoPlayer->Close();
}

float AMuseumExhibit::GetScanProgress() const
{
	if (Phase == EMuseumExhibitPhase::Idle) return 0.f;

	return Phase == EMuseumExhibitPhase::Scanning ? FMath::Clamp(ScanElapsed / ScanDuration, 0.f, 1.f) : 1.f;
}

EMuseumLanguage AMuseumExhibit::GetLanguage() const
{
	return UMuseumLanguageSubsystem::GetCurrent(this);
}

TArray<UWidgetComponent*> AMuseumExhibit::GetCardWidgets() const
{
	TArray<UWidgetComponent*> Widgets;
	for (const FMuseumExhibitCard& Card : Cards)
	{
		if (Card.Widget) Widgets.Add(Card.Widget);
	}
	if (ScreenWidget) Widgets.Add(ScreenWidget);

	return Widgets;
}

FBox AMuseumExhibit::GetCardBounds() const
{
	// Where the cards stand (or will stand once the layout settles)
	FBox Bounds(ForceInit);
	const FTransform& Actor = GetActorTransform();
	for (const FMuseumExhibitCard& Card : Cards)
	{
		const FTransform Pose = Card.To * Actor;
		const FVector Half(0., Card.ToSize.X * 0.5, Card.ToSize.Y * 0.5);
		Bounds += Pose.TransformPosition(Half);
		Bounds += Pose.TransformPosition(-Half);
	}

	return Bounds;
}

FVector AMuseumExhibit::GetViewPoint() const
{
	return GetActorTransform().TransformPosition(ViewOffset);
}

// ====== Visit ====== //

bool AMuseumExhibit::IsVisitorInZone() const
{
	const APlayerController* PC = LocalController.Get();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UBoxComponent* Box = ViewingZone ? ViewingZone->FindComponentByClass<UBoxComponent>() : nullptr;
	if (!Pawn || !Box) return false;

	// Inside the box, in its own axes
	const FVector LocalPosition = Box->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
	const FVector Extent = Box->GetUnscaledBoxExtent();

	return FMath::Abs(LocalPosition.X) <= Extent.X && FMath::Abs(LocalPosition.Y) <= Extent.Y && FMath::Abs(LocalPosition.Z) <= Extent.Z;
}

FTransform AMuseumExhibit::GetPlateTransform() const
{
	return FTransform(GetActorQuat(), GetActorTransform().TransformPosition(PlateOffset));
}

void AMuseumExhibit::UpdateActivation()
{
	if (!ViewingZone) return;

	// Entering starts the visit, leaving ends it
	const bool bWasActive = bZoneActive;
	bZoneActive = IsVisitorInZone();
	if (bZoneActive && !bWasActive) BeginScan();
	else if (!bZoneActive && bWasActive) EndPresentation();
}

void AMuseumExhibit::UpdateScan(const float DeltaSeconds)
{
	if (Phase == EMuseumExhibitPhase::Appearing) AppearElapsed += DeltaSeconds;
	if (Phase != EMuseumExhibitPhase::Scanning) return;

	// Locked on: the twin appears
	ScanElapsed += DeltaSeconds;
	if (ScanElapsed >= ScanDuration) StartPresentation();
}

void AMuseumExhibit::UpdatePanelDelay(const float DeltaSeconds)
{
	if (PanelDelayRemaining < 0.f) return;

	PanelDelayRemaining -= DeltaSeconds;
	if (PanelDelayRemaining <= 0.f)
	{
		PanelDelayRemaining = -1.f;
		SetPanelVisible(true);
	}
}

void AMuseumExhibit::SetPanelVisible(const bool bVisible)
{
	if (bPanelVisible == bVisible) return;

	bPanelVisible = bVisible;
	if (!bVisible) return;

	// The first card opens with them; they stand in their places at once and fade in one after the other
	OpenPanelIndex = Cards.IsValidIndex(InitialPanel) ? InitialPanel : INDEX_NONE;
	if (OpenPanelIndex != INDEX_NONE) HandlePanelOpened();
	CardsTime = 0.f;
	CardsFade = 1.f;
	RefreshCards(true);
	LayoutAlpha = 1.f;
	for (FMuseumExhibitCard& Card : Cards)
	{
		Card.From = Card.To;
		Card.FromSize = Card.ToSize;
		Card.FromScale = Card.ToScale;
		Card.bSwapping = false;
		if (Card.bPending)
		{
			Card.bPending = false;
			Card.Shown = Card.Pending;
			Card.Widget->SetDrawSize(UMuseumMRInfoCardWidget::GetDrawSize(Card.Shown.Mode, Card.Shown.Kind));
			if (UMuseumMRInfoCardWidget* Widget = Cast<UMuseumMRInfoCardWidget>(Card.Widget->GetUserWidgetObject())) Widget->SetView(Card.Shown);
		}
	}
	OnPanelShown.Broadcast(this);
}

void AMuseumExhibit::HandleHologramAppeared()
{
	if (Phase != EMuseumExhibitPhase::Appearing) return;

	Phase = EMuseumExhibitPhase::Shown;
	ArtifactClickBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PanelDelayRemaining = PanelDelay;
	OnExhibitAppearCompleted.Broadcast(this);
}

void AMuseumExhibit::HandleHologramDisappeared()
{
	if (Phase != EMuseumExhibitPhase::Hiding) return;

	Phase = EMuseumExhibitPhase::Idle;
	HologramRoot->SetVisibility(false, true);
	ArtifactClickBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnExhibitHidden.Broadcast(this);
}

void AMuseumExhibit::HandleLanguageChanged(EMuseumLanguage)
{
	// The texts at once; the open card is read again in the new language
	if (OpenPanelIndex != INDEX_NONE && OpenPanelIndex != GetVideoPanel()) PlayOpenPanelVoiceOver();
	RefreshCards(false);
	RefreshVideoScreen();
}

void AMuseumExhibit::HandleVoiceOverFinished()
{
	RefreshCards(false);

	// A stopped clip reports late, while its replacement already plays: only a natural end with nothing playing counts
	if (VoiceOver->IsPlaying()) return;

	// Hands-free guide: the next card follows the one just read
	if (bAutoAdvance && OpenPanelIndex != INDEX_NONE && Data && Data->Panels.IsValidIndex(OpenPanelIndex) && Data->Panels.IsValidIndex(OpenPanelIndex + 1)) OpenPanel(OpenPanelIndex + 1);
}

void AMuseumExhibit::PlayVoiceOver(USoundBase* Sound)
{
	StopVoiceOver();
	if (!Sound) return;

	VoiceOver->SetSound(Sound);
	VoiceOver->Play();
	UE_LOG(LogMuseum, Log, TEXT("VOCUE play %.3f %s"), GetWorld()->GetTimeSeconds(), *Sound->GetName());
}

void AMuseumExhibit::StopVoiceOver()
{
	if (VoiceOver->IsPlaying()) UE_LOG(LogMuseum, Log, TEXT("VOCUE stop %.3f"), GetWorld()->GetTimeSeconds());
	VoiceOver->Stop();
}

void AMuseumExhibit::PlayOpenPanelVoiceOver()
{
	if (Data && Data->Panels.IsValidIndex(OpenPanelIndex)) PlayVoiceOver(Data->Panels[OpenPanelIndex].VoiceOver.Load(GetLanguage()));
}

// ====== Cards ====== //

EMuseumInfoCardKind AMuseumExhibit::GetKind(const int32 Index) const
{
	if (Index == GetVideoPanel()) return EMuseumInfoCardKind::Video;

	return Data && Data->Panels.IsValidIndex(Index) && !Data->Panels[Index].Image.IsNull() ? EMuseumInfoCardKind::Image : EMuseumInfoCardKind::Text;
}

EMuseumInfoCardMode AMuseumExhibit::GetMode(const int32 Index) const
{
	return Index == OpenPanelIndex ? EMuseumInfoCardMode::Expanded : EMuseumInfoCardMode::Minimized;
}

FMuseumInfoCardView AMuseumExhibit::MakeView(const int32 Index) const
{
	FMuseumInfoCardView View;
	View.Index = Index;
	if (!Data || !Cards.IsValidIndex(Index)) return View;

	const EMuseumLanguage Language = GetLanguage();
	View.Mode = GetMode(Index);
	View.Kind = GetKind(Index);
	View.bEnglish = Language == EMuseumLanguage::English;
	if (View.Kind == EMuseumInfoCardKind::Video)
	{
		View.Heading = View.bEnglish ? LOCTEXT("VideoHeadingEN", "VIDEO OF THE COLLECTION") : LOCTEXT("VideoHeadingID", "VIDEO KOLEKSI");
		View.Title = Data->Name.Get(Language);
		View.Body = View.bEnglish ? LOCTEXT("VideoBodyEN", "Recorded at Museum Nasional Indonesia. \"Enlarge\" opens a large screen.")
			: LOCTEXT("VideoBodyID", "Direkam di Museum Nasional Indonesia. \"Perbesar\" membuka layar besar.");
		View.Picture = VideoTexture;
		View.bVideoPlaying = bVideoWanted;

		return View;
	}

	const FMuseumExhibitPanel& Panel = Data->Panels[Index];
	View.Heading = Panel.Heading.Get(Language);
	View.Title = Panel.Title.Get(Language);
	View.Body = Panel.Body.Get(Language);
	View.Picture = View.Kind == EMuseumInfoCardKind::Image ? Panel.Image.LoadSynchronous() : nullptr;
	View.bHeard = WasPanelHeard(Index);
	View.bVoicePlaying = Index == OpenPanelIndex && VoiceOver->IsPlaying();

	return View;
}

void AMuseumExhibit::RefreshCards(const bool bLayout)
{
	if (Cards.IsEmpty()) return;

	TArray<FTransform> Poses;
	TArray<FVector2D> Sizes;
	TArray<float> Scales;
	if (bLayout) ComputeLayout(Poses, Sizes, Scales);

	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		FMuseumExhibitCard& Card = Cards[Index];
		const FMuseumInfoCardView View = MakeView(Index);
		UMuseumMRInfoCardWidget* Widget = Card.Widget ? Cast<UMuseumMRInfoCardWidget>(Card.Widget->GetUserWidgetObject()) : nullptr;

		if (bLayout)
		{
			// From where it stands now to its new place; the widget swaps its look halfway, while faded out
			Card.From = Card.Root->GetRelativeTransform();
			Card.From.SetScale3D(FVector::OneVector);
			Card.FromSize = Card.Size;
			Card.FromScale = Card.Scale;
			Card.To = Poses[Index];
			Card.ToSize = Sizes[Index];
			Card.ToScale = Scales[Index];
			Card.bSwapping = View.Mode != Card.Shown.Mode || Card.Size.IsNearlyZero();
			Card.Pending = View;
			Card.bPending = true;
			continue;
		}

		// Same layout: the texts update in place (or wait for the swap still under way)
		if (Card.bPending) Card.Pending = View;
		else
		{
			Card.Shown = View;
			if (Widget) Widget->SetView(View);
		}
	}
	if (bLayout) LayoutAlpha = 0.f;
}

float AMuseumExhibit::GetCardSide() const
{
	const FVector Eye = ViewOffset;
	const FVector Object = RealObject->GetRelativeLocation();
	const float ObjectYaw = Deg(FMath::Atan2(Object.Y - Eye.Y, Object.X - Eye.X));

	return (PanelZone.X + PanelZone.Z) * 0.5f >= ObjectYaw ? 1.f : -1.f;
}

void AMuseumExhibit::ComputeLayout(TArray<FTransform>& OutPoses, TArray<FVector2D>& OutSizes, TArray<float>& OutScales) const
{
	const int32 Count = Cards.Num();
	OutPoses.SetNum(Count);
	OutSizes.SetNum(Count);
	OutScales.Init(CardScale, Count);
	if (Count == 0) return;

	const FVector Eye = ViewOffset;
	const float Depth = NearDepth;
	auto AngleOf = [Depth](const float Centimetres) { return 2.f * Deg(FMath::Atan(Centimetres * 0.5f / Depth)); };
	auto CentimetresOf = [Depth](const float Degrees) { return 2.f * Depth * TanDeg(Degrees * 0.5f); };

	// A card at a direction, turned to the eye (tilted with its pitch), its inner edge turned away by Turn
	auto Place = [&](const int32 Index, const float Yaw, const float Pitch, const float Turn)
	{
		const FVector Location = Eye + FRotator(Pitch, Yaw, 0.f).Vector() * Depth;
		OutPoses[Index] = FTransform(FRotator(-Pitch, Yaw + 180.f + Turn, 0.f), Location);
	};

	// Cards with their own area: the largest scale (up to CardScale) at which the open card fits it; the card's anchor point on the area's
	TArray<int32> Column;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FMuseumPanelSlot* Slot = PanelSlots.IsValidIndex(Index) && PanelSlots[Index].HasArea() ? &PanelSlots[Index] : nullptr;
		if (!Slot)
		{
			Column.Add(Index);
			continue;
		}

		const FVector4f Area(Slot->Area);
		const FVector2f Open(UMuseumMRInfoCardWidget::GetDrawSize(EMuseumInfoCardMode::Expanded, GetKind(Index)));
		const float Scale = FMath::Min3(CardScale, CentimetresOf(Area.Z - Area.X) / Open.X, CentimetresOf(Area.W - Area.Y) / Open.Y);
		const FVector2D Size = UMuseumMRInfoCardWidget::GetDrawSize(GetMode(Index), GetKind(Index)) * Scale;
		const FVector2f Anchor(FMath::Clamp(static_cast<float>(Slot->Anchor.X), 0.f, 1.f), FMath::Clamp(static_cast<float>(Slot->Anchor.Y), 0.f, 1.f));
		const float Yaw = FMath::Lerp(Area.X, Area.Z, Anchor.X) + (0.5f - Anchor.X) * AngleOf(Size.X);
		const float Pitch = FMath::Lerp(Area.Y, Area.W, Anchor.Y) + (0.5f - Anchor.Y) * AngleOf(Size.Y);
		Place(Index, Yaw, Pitch, 0.f);
		OutSizes[Index] = Size;
		OutScales[Index] = Scale;
	}
	if (Column.IsEmpty()) return;

	// The column beside the object (PanelZone): bars and the open card stacked top to bottom, one scale whichever card is open
	const float Yaw0 = FMath::Min(PanelZone.X, PanelZone.Z);
	const float Yaw1 = FMath::Max(PanelZone.X, PanelZone.Z);
	const float Pitch0 = FMath::Min(PanelZone.Y, PanelZone.W);
	const float Pitch1 = FMath::Max(PanelZone.Y, PanelZone.W);
	const float Side = GetCardSide();
	const float FanCos = FMath::Cos(FMath::DegreesToRadians(Fan));
	auto Fits = [&](const float Scale, const int32 Open)
	{
		float Height = CardGap * (Column.Num() - 1);
		float Width = 0.f;
		for (const int32 Index : Column)
		{
			const FVector2D Pixels = UMuseumMRInfoCardWidget::GetDrawSize(Index == Open ? EMuseumInfoCardMode::Expanded : EMuseumInfoCardMode::Minimized, GetKind(Index));
			Height += AngleOf(Pixels.Y * Scale);
			Width = FMath::Max(Width, AngleOf(Pixels.X * Scale * FanCos));
		}

		return Height <= Pitch1 - Pitch0 && Width <= Yaw1 - Yaw0;
	};
	float Scale = CardScale;
	for (const int32 Open : Column)
	{
		if (Fits(Scale, Open)) continue;

		float Low = 0.01f, High = Scale;
		for (int32 Step = 0; Step < 24; ++Step)
		{
			const float Mid = (Low + High) * 0.5f;
			(Fits(Mid, Open) ? Low : High) = Mid;
		}
		Scale = Low;
	}

	// Centred in the zone's height, the inner edges on the zone's edge next to the object
	float Total = CardGap * (Column.Num() - 1);
	for (const int32 Index : Column) Total += AngleOf(UMuseumMRInfoCardWidget::GetDrawSize(GetMode(Index), GetKind(Index)).Y * Scale);
	float Cursor = FMath::Min(Pitch1, (Pitch0 + Pitch1) * 0.5f + Total * 0.5f);
	const float Edge = Side > 0.f ? Yaw0 : Yaw1;
	for (const int32 Index : Column)
	{
		const FVector2D Size = UMuseumMRInfoCardWidget::GetDrawSize(GetMode(Index), GetKind(Index)) * Scale;
		const float High = AngleOf(Size.Y);
		Place(Index, Edge + Side * AngleOf(Size.X * FanCos) * 0.5f, Cursor - High * 0.5f, Side * Fan);
		Cursor -= High + CardGap;
		OutSizes[Index] = Size;
		OutScales[Index] = Scale;
	}
}

void AMuseumExhibit::UpdateCards(const float DeltaSeconds)
{
	if (Cards.IsEmpty()) return;

	// Fade: staggered in from the card next to the object, all out together
	if (bPanelVisible) CardsTime += DeltaSeconds;
	else CardsFade = FMath::Max(0.f, CardsFade - DeltaSeconds / CardHideSeconds);

	// Layout change: the widgets swap their look halfway, faded out
	const float Previous = LayoutAlpha;
	LayoutAlpha = FMath::Min(1.f, LayoutAlpha + DeltaSeconds / LayoutSeconds);
	const bool bSwapNow = Previous < 0.5f && LayoutAlpha >= 0.5f;

	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		FMuseumExhibitCard& Card = Cards[Index];
		if (!Card.Root) continue;

		if (Card.bPending && (bSwapNow || LayoutAlpha >= 1.f))
		{
			const bool bWasExpanded = Card.Shown.Mode == EMuseumInfoCardMode::Expanded;
			Card.bPending = false;
			Card.Shown = Card.Pending;
			Card.Widget->SetDrawSize(UMuseumMRInfoCardWidget::GetDrawSize(Card.Shown.Mode, Card.Shown.Kind));
			if (UMuseumMRInfoCardWidget* Widget = Cast<UMuseumMRInfoCardWidget>(Card.Widget->GetUserWidgetObject()))
			{
				Widget->SetView(Card.Shown);
				if (!bWasExpanded && Card.Shown.Mode == EMuseumInfoCardMode::Expanded) Widget->ScrollToStart();
			}
		}

		// The large video screen hides the cards behind it
		const float In = Ease((CardsTime - Index * CardStagger) / CardAppearSeconds);
		ApplyCard(Card, (bPanelVisible ? In : FMath::Min(In, CardsFade)) * (1.f - Ease(ScreenFade)));
	}
	UpdateLeader();
}

void AMuseumExhibit::ApplyCard(FMuseumExhibitCard& Card, const float Visibility)
{
	const float Alpha = Ease(LayoutAlpha);
	FTransform Pose;
	Pose.Blend(Card.From, Card.To, Alpha);
	Pose.AddToTranslation(FVector(0., 0., -8. * (1. - Visibility)));
	Card.Root->SetRelativeTransform(Pose);

	Card.Size = FMath::Lerp(Card.FromSize, Card.ToSize, Alpha);
	Card.Scale = LayoutAlpha < 0.5f ? Card.FromScale : Card.ToScale;
	SetBoxSize(Card.Slab, FVector(SlabThickness, Card.Size.X, Card.Size.Y));
	Card.Widget->SetRelativeScale3D(FVector(Card.Scale));

	const bool bVisible = Visibility > 0.003f;
	Card.Root->SetVisibility(bVisible, true);
	SetFade(Card.Slab, Visibility);
	const float Swap = Card.bSwapping && LayoutAlpha < 1.f ? FMath::Abs(LayoutAlpha * 2.f - 1.f) : 1.f;
	if (UUserWidget* Widget = Card.Widget->GetUserWidgetObject()) Widget->SetRenderOpacity(Visibility * Swap);
	Card.Widget->SetCollisionEnabled(Visibility > 0.5f ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (LayoutAlpha >= 1.f) Card.bSwapping = false;
}

int32 AMuseumExhibit::GetLeaderCard() const
{
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		if (!PanelSlots.IsValidIndex(Index) || !PanelSlots[Index].HasArea()) return Index;
	}

	return INDEX_NONE;
}

void AMuseumExhibit::UpdateLeader()
{
	if (!Leader) return;

	// From the inner edge of the first column card to a dot on the object's near side
	const int32 InnerIndex = GetLeaderCard();
	const FMuseumExhibitCard* Inner = Cards.IsValidIndex(InnerIndex) ? &Cards[InnerIndex] : nullptr;
	const float Side = GetCardSide();
	const float CardFade = (bPanelVisible ? FMath::Clamp((CardsTime - CardAppearSeconds) / 0.3f, 0.f, 1.f) : CardsFade) * (1.f - Ease(ScreenFade));
	SetFade(Leader, FMath::Min(CardFade, Inner && Inner->Root && Inner->Root->IsVisible() ? 1.f : 0.f) * 0.9f);
	SetFade(LeaderDot, Leader->IsVisible() ? CardFade : 0.f);
	if (!Leader->IsVisible() || !Inner || !Inner->Root) return;

	const FTransform& Card = Inner->Root->GetComponentTransform();
	const FVector Edge = Card.TransformPosition(FVector(SlabThickness * 0.5f, Side * Inner->Size.X * 0.5f, 0.));
	const FVector Box = RealObject->GetRelativeLocation();
	const FVector Extent = RealObject->GetScaledBoxExtent();
	const FVector Dot = GetActorTransform().TransformPosition(FVector(Box.X - Extent.X, Box.Y + Side * Extent.Y * 0.55f, Box.Z + Extent.Z * 0.1f));
	SetLine(Leader, Edge, Dot, 0.35f);
	LeaderDot->SetWorldLocation(Dot);
}

void AMuseumExhibit::HandleCardToggle(const int32 Index)
{
	TogglePanel(Index);
}

void AMuseumExhibit::HandleCardVoice()
{
	ToggleVoiceOver();
}

void AMuseumExhibit::HandleCardLanguage()
{
	if (UMuseumLanguageSubsystem* Language = UMuseumLanguageSubsystem::Get(this)) Language->ToggleLanguage();
}

// ====== Video screen ====== //

void AMuseumExhibit::PlaceVideoScreen()
{
	// Straight ahead of the visitor's eyes (level-ish), as wide as VideoScreenShare of the view
	FVector Eye = GetViewPoint();
	FRotator View = GetActorRotation();
	float HorizontalFov = 90.f;
	if (const APlayerController* PC = LocalController.Get())
	{
		PC->GetPlayerViewPoint(Eye, View);
		if (PC->PlayerCameraManager) HorizontalFov = PC->PlayerCameraManager->GetFOVAngle();
	}
	const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(View.Pitch), -12.f, 12.f);
	const FVector Direction = FRotator(Pitch, View.Yaw, 0.f).Vector();
	const FVector2D Pixels = UMuseumMRVideoScreenWidget::GetDrawSize();
	const float Width = VideoScreenShare * 2.f * VideoScreenDistance * TanDeg(HorizontalFov * 0.5f);
	const float Scale = Width / Pixels.X;
	ScreenSize = Pixels * Scale;

	ScreenRoot->SetWorldLocationAndRotation(Eye + Direction * VideoScreenDistance, FRotator(-Pitch, View.Yaw + 180.f, 0.f));
	ScreenWidget->SetRelativeScale3D(FVector(Scale));
	SetBoxSize(ScreenSlab, FVector(SlabThickness, ScreenSize.X, ScreenSize.Y));
}

void AMuseumExhibit::RefreshVideoScreen()
{
	UMuseumMRVideoScreenWidget* Widget = ScreenWidget ? Cast<UMuseumMRVideoScreenWidget>(ScreenWidget->GetUserWidgetObject()) : nullptr;
	if (!Widget || !Data) return;

	FMuseumVideoScreenView View;
	View.Title = Data->Name.Get(GetLanguage());
	View.Video = VideoTexture;
	View.bPlaying = bVideoWanted;
	View.bEnglish = GetLanguage() == EMuseumLanguage::English;
	Widget->SetView(View);
}

void AMuseumExhibit::UpdateVideoScreen(const float DeltaSeconds)
{
	if (!ScreenRoot) return;

	// Fades and rises in while open, out when closed or when the cards go
	const bool bOpen = bScreenOpen && bPanelVisible;
	ScreenFade = FMath::FInterpConstantTo(ScreenFade, bOpen ? 1.f : 0.f, DeltaSeconds, 1.f / ScreenSeconds);
	const float Alpha = Ease(ScreenFade);
	const bool bVisible = Alpha > 0.003f;
	if (ScreenRoot->IsVisible() != bVisible) ScreenRoot->SetVisibility(bVisible, true);
	// The hologram's halo is a post-process outline from custom depth: the translucent screen writes no depth, so the outline
	// would show through it. Off while the screen stands.
	const bool bHalo = Alpha < 0.1f;
	if (ArtifactMesh && ArtifactMesh->bRenderCustomDepth != bHalo) ArtifactMesh->SetRenderCustomDepth(bHalo);
	if (!bVisible) return;

	ScreenWidget->SetRelativeLocation(FVector(SlabThickness * 0.5f + WidgetGap, 0., -ScreenRise * (1.f - Alpha)));
	SetFade(ScreenSlab, Alpha);
	if (UUserWidget* Widget = ScreenWidget->GetUserWidgetObject()) Widget->SetRenderOpacity(Alpha);
	ScreenWidget->SetCollisionEnabled(Alpha > 0.5f ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

// ====== Animation ====== //

void AMuseumExhibit::UpdateAnimation(const float DeltaSeconds)
{
	if (Phase == EMuseumExhibitPhase::Idle || Phase == EMuseumExhibitPhase::Scanning) return;

	// Float
	FloatTime += DeltaSeconds;
	const float Bob = FMath::Sin(FloatTime * 2.f * PI * FloatFrequency) * FloatAmplitude;
	HologramRoot->SetRelativeLocation(RestHologramLocation + FVector(0., 0., Bob));

	// Turntable, easing to a stop while a card is read
	TurntableRate = FMath::FInterpTo(TurntableRate, IsVoiceOverPlaying() || bScreenOpen ? 0.f : 1.f, DeltaSeconds, TurntableEaseSpeed);
	TurntableYaw = FMath::Fmod(TurntableYaw + TurntableSpeed * TurntableRate * DeltaSeconds, 360.f);
	ArtifactMesh->SetRelativeRotation(FRotator(0., TurntableYaw, 0.));
}

void AMuseumExhibit::UpdateDetection(const float DeltaSeconds)
{
	// The detection look: up while scanning, back to plain passthrough while the twin materializes
	float LabelFade = 0.f;
	if (Phase == EMuseumExhibitPhase::Scanning)
	{
		DetectTime += DeltaSeconds;
		DetectAmount = Ease(ScanElapsed / DetectRampSeconds);
		LabelFade = Ease(ScanElapsed / 0.4f);
	}
	else if (Phase == EMuseumExhibitPhase::Appearing && bDetected)
	{
		DetectTime += DeltaSeconds;
		DetectAmount = 1.f - Ease(AppearElapsed / (RevealDuration * DetectFadeShare));
		LabelFade = 1.f - Ease((AppearElapsed - 0.3f) / 0.5f);
	}
	else DetectAmount = FMath::FInterpConstantTo(DetectAmount, 0.f, DeltaSeconds, 2.f);

	UpdateScanShells();

	DetectLabel->SetVisibility(LabelFade > 0.003f);
	if (LabelFade <= 0.003f) return;

	UMuseumMRLabelWidget* Label = Cast<UMuseumMRLabelWidget>(DetectLabel->GetUserWidgetObject());
	if (!Label) return;

	// Scanning, then locked on
	const bool bEnglish = GetLanguage() == EMuseumLanguage::English;
	const float Progress = GetScanProgress();
	const bool bLocked = Progress >= 0.72f;
	const FText Kicker = bLocked ? (bEnglish ? LOCTEXT("DetectedEN", "OBJECT DETECTED") : LOCTEXT("DetectedID", "OBJEK TERDETEKSI"))
		: (bEnglish ? LOCTEXT("ScanningEN", "SCANNING OBJECT…") : LOCTEXT("ScanningID", "MEMINDAI OBJEK…"));
	const FText Name = Data ? Data->Name.Get(GetLanguage()) : FText::GetEmpty();
	Label->SetLabel(Kicker, bLocked ? Name : (bEnglish ? LOCTEXT("ScanNameEN", "Matching the collection…") : LOCTEXT("ScanNameID", "Mencocokkan koleksi…")), FMath::Min(1.f, Progress / 0.72f), bLocked ? Green() : Cyan());
	Label->SetRenderOpacity(LabelFade);
	FaceViewer(DetectLabel);
}

void AMuseumExhibit::CreateScanShells()
{
	if (!RealActor || !ScanMaterial) return;

	// One shell per mesh of the real object, a hair larger so the scan sits on its surface
	ScanInstance = UMaterialInstanceDynamic::Create(ScanMaterial, this);
	TArray<UStaticMeshComponent*> Meshes;
	RealActor->GetComponents(Meshes);
	FBox Bounds(ForceInit);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!Mesh->GetStaticMesh() || !Mesh->IsVisible()) continue;

		UStaticMeshComponent* Shell = NewObject<UStaticMeshComponent>(this);
		Shell->SetupAttachment(Mesh);
		Shell->SetStaticMesh(Mesh->GetStaticMesh());
		Shell->SetRelativeScale3D(FVector(1.004));
		Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shell->SetCanEverAffectNavigation(false);
		Shell->SetCastShadow(false);
		Shell->SetAffectDistanceFieldLighting(false);
		Shell->bAffectDynamicIndirectLighting = false;
		for (int32 Slot = 0; Slot < Shell->GetNumMaterials(); ++Slot) Shell->SetMaterial(Slot, ScanInstance);
		Shell->SetVisibility(false);
		Shell->RegisterComponent();
		ScanShells.Add(Shell);
		Bounds += Mesh->Bounds.GetBox();
	}

	// The scan climbs from the object's base to its top
	if (Bounds.IsValid)
	{
		ScanInstance->SetScalarParameterValue(TEXT("ScanBottom"), static_cast<float>(Bounds.Min.Z));
		ScanInstance->SetScalarParameterValue(TEXT("ScanTop"), static_cast<float>(Bounds.Max.Z));
	}
}

void AMuseumExhibit::UpdateScanShells()
{
	if (!ScanInstance) return;

	const bool bShown = DetectAmount > 0.001f;
	for (UStaticMeshComponent* Shell : ScanShells)
	{
		if (Shell->IsVisible() != bShown) Shell->SetVisibility(bShown);
	}
	if (!bShown) return;

	ScanInstance->SetScalarParameterValue(TEXT("DetectAmount"), DetectAmount);
	ScanInstance->SetScalarParameterValue(TEXT("DetectTime"), DetectTime);
}

void AMuseumExhibit::UpdatePedestal(const float DeltaSeconds)
{
	if (!Pedestal) return;

	// The pedestal glows in with the twin and fades with it
	const bool bUp = Phase == EMuseumExhibitPhase::Appearing || Phase == EMuseumExhibitPhase::Shown;
	PedestalFade = FMath::FInterpConstantTo(PedestalFade, bUp ? 1.f : 0.f, DeltaSeconds, bUp ? 2.5f : 1.5f);
	SetFade(Pedestal, PedestalFade);
	SetFade(PedestalRing, PedestalFade);

	// The build ring climbs from the feet to the top with the reveal
	const float Reveal = Phase == EMuseumExhibitPhase::Appearing ? FMath::Clamp(AppearElapsed / RevealDuration, 0.f, 1.f) : 1.f;
	const float RingFade = Phase == EMuseumExhibitPhase::Appearing ? FMath::Min(1.f, Reveal * 8.f) * (1.f - FMath::Pow(Reveal, 6.f)) : 0.f;
	if (BuildRing)
	{
		const FVector Base = PedestalRing->GetRelativeLocation();
		BuildRing->SetRelativeLocation(FVector(Base.X, Base.Y, PedestalTop + HologramHeight * Ease(Reveal)));
		SetFade(BuildRing, RingFade);
	}

	// "Hologram · name" once the twin stands
	const float LabelFade = PedestalFade * (Phase == EMuseumExhibitPhase::Appearing ? Ease((Reveal - 0.6f) / 0.4f) : 1.f);
	HologramLabel->SetVisibility(LabelFade > 0.003f);
	if (LabelFade <= 0.003f) return;

	if (UMuseumMRLabelWidget* Label = Cast<UMuseumMRLabelWidget>(HologramLabel->GetUserWidgetObject()))
	{
		const FText Name = Data ? Data->Name.Get(GetLanguage()) : FText::GetEmpty();
		Label->SetLabel(FText::GetEmpty(), FText::Format(LOCTEXT("HologramTag", "Hologram · {0}"), Name));
		Label->SetRenderOpacity(LabelFade);
	}
	FaceViewer(HologramLabel);
}

void AMuseumExhibit::FaceViewer(USceneComponent* Component) const
{
	if (!Component) return;

	// Upright, turned to the visitor's eyes (the live camera, else the viewing spot)
	FVector Eye = GetViewPoint();
	if (const APlayerController* PC = LocalController.Get())
	{
		FRotator Rotation;
		PC->GetPlayerViewPoint(Eye, Rotation);
	}
	const FVector ToEye = Eye - Component->GetComponentLocation();
	Component->SetWorldRotation(FRotator(0., ToEye.Rotation().Yaw, 0.));
}

// ====== Pointer ====== //

void AMuseumExhibit::UpdatePointer()
{
	TArray<UWidgetComponent*> Targets;
	if (Phase == EMuseumExhibitPhase::Shown && bPanelVisible)
	{
		for (UWidgetComponent* Widget : GetCardWidgets())
		{
			if (bScreenOpen && Widget != ScreenWidget) continue;
			if (Widget->IsVisible() && Widget->GetCollisionEnabled() != ECollisionEnabled::NoCollision) Targets.Add(Widget);
		}
	}
	Pointer->UpdateTargets(LocalController.Get(), Targets);
}

void AMuseumExhibit::HandleMouseScrolled(const float Value)
{
	if (!bPanelVisible || Value == 0.f) return;

	UpdatePointer();
	Pointer->Scroll(Value);
}

void AMuseumExhibit::HandleMousePressed()
{
	if (Phase != EMuseumExhibitPhase::Shown) return;

	// The cards' buttons
	UpdatePointer();
	if (Pointer->Press()) return;

	// The hologram brings the cards up early
	if (!bPanelVisible && Pointer->RayHits(LocalController.Get(), ArtifactClickBounds))
	{
		PanelDelayRemaining = -1.f;
		SetPanelVisible(true);
	}
}

void AMuseumExhibit::HandleMouseReleased()
{
	if (Pointer) Pointer->Release();
}

#undef LOCTEXT_NAMESPACE
