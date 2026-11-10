#include "MR/MuseumMRHud.h"

#include "Algo/Sort.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Exhibit/MuseumExhibit.h"
#include "Exhibit/MuseumExhibitData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "MR/MuseumMRHudWidgets.h"
#include "MR/MuseumMRMinimap.h"
#include "MR/MuseumMRPointer.h"
#include "MR/MuseumMRWaypoints.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "PathGuide/World/PathGuideDestination.h"
#include "PathGuide/World/PathGuideRouteActor.h"
#include "Engine/Texture2D.h"
#include "Player/MuseumPlayerController.h"

#define LOCTEXT_NAMESPACE "MuseumMRHud"

using namespace MuseumMR;

namespace
{
	/** HUD bar: distance of its arc from the eye, how far below the eye, arc length (cm). */
	constexpr float BarDistance = 95.f;
	constexpr float BarPitch = -19.f;
	constexpr float BarArcLength = 112.f;

	/** Menu: distance, arc length, slightly below eye level. */
	constexpr float MenuDistance = 130.f;
	constexpr float MenuArcLength = 150.f;
	constexpr float MenuPitch = -4.f;

	/** The bar follows the head once it looks this far away, at this speed. */
	constexpr float FollowThreshold = 18.f;
	constexpr float FollowSpeed = 3.5f;
	constexpr float FollowFreezePitch = -9.f;
	/** While walking (faster than this, cm/s) the bar keeps closer to the head: the wide bar must not leave the view. */
	constexpr float WalkingSpeed = 20.f;
	constexpr float WalkingFollowThreshold = 6.f;
	constexpr float FrontMapHalfWidth = 22.f;
	/** After "Ya" on the map it stays in front this long (s): the route draws itself on it. */
	constexpr float MapRouteShowTime = 2.6f;

	float ArcDegrees(const float Length, const float Distance)
	{
		return FMath::RadiansToDegrees(Length / Distance);
	}

	bool GetEye(const APlayerController* Player, FVector& OutEye, FRotator& OutRotation)
	{
		if (!Player) return false;

		Player->GetPlayerViewPoint(OutEye, OutRotation);

		return true;
	}
}

// ====== Lifecycle ====== //

AMuseumMRHud::AMuseumMRHud()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	auto MakeCurved = [this](const TCHAR* Name, const TSubclassOf<UUserWidget> Class, const FVector2D& DrawSize, const float Length, const float Distance)
	{
		UWidgetComponent* Widget = CreateDefaultSubobject<UWidgetComponent>(Name);
		Widget->SetupAttachment(RootComponent);
		Widget->SetUsingAbsoluteLocation(true);
		Widget->SetUsingAbsoluteRotation(true);
		Widget->SetUsingAbsoluteScale(true);
		Widget->SetWidgetSpace(EWidgetSpace::World);
		Widget->SetWidgetClass(Class);
		Widget->SetDrawSize(DrawSize);
		Widget->SetPivot(FVector2D(.5, .5));
		Widget->SetGeometryMode(EWidgetGeometryMode::Cylinder);
		Widget->SetCylinderArcAngle(ArcDegrees(Length, Distance));
		Widget->SetWorldScale3D(FVector(Length / DrawSize.X));
		Widget->SetTwoSided(false);
		Widget->SetBlendMode(EWidgetBlendMode::Transparent);
		Widget->SetCastShadow(false);
		Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Widget->SetCanEverAffectNavigation(false);
		Widget->SetCollisionResponseToAllChannels(ECR_Ignore);
		Widget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Widget->SetTranslucentSortPriority(3);
		Widget->ComponentTags.Add(TEXT("MuseumMRInteractive"));

		return Widget;
	};
	Bar = MakeCurved(TEXT("Bar"), UMuseumMRHudBarWidget::StaticClass(), UMuseumMRHudBarWidget::GetDrawSize(), BarArcLength, BarDistance);
	Menu = MakeCurved(TEXT("Menu"), UMuseumMRMenuWidget::StaticClass(), UMuseumMRMenuWidget::GetDrawSize(), MenuArcLength, MenuDistance);

	Pointer = CreateDefaultSubobject<UMuseumMRPointer>(TEXT("Pointer"));
	Pointer->SetupAttachment(RootComponent);
}

void AMuseumMRHud::BeginPlay()
{
	Super::BeginPlay();
	Pointer->PointerIndex = PointerIndex;

	Bar->InitWidget();
	Menu->InitWidget();
	if (UMuseumMRHudBarWidget* Widget = Cast<UMuseumMRHudBarWidget>(Bar->GetUserWidgetObject()))
	{
		Widget->OnNavigate.BindUObject(this, &AMuseumMRHud::HandleNavigate);
		Widget->OnMinimap.BindUObject(this, &AMuseumMRHud::HandleMinimap);
	}
	if (UMuseumMRMenuWidget* Widget = Cast<UMuseumMRMenuWidget>(Menu->GetUserWidgetObject()))
	{
		Widget->OnSelect.BindUObject(this, &AMuseumMRHud::HandleSelect);
		Widget->OnClose.BindUObject(this, &AMuseumMRHud::HandleMenuClose);
		Widget->OnLanguage.BindUObject(this, &AMuseumMRHud::HandleMenuLanguage);
	}
	SetShown(Bar, 0.f);
	SetShown(Menu, 0.f);

	// The tour's exhibits in tour order; a visit counts on the bar and the map
	for (TActorIterator<AMuseumExhibit> It(GetWorld()); It; ++It)
	{
		if (!It->Data) continue;
		Exhibits.Add(*It);
		It->OnVisited.AddDynamic(this, &AMuseumMRHud::HandleExhibitVisited);
	}
	Algo::Sort(Exhibits, [](const TObjectPtr<AMuseumExhibit>& A, const TObjectPtr<AMuseumExhibit>& B) { return A->Data->Number < B->Data->Number; });

	FActorSpawnParameters Params;
	Params.Owner = this;
	Minimap = GetWorld()->SpawnActor<AMuseumMRMinimap>(Params);
	Waypoints = GetWorld()->SpawnActor<AMuseumMRWaypoints>(Params);
	if (Minimap)
	{
		Minimap->OnMinimize.BindUObject(this, &AMuseumMRHud::HandleMapMinimize);
		Minimap->OnConfirm.BindUObject(this, &AMuseumMRHud::HandleMapConfirm);
	}
	BuildMap();
}

void AMuseumMRHud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Controller)
	{
		Controller->OnPlacesChanged.RemoveAll(this);
		Controller->OnMenuOpenChanged.RemoveAll(this);
		Controller->OnNavigateButtonChanged.RemoveAll(this);
		Controller->OnLanguageChanged.RemoveAll(this);
	}
	if (APlayerController* PC = Player.Get()) DisableInput(PC);
	if (Minimap) Minimap->Destroy();
	if (Waypoints) Waypoints->Destroy();

	Super::EndPlay(EndPlayReason);
}

void AMuseumMRHud::Setup(APlayerController* InPlayer, UMuseumPathGuideWidgetController* InController)
{
	Player = InPlayer;
	Controller = InController;
	if (Controller)
	{
		Controller->OnPlacesChanged.AddDynamic(this, &AMuseumMRHud::HandlePlacesChanged);
		Controller->OnMenuOpenChanged.AddDynamic(this, &AMuseumMRHud::HandleMenuOpenChanged);
		Controller->OnNavigateButtonChanged.AddDynamic(this, &AMuseumMRHud::HandleNavigateButtonChanged);
		Controller->OnLanguageChanged.AddDynamic(this, &AMuseumMRHud::HandleLanguageChanged);
	}

	// Clicks (the VR trigger arrives as a left click) and the wheel, never consumed
	if (InPlayer)
	{
		EnableInput(InPlayer);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMuseumMRHud::HandlePressed).bConsumeInput = false;
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMuseumMRHud::HandleReleased).bConsumeInput = false;
		FInputAxisKeyBinding& Wheel = InputComponent->BindAxisKey(EKeys::MouseWheelAxis);
		Wheel.bConsumeInput = false;
		Wheel.AxisDelegate.BindDelegate(this, &AMuseumMRHud::HandleScrolled);
	}

	// The default button until the controller sends its own
	NavigateView.ButtonText = IsEnglish() ? LOCTEXT("NavigateEN", "Navigate") : LOCTEXT("NavigateID", "Navigasi");
	RefreshBar();
}

void AMuseumMRHud::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	FVector Eye;
	FRotator View;
	if (!GetEye(Player.Get(), Eye, View)) return;

	// The bar's direction trails the head
	if (!bFollowing)
	{
		FollowYaw = View.Yaw;
		bFollowing = true;
	}
	// It holds still while the visitor stands looking down at it or points at it (else it runs from the eyes reaching its ends);
	// a visitor walking with the head down (at the floor arrows) is not using it, so it keeps following
	const APawn* Pawn = Player.IsValid() ? Player->GetPawn() : nullptr;
	const bool bWalking = Pawn && Pawn->GetVelocity().Size2D() > WalkingSpeed;
	const bool bUsingBar = (!bWalking && FRotator::NormalizeAxis(View.Pitch) < FollowFreezePitch) || (Pointer->GetTarget() == Bar && BarShown > 0.5f);
	const float Threshold = bWalking ? WalkingFollowThreshold : FollowThreshold;
	const float Off = FMath::FindDeltaAngleDegrees(FollowYaw, View.Yaw);
	if (!bUsingBar && FMath::Abs(Off) > Threshold) FollowYaw += Off * FMath::Clamp(DeltaSeconds * FollowSpeed, 0.f, 1.f) * (1.f - Threshold / FMath::Abs(Off) * 0.5f);
	FollowYaw = FRotator::NormalizeAxis(FollowYaw);

	const bool bPresenting = IsPresenting();
	const bool bGuiding = IsGuiding();
	if (bGuiding && !bWasGuiding) bSideDismissed = false;
	if (bPresenting) bMapFront = false;
	bWasGuiding = bGuiding;
	if (!bMapFront) MapCloseTimer = 0.f;
	if (MapCloseTimer > 0.f)
	{
		MapCloseTimer -= DeltaSeconds;
		if (MapCloseTimer <= 0.f)
		{
			MapCloseTimer = 0.f;
			bMapFront = false;
			bSideDismissed = false;
		}
	}

	// The bar, low in front; folded away while the menu or the map in front is open, or an exhibit presents itself
	BarShown = FMath::FInterpConstantTo(BarShown, !bPresenting && !bMenuOpen && !bMapFront ? 1.f : 0.f, DeltaSeconds, 3.f);
	PlaceCurved(Bar, Eye - FVector(0., 0., 4.f * (1.f - Ease(BarShown))), FollowYaw, BarPitch, BarDistance, ArcDegrees(BarArcLength, BarDistance));
	SetShown(Bar, Ease(BarShown));

	// The menu stays where the visitor looked when it opened
	MenuShown = FMath::FInterpConstantTo(MenuShown, bMenuOpen ? 1.f : 0.f, DeltaSeconds, 3.5f);
	const float MenuAlpha = Ease(MenuShown);
	PlaceCurved(Menu, MenuEye, MenuYaw, MenuPitch, MenuDistance + 25.f * (1.f - MenuAlpha), ArcDegrees(MenuArcLength, MenuDistance));
	SetShown(Menu, MenuAlpha);

	// The map: in front on request, beside the view while guiding, else folded into the bar's disc
	EMuseumMinimapMode Mode = EMuseumMinimapMode::Hidden;
	if (!bPresenting && !bMenuOpen)
	{
		if (bMapFront) Mode = EMuseumMinimapMode::Front;
		else if (bGuiding && !bSideDismissed) Mode = EMuseumMinimapMode::Side;
	}
	const AMuseumPlayerController* Visitor = Cast<AMuseumPlayerController>(Player.Get());
	const UWzPathGuideComponent* PathGuide = Visitor ? Visitor->GetPathGuide() : nullptr;
	static const TArray<FVector> NoRoute;
	const TArray<FVector>& Route = bGuiding && PathGuide ? PathGuide->GetRoute() : NoRoute;
	const FVector Feet = Pawn ? Pawn->GetActorLocation() : Eye;
	// The map in front never covers the exhibit the visitor stands at: it opens on the free side instead
	float MapYaw = FollowYaw;
	if (Mode == EMuseumMinimapMode::Front) GetFreeYaw(MapYaw);
	if (Minimap) Minimap->UpdateMap(DeltaSeconds, Mode, Eye, MapYaw, Feet, View.Yaw, GetVisited(), Route, bGuiding ? GetActiveNumber() : 0, IsEnglish(), Player.Get());

	// The dotted floor path and the destination pin; the plugin's own route stays hidden
	if (Waypoints) Waypoints->UpdateWaypoints(DeltaSeconds, bGuiding && !bPresenting ? Route : NoRoute, Eye, GetActiveName(), PathGuide ? PathGuide->GetRemainingDistance() : 0.f);
	for (TActorIterator<AWzPathGuideRouteActor> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden()) It->SetActorHiddenInGame(true);
	}

	RefreshBar();

	// The pointer on whatever HUD piece it crosses
	TArray<UWidgetComponent*> Targets;
	if (BarShown > 0.5f) Targets.Add(Bar);
	if (MenuShown > 0.5f) Targets.Add(Menu);
	if (Minimap && Minimap->GetShown() > 0.5f) Targets.Add(Minimap->GetHeader());
	if (Minimap && Minimap->IsConfirmOpen()) Targets.Add(Minimap->GetConfirm());
	Pointer->UpdateTargets(Player.Get(), Targets);
}

// ====== Controller events ====== //

void AMuseumMRHud::HandlePlacesChanged(const TArray<FMuseumPlaceCardView>& Places)
{
	PlaceViews = Places;
	RefreshMenu();
}

void AMuseumMRHud::HandleMenuOpenChanged(const bool bOpen)
{
	if (bOpen && !bMenuOpen)
	{
		// Opens where the visitor looks
		FRotator View;
		if (GetEye(Player.Get(), MenuEye, View)) MenuYaw = View.Yaw;
		bMapFront = false;
	}
	bMenuOpen = bOpen;
	RefreshMenu();
}

void AMuseumMRHud::HandleNavigateButtonChanged(const FMuseumNavigateButtonView& Button)
{
	NavigateView = Button;
	RefreshBar();
}

void AMuseumMRHud::HandleLanguageChanged(const FText&)
{
	RefreshBar();
	RefreshMenu();
}

void AMuseumMRHud::HandleExhibitVisited(AMuseumExhibit* Exhibit)
{
	if (Exhibit && Exhibit->Data) VisitedPlaces.Add(Exhibit->Data->PlaceTag);
	RefreshBar();
}

// ====== Widget events ====== //

void AMuseumMRHud::HandleNavigate()
{
	if (Controller) Controller->ToggleMenu();
}

void AMuseumMRHud::HandleMinimap()
{
	bMapFront = !bMapFront;
	if (!bMapFront && IsGuiding()) bSideDismissed = true;
}

void AMuseumMRHud::HandleMapMinimize()
{
	bMapFront = false;
	if (IsGuiding()) bSideDismissed = true;
}

void AMuseumMRHud::HandleMapConfirm(const int32 Number)
{
	// "Ya, pandu saya" on the map: guided there; the route draws itself on the map, then the map goes back beside the view
	const AMuseumExhibit* Exhibit = FindExhibit(Number);
	if (!Exhibit || !Controller) return;

	Controller->SelectPlace(Exhibit->Data->PlaceTag);
	MapCloseTimer = MapRouteShowTime;
}

void AMuseumMRHud::HandleSelect(const FGameplayTag& PlaceTag)
{
	if (Controller) Controller->SelectPlace(PlaceTag);
}

void AMuseumMRHud::HandleMenuClose()
{
	if (Controller) Controller->CloseMenu();
}

void AMuseumMRHud::HandleMenuLanguage()
{
	if (Controller) Controller->ToggleLanguage();
}

// ====== Input ====== //

void AMuseumMRHud::HandlePressed()
{
	if (Pointer->Press()) return;

	// No widget under the pointer: a stop on the map in front asks to guide there, the tray itself is grabbed to spin it
	const int32 Number = Minimap ? Minimap->GetHoveredNumber() : 0;
	const AMuseumExhibit* Exhibit = FindExhibit(Number);
	if (!Exhibit)
	{
		BeginMapDrag();

		return;
	}

	const EMuseumLanguage Language = UMuseumLanguageSubsystem::GetCurrent(this);
	Minimap->PressStop(Number, Exhibit->Data->Name.Get(Language), Exhibit->Data->Thumbnail.LoadSynchronous(), Language == EMuseumLanguage::English);
}

void AMuseumMRHud::HandleReleased()
{
	Pointer->Release();
	EndMapDrag();
}

bool AMuseumMRHud::BeginMapDrag()
{
	// Not while the route draws itself after "Ya"
	return Minimap && MapCloseTimer <= 0.f && Minimap->BeginDrag(Player.Get());
}

void AMuseumMRHud::EndMapDrag()
{
	if (Minimap) Minimap->EndDrag();
}

void AMuseumMRHud::HandleScrolled(const float Value)
{
	if (Value != 0.f) Pointer->Scroll(Value);
}

// ====== Helpers ====== //

void AMuseumMRHud::RefreshBar()
{
	UMuseumMRHudBarWidget* Widget = Bar ? Cast<UMuseumMRHudBarWidget>(Bar->GetUserWidgetObject()) : nullptr;
	if (!Widget) return;

	const bool bEnglish = IsEnglish();
	const bool bGuiding = IsGuiding();
	FMuseumHudBarView View;
	View.bEnglish = bEnglish;
	View.bGuiding = bGuiding;
	View.bMapOpen = bMapFront;
	View.Visited = GetVisited();
	View.NavigateText = bGuiding ? (bEnglish ? LOCTEXT("GuidingEN", "GUIDING") : LOCTEXT("GuidingID", "MEMANDU")) : NavigateView.ButtonText.ToUpper();
	View.NavigateDetail = bGuiding ? NavigateView.DistanceText : (bEnglish ? LOCTEXT("ChooseEN", "Choose a destination") : LOCTEXT("ChooseID", "Pilih tujuan"));

	int32 Count = 0;
	for (const bool bVisited : View.Visited) Count += bVisited ? 1 : 0;
	if (bGuiding)
	{
		View.Message = FText::Format(bEnglish ? LOCTEXT("ToEN", "To {0}") : LOCTEXT("ToID", "Menuju {0}"), GetActiveName());
		View.MessageDetail = bEnglish ? LOCTEXT("FollowEN", "Follow the arrows on the floor") : LOCTEXT("FollowID", "Ikuti panah di lantai");
	}
	else if (Count == 0)
	{
		View.Message = bEnglish ? LOCTEXT("WelcomeEN", "Welcome to Museum Nasional") : LOCTEXT("WelcomeID", "Selamat datang di Museum Nasional");
		View.MessageDetail = bEnglish ? LOCTEXT("StartEN", "Press NAVIGATE to choose where to go") : LOCTEXT("StartID", "Tekan NAVIGASI untuk memilih tujuan");
	}
	else
	{
		View.Message = Count == View.Visited.Num() ? (bEnglish ? LOCTEXT("DoneEN", "Tour complete, thank you!") : LOCTEXT("DoneID", "Tur selesai, terima kasih!"))
			: (bEnglish ? LOCTEXT("NextEN", "Continue the tour") : LOCTEXT("NextID", "Lanjutkan tur"));
		View.MessageDetail = bEnglish ? LOCTEXT("PickEN", "Choose the next collection") : LOCTEXT("PickID", "Pilih koleksi berikutnya");
	}
	Widget->SetView(View);
}

void AMuseumMRHud::RefreshMenu()
{
	UMuseumMRMenuWidget* Widget = Menu ? Cast<UMuseumMRMenuWidget>(Menu->GetUserWidgetObject()) : nullptr;
	if (!Widget || PlaceViews.IsEmpty()) return;

	TArray<FMuseumMenuPlace> Places;
	for (const FMuseumPlaceCardView& View : PlaceViews)
	{
		FMuseumMenuPlace& Place = Places.AddDefaulted_GetRef();
		Place.PlaceTag = View.PlaceTag;
		Place.Number = View.Number;
		Place.Name = View.DisplayName;
		Place.Distance = View.DistanceText;
		Place.Thumbnail = View.Thumbnail;
		Place.bActive = View.bIsActiveDestination;
		Place.bVisited = VisitedPlaces.Contains(View.PlaceTag);

		// The exhibit's own number, name and picture (the same as on its cards), where the place cards have none
		for (const AMuseumExhibit* Exhibit : Exhibits)
		{
			if (!IsValid(Exhibit) || !Exhibit->Data || Exhibit->Data->PlaceTag != View.PlaceTag) continue;

			Place.Number = Exhibit->Data->Number;
			Place.Name = Exhibit->Data->Name.Get(UMuseumLanguageSubsystem::GetCurrent(this));
			if (!Place.Thumbnail) Place.Thumbnail = Exhibit->Data->Thumbnail.LoadSynchronous();
			break;
		}
	}
	Places.Sort([](const FMuseumMenuPlace& A, const FMuseumMenuPlace& B) { return A.Number < B.Number; });
	Widget->SetPlaces(Places, IsEnglish());
}

bool AMuseumMRHud::GetFreeYaw(float& OutYaw) const
{
	for (const AMuseumExhibit* Exhibit : Exhibits)
	{
		if (!IsValid(Exhibit) || !Exhibit->IsZoneActive()) continue;

		// The zone's angles are in exhibit space, seen from its viewing point. The map hugs the zone's inner edge (next to the
		// object, the edge nearer the middle of the view), so the head turns no farther than needed
		const float Inner = FMath::Abs(Exhibit->PanelZone.X) <= FMath::Abs(Exhibit->PanelZone.Z) ? Exhibit->PanelZone.X : Exhibit->PanelZone.Z;
		const float Outer = Inner == Exhibit->PanelZone.X ? Exhibit->PanelZone.Z : Exhibit->PanelZone.X;
		const float ZoneYaw = Inner + FMath::Sign(Outer - Inner) * FrontMapHalfWidth;
		OutYaw = Exhibit->GetActorTransform().TransformVectorNoScale(FRotator(0.f, ZoneYaw, 0.f).Vector()).Rotation().Yaw;

		return true;
	}

	return false;
}

bool AMuseumMRHud::IsPresenting() const
{
	for (const AMuseumExhibit* Exhibit : Exhibits)
	{
		if (IsValid(Exhibit) && Exhibit->HasPresentationStarted()) return true;
	}

	return false;
}

bool AMuseumMRHud::IsGuiding() const
{
	const AMuseumPlayerController* Visitor = Cast<AMuseumPlayerController>(Player.Get());
	const UWzPathGuideComponent* PathGuide = Visitor ? Visitor->GetPathGuide() : nullptr;

	return PathGuide && PathGuide->GetState() == EPathGuideState::Guiding;
}

bool AMuseumMRHud::IsEnglish() const
{
	return UMuseumLanguageSubsystem::GetCurrent(this) == EMuseumLanguage::English;
}

TArray<bool> AMuseumMRHud::GetVisited() const
{
	TArray<bool> Visited;
	for (const AMuseumExhibit* Exhibit : Exhibits) Visited.Add(IsValid(Exhibit) && Exhibit->Data && VisitedPlaces.Contains(Exhibit->Data->PlaceTag));

	return Visited;
}

int32 AMuseumMRHud::GetActiveNumber() const
{
	const AMuseumPlayerController* Visitor = Cast<AMuseumPlayerController>(Player.Get());
	const UWzPathGuideComponent* PathGuide = Visitor ? Visitor->GetPathGuide() : nullptr;
	if (!PathGuide) return 0;

	for (const AMuseumExhibit* Exhibit : Exhibits)
	{
		if (IsValid(Exhibit) && Exhibit->Data && Exhibit->Data->PlaceTag == PathGuide->GetActivePlaceTag()) return Exhibit->Data->Number;
	}

	return 0;
}

const AMuseumExhibit* AMuseumMRHud::FindExhibit(const int32 Number) const
{
	for (const AMuseumExhibit* Exhibit : Exhibits)
	{
		if (Number != 0 && IsValid(Exhibit) && Exhibit->Data && Exhibit->Data->Number == Number) return Exhibit;
	}

	return nullptr;
}

FText AMuseumMRHud::GetActiveName() const
{
	const int32 Number = GetActiveNumber();
	for (const AMuseumExhibit* Exhibit : Exhibits)
	{
		if (IsValid(Exhibit) && Exhibit->Data && Exhibit->Data->Number == Number) return Exhibit->Data->Name.Get(UMuseumLanguageSubsystem::GetCurrent(this));
	}

	return FText::GetEmpty();
}

void AMuseumMRHud::BuildMap()
{
	if (!Minimap) return;

	// Each tour stop: its Path Guide destination (the viewing spot), the object with its mesh for the miniature; the tour
	// starts at the player start
	TArray<FMuseumMinimapStopSpec> Stops;
	for (const AMuseumExhibit* Exhibit : Exhibits)
	{
		FMuseumMinimapStopSpec& Stop = Stops.AddDefaulted_GetRef();
		Stop.Number = Exhibit->Data->Number;
		Stop.Spot = Exhibit->GetViewPoint();
		for (TActorIterator<AWzPathGuideDestination> It(GetWorld()); It; ++It)
		{
			if (It->PlaceTag == Exhibit->Data->PlaceTag)
			{
				Stop.Spot = It->GetActorLocation();
				break;
			}
		}
		Stop.Object = Exhibit->RealObject ? Exhibit->RealObject->GetComponentLocation() : Exhibit->GetActorLocation();
		Stop.Mesh = Exhibit->ArtifactMesh ? Exhibit->ArtifactMesh->GetStaticMesh() : nullptr;
		Stop.Yaw = Exhibit->GetActorRotation().Yaw;
	}
	FVector Start = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Start = It->GetActorLocation();
		break;
	}
	Minimap->Build(Stops, Start);
}

void AMuseumMRHud::PlaceCurved(UWidgetComponent* Widget, const FVector& Eye, const float Yaw, const float Pitch, const float Distance, const float Arc)
{
	// The chord of the arc lies on the component's plane, the arc's middle Distance away: the plane stands at the apothem
	const float Apothem = Distance * FMath::Cos(FMath::DegreesToRadians(Arc * 0.5f));
	const FRotator Direction(Pitch, Yaw, 0.f);
	Widget->SetWorldLocationAndRotation(Eye + Direction.Vector() * Apothem, FRotator(-Pitch, Yaw + 180.f, 0.f));
}

void AMuseumMRHud::SetShown(UWidgetComponent* Widget, const float Alpha)
{
	if (!Widget) return;

	Widget->SetVisibility(Alpha > 0.003f);
	Widget->SetCollisionEnabled(Alpha > 0.5f ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (UUserWidget* Content = Widget->GetUserWidgetObject()) Content->SetRenderOpacity(Alpha);
}

#undef LOCTEXT_NAMESPACE
