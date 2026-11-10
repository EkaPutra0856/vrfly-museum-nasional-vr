#include "MR/MuseumMRMinimap.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "MR/MuseumMRHudWidgets.h"
#include "MR/MuseumMRPointer.h"
#include "MR/MuseumMRStyle.h"

using namespace MuseumMR;

namespace
{
	/** Half the tray's long side (cm), the room left around the hall (world cm), how much the table tilts toward the visitor. */
	constexpr float TrayHalfLength = 28.f;
	constexpr float HallMargin = 140.f;
	constexpr float Tilt = 26.f;
	constexpr float WallHeight = 2.4f;

	/** A stop: plinth size and height, the miniature's largest size (cm, larger than to scale so it reads). */
	constexpr float PlinthSize = 3.4f;
	constexpr float PlinthHeight = 1.6f;
	constexpr float MiniatureSize = 6.f;

	/** Pointed at: grows by this, pressed: pops by this more. */
	constexpr float HoverGrow = 0.3f;
	constexpr float PressPop = 0.25f;
	constexpr float PressTime = 0.45f;

	/** Placements relative to the eye: distance, height, yaw offset. */
	constexpr float FrontDistance = 72.f;
	constexpr float FrontDrop = 46.f;
	constexpr float FrontScale = 0.8f;
	// Beside the view while guiding: left of the HUD bar's end (bar: -34..34 degrees, pitch -19) and above it
	constexpr float SideDistance = 80.f;
	constexpr float SideDrop = 2.f;
	constexpr float SideYaw = -40.f;
	constexpr float SideScale = 0.5f;

	/** Dragging the table: spin per degree the hand turns, tilt per degree it rises / turns, the few degrees it may tilt at most. */
	constexpr float SpinPerDegree = 5.f;
	constexpr float PitchPerDegree = 1.2f;
	constexpr float RollPerDegree = 0.25f;
	constexpr float MaxPitch = 8.f;
	constexpr float MaxRoll = 5.f;
	/** Room around the tray where a press still grabs it (cm, model). */
	constexpr float GrabMargin = 4.f;

	/** A newly guided route draws itself on the table in this long (s). */
	constexpr float RouteDrawTime = 1.4f;

	constexpr float HeaderScale = 0.04f;
	constexpr float ConfirmScale = 0.034f;
	constexpr int32 RouteSegmentCount = 48;

	/** A flat bar in the model plane between two model points. */
	void SetBar(UStaticMeshComponent* Mesh, const FVector& From, const FVector& To, const float Width, const float Height)
	{
		const FVector Delta = To - From;
		const double Length = FVector2D(Delta.X, Delta.Y).Size();
		Mesh->SetRelativeLocation((From + To) * 0.5);
		Mesh->SetRelativeRotation(FRotator(0., FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0.));
		Mesh->SetRelativeScale3D(FVector(FMath::Max(Length, 0.01) / 100., Width / 100., Height / 100.));
	}

	const FLinearColor& PlinthWhite()
	{
		static const FLinearColor Color(0.86f, 0.9f, 0.92f);
		return Color;
	}
}

// ====== Lifecycle ====== //

AMuseumMRMinimap::AMuseumMRMinimap()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Model = CreateDefaultSubobject<USceneComponent>(TEXT("Model"));
	Model->SetupAttachment(RootComponent);

	auto MakePanel = [this](const TCHAR* Name, const TSubclassOf<UUserWidget> Class, const FVector2D& DrawSize, const float Scale, const int32 Sort)
	{
		UWidgetComponent* Widget = CreateDefaultSubobject<UWidgetComponent>(Name);
		Widget->SetupAttachment(RootComponent);
		Widget->SetWidgetSpace(EWidgetSpace::World);
		Widget->SetWidgetClass(Class);
		Widget->SetDrawSize(DrawSize);
		Widget->SetPivot(FVector2D(.5, .5));
		Widget->SetTwoSided(false);
		Widget->SetBlendMode(EWidgetBlendMode::Transparent);
		Widget->SetCastShadow(false);
		Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Widget->SetCanEverAffectNavigation(false);
		Widget->SetCollisionResponseToAllChannels(ECR_Ignore);
		Widget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Widget->SetRelativeScale3D(FVector(Scale));
		Widget->SetTranslucentSortPriority(Sort);
		Widget->ComponentTags.Add(TEXT("MuseumMRInteractive"));

		return Widget;
	};
	Header = MakePanel(TEXT("Header"), UMuseumMRMapHeaderWidget::StaticClass(), UMuseumMRMapHeaderWidget::GetDrawSize(), HeaderScale, 2);
	Confirm = MakePanel(TEXT("Confirm"), UMuseumMRMapConfirmWidget::StaticClass(), UMuseumMRMapConfirmWidget::GetDrawSize(), ConfirmScale, 4);
}

void AMuseumMRMinimap::BeginPlay()
{
	Super::BeginPlay();

	Header->InitWidget();
	if (UMuseumMRMapHeaderWidget* Widget = Cast<UMuseumMRMapHeaderWidget>(Header->GetUserWidgetObject()))
	{
		Widget->OnMinimize.BindLambda([this]() { OnMinimize.ExecuteIfBound(); });
	}
	Confirm->InitWidget();
	if (UMuseumMRMapConfirmWidget* Widget = Cast<UMuseumMRMapConfirmWidget>(Confirm->GetUserWidgetObject()))
	{
		Widget->OnYes.BindLambda([this]()
		{
			const int32 Number = ConfirmNumber;
			CloseConfirm();
			if (Number != 0) OnConfirm.ExecuteIfBound(Number);
		});
		Widget->OnNo.BindUObject(this, &AMuseumMRMinimap::CloseConfirm);
	}
	Confirm->SetVisibility(false);
}

// ====== API ====== //

void AMuseumMRMinimap::Build(const TArray<FMuseumMinimapStopSpec>& InStops, const FVector& Start)
{
	// Scale: the hall around every stop, its object and the start, its long side the tray's
	StartWorld = Start;
	FBox Bounds(ForceInit);
	Bounds += Start;
	for (const FMuseumMinimapStopSpec& Spec : InStops)
	{
		Bounds += Spec.Spot;
		Bounds += Spec.Object;
	}
	Centre = Bounds.GetCenter();
	const FVector2D Half = FVector2D(Bounds.GetExtent()) + FVector2D(HallMargin);
	MapScale = TrayHalfLength / FMath::Max(Half.X, Half.Y);
	TrayHalf = Half * MapScale;

	// Landscape: of the four quarter turns, the long side across the view and the tour's start nearest the visitor
	float BestScore = TNumericLimits<float>::Max();
	for (const float Candidate : { 0.f, 90.f, 180.f, 270.f })
	{
		const FRotator Turn(0.f, Candidate, 0.f);
		const FVector Across = Turn.RotateVector(FVector(TrayHalf.X, TrayHalf.Y, 0.));
		const bool bLandscape = FMath::Abs(Across.Y) >= FMath::Abs(Across.X) - 0.01;
		const float Score = (bLandscape ? 0.f : 10000.f) + static_cast<float>(Turn.RotateVector(ToModel(Start)).X);
		if (Score < BestScore)
		{
			BestScore = Score;
			BaseYaw = Candidate;
		}
	}

	// The dark glass tray, its glowing rim, the hall's low glass walls on it
	Tray = MakeMesh(this, Model, EShape::Cube, EMaterial::Glass, TEXT("MapTray"));
	SetColor(Tray, Cyan());
	Tray->SetRelativeLocation(FVector(0., 0., -0.6));
	Tray->SetRelativeScale3D(FVector(TrayHalf.X * 2.f / 100.f, TrayHalf.Y * 2.f / 100.f, 1.2f / 100.f));
	const FVector Corners[4] = { FVector(-TrayHalf.X, -TrayHalf.Y, 0.), FVector(TrayHalf.X, -TrayHalf.Y, 0.), FVector(TrayHalf.X, TrayHalf.Y, 0.), FVector(-TrayHalf.X, TrayHalf.Y, 0.) };
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const FVector& From = Corners[Side];
		const FVector& To = Corners[(Side + 1) % 4];
		UStaticMeshComponent* Rim = MakeMesh(this, Model, EShape::Cube, EMaterial::Glow);
		SetColor(Rim, Cyan(), 3.f);
		SetBar(Rim, From + FVector(0., 0., 0.05), To + FVector(0., 0., 0.05), 0.35f, 0.35f);
		Rims.Add(Rim);

		UStaticMeshComponent* Wall = MakeMesh(this, Model, EShape::Cube, EMaterial::Glass);
		SetColor(Wall, Cyan());
		SetBar(Wall, From * 0.98 + FVector(0., 0., WallHeight * 0.5), To * 0.98 + FVector(0., 0., WallHeight * 0.5), 0.4f, WallHeight);
		Walls.Add(Wall);
	}

	// Stops in tour order: plinth, miniature, halo, number, the box the pointer hits
	TArray<FMuseumMinimapStopSpec> Sorted = InStops;
	Sorted.Sort([](const FMuseumMinimapStopSpec& A, const FMuseumMinimapStopSpec& B) { return A.Number < B.Number; });
	for (const FMuseumMinimapStopSpec& Spec : Sorted)
	{
		FMuseumMinimapStop& Stop = Stops.AddDefaulted_GetRef();
		Stop.Number = Spec.Number;
		Stop.World = Spec.Spot;

		Stop.Pivot = NewObject<USceneComponent>(this);
		Stop.Pivot->SetupAttachment(Model);
		Stop.Pivot->SetRelativeLocation(ToModel(Spec.Object));
		Stop.Pivot->RegisterComponent();
		AddInstanceComponent(Stop.Pivot);

		Stop.Glow = MakeMesh(this, Stop.Pivot, EShape::Cylinder, EMaterial::Glow);
		SetColor(Stop.Glow, Cyan(), 6.f);
		Stop.Glow->SetRelativeLocation(FVector(0., 0., 0.1));
		Stop.Glow->SetRelativeScale3D(FVector(0.075, 0.075, 0.002));
		Stop.Halo = MakeMesh(this, Stop.Pivot, EShape::Plane, EMaterial::Ring);
		Stop.Halo->SetRelativeLocation(FVector(0., 0., 0.25));
		Stop.Halo->SetRelativeScale3D(FVector(0.065));
		Stop.Plinth = MakeMesh(this, Stop.Pivot, EShape::Cube, EMaterial::Glow);
		SetColor(Stop.Plinth, PlinthWhite(), 0.6f);
		Stop.Plinth->SetRelativeLocation(FVector(0., 0., PlinthHeight * 0.5));
		SetBoxSize(Stop.Plinth, FVector(PlinthSize, PlinthSize, PlinthHeight));

		// The exhibit's own mesh in miniature, standing on the plinth, turned as in the hall
		float Top = PlinthHeight + 2.f;
		if (Spec.Mesh)
		{
			Stop.Miniature = NewObject<UStaticMeshComponent>(this);
			Stop.Miniature->SetStaticMesh(Spec.Mesh);
			Stop.Miniature->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Stop.Miniature->SetCanEverAffectNavigation(false);
			Stop.Miniature->SetCastShadow(false);
			Stop.Miniature->bVisibleInRayTracing = false;
			Stop.Miniature->SetMobility(EComponentMobility::Movable);
			Stop.Miniature->SetupAttachment(Stop.Pivot);
			const FBoxSphereBounds Local = Spec.Mesh->GetBounds();
			const FVector Size = Local.BoxExtent * 2.;
			const float Scale = MiniatureSize / FMath::Max(1., FMath::Max3(Size.X, Size.Y, Size.Z));
			const FRotator Turn(0.f, Spec.Yaw, 0.f);
			const FVector Offset = Turn.RotateVector(FVector(-Local.Origin.X, -Local.Origin.Y, 0.) * Scale);
			Stop.Miniature->SetRelativeLocationAndRotation(FVector(Offset.X, Offset.Y, PlinthHeight - (Local.Origin.Z - Local.BoxExtent.Z) * Scale), Turn);
			Stop.Miniature->SetRelativeScale3D(FVector(Scale));
			Stop.Miniature->RegisterComponent();
			AddInstanceComponent(Stop.Miniature);
			Top = PlinthHeight + Size.Z * Scale;
		}

		Stop.Label = NewObject<UTextRenderComponent>(this);
		Stop.Label->SetupAttachment(Stop.Pivot);
		Stop.Label->SetText(FText::AsNumber(Spec.Number));
		Stop.Label->SetWorldSize(4.2f);
		// Unlit so the number keeps its colour in the dark hall (the default text material is lit and reads black)
		if (UMaterialInterface* TextMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/VREditor/Fonts/VRTextMaterial.VRTextMaterial")))
		{
			Stop.Label->SetTextMaterial(TextMaterial);
		}
		Stop.Label->SetHorizontalAlignment(EHTA_Center);
		Stop.Label->SetVerticalAlignment(EVRTA_TextCenter);
		Stop.Label->SetCastShadow(false);
		Stop.Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stop.Label->SetCanEverAffectNavigation(false);
		Stop.Label->SetRelativeLocation(FVector(0., 0., Top + 2.6f));
		Stop.Label->RegisterComponent();
		AddInstanceComponent(Stop.Label);

		Stop.CheckShort = MakeMesh(this, Model, EShape::Cube, EMaterial::Glow);
		Stop.CheckLong = MakeMesh(this, Model, EShape::Cube, EMaterial::Glow);
		SetColor(Stop.CheckShort, Green(), 6.f);
		SetColor(Stop.CheckLong, Green(), 6.f);

		Stop.Hit = NewObject<UBoxComponent>(this);
		Stop.Hit->SetupAttachment(Stop.Pivot);
		Stop.Hit->SetBoxExtent(FVector(PlinthSize * 0.75f, PlinthSize * 0.75f, (Top + 4.f) * 0.5f));
		Stop.Hit->SetRelativeLocation(FVector(0., 0., (Top + 4.f) * 0.5f));
		Stop.Hit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stop.Hit->SetCollisionResponseToAllChannels(ECR_Ignore);
		Stop.Hit->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Stop.Hit->SetCanEverAffectNavigation(false);
		Stop.Hit->SetGenerateOverlapEvents(false);
		Stop.Hit->SetHiddenInGame(true);
		Stop.Hit->ComponentTags.Add(TEXT("MuseumMRInteractive"));
		Stop.Hit->RegisterComponent();
		AddInstanceComponent(Stop.Hit);
	}

	StartMark = MakeMesh(this, Model, EShape::Sphere, EMaterial::Glow, TEXT("MapStart"));
	SetColor(StartMark, Text(), 2.f);
	StartMark->SetRelativeLocation(ToModel(Start) + FVector(0., 0., 1.));
	StartMark->SetRelativeScale3D(FVector(0.014));

	for (int32 Index = 0; Index < RouteSegmentCount; ++Index)
	{
		UStaticMeshComponent* Segment = MakeMesh(this, Model, EShape::Cube, EMaterial::Glow);
		SetColor(Segment, Cyan(), 10.f);
		SetFade(Segment, 0.f);
		RouteSegments.Add(Segment);
	}

	VisitorDot = MakeMesh(this, Model, EShape::Sphere, EMaterial::Glow, TEXT("MapVisitor"));
	SetColor(VisitorDot, Cyan(), 12.f);
	VisitorDot->SetRelativeScale3D(FVector(0.02));
	VisitorPulse = MakeMesh(this, Model, EShape::Plane, EMaterial::Ring, TEXT("MapVisitorPulse"));
	SetColor(VisitorPulse, Cyan(), 8.f);
	VisitorArrow = MakeMesh(this, Model, EShape::Cone, EMaterial::Glow, TEXT("MapVisitorArrow"));
	SetColor(VisitorArrow, Cyan(), 10.f);
	VisitorArrow->SetRelativeScale3D(FVector(0.016, 0.016, 0.024));

	VisitorLabel = NewObject<UTextRenderComponent>(this);
	VisitorLabel->SetupAttachment(Model);
	VisitorLabel->SetWorldSize(2.6f);
	if (UMaterialInterface* TextMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/VREditor/Fonts/VRTextMaterial.VRTextMaterial")))
	{
		VisitorLabel->SetTextMaterial(TextMaterial);
	}
	VisitorLabel->SetHorizontalAlignment(EHTA_Center);
	VisitorLabel->SetVerticalAlignment(EVRTA_TextBottom);
	VisitorLabel->SetTextRenderColor(Cyan().ToFColorSRGB());
	VisitorLabel->SetCastShadow(false);
	VisitorLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisitorLabel->SetCanEverAffectNavigation(false);
	VisitorLabel->RegisterComponent();
	AddInstanceComponent(VisitorLabel);

	RouteHead = MakeMesh(this, Model, EShape::Sphere, EMaterial::Glow, TEXT("MapRouteHead"));
	SetColor(RouteHead, Cyan(), 30.f);
	SetFade(RouteHead, 0.f);

	SetAlpha(0.f);
}

void AMuseumMRMinimap::UpdateMap(const float DeltaSeconds, const EMuseumMinimapMode Mode, const FVector& Eye, const float ViewYaw, const FVector& Visitor, const float VisitorYaw,
	const TArray<bool>& Visited, const TArray<FVector>& Route, const int32 ActiveNumber, const bool bEnglish, const APlayerController* Player)
{
	if (!Tray) return;
	Time += DeltaSeconds;

	// Fade in / out; a folded map keeps its last place
	if (Mode == EMuseumMinimapMode::Front && Shown <= 0.f)
	{
		// Opens landscape, as it was built
		SpinYaw = ShownSpin = 0.f;
		TiltPitch = ShownPitch = 0.f;
		TiltRoll = ShownRoll = 0.f;
	}
	if (Mode != EMuseumMinimapMode::Hidden) CurrentMode = Mode;
	Shown = FMath::FInterpConstantTo(Shown, Mode == EMuseumMinimapMode::Hidden ? 0.f : 1.f, DeltaSeconds, 3.f);
	const float Alpha = Ease(Shown);
	if (Mode != EMuseumMinimapMode::Front) CloseConfirm();

	// Placement: in front and low, or on the left while guiding; tilted toward the visitor
	const bool bFront = CurrentMode == EMuseumMinimapMode::Front;
	const float Yaw = ViewYaw + (bFront ? 0.f : SideYaw);
	const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Wanted = Eye + Forward * (bFront ? FrontDistance : SideDistance) - FVector(0., 0., (bFront ? FrontDrop : SideDrop) + 8.f * (1.f - Alpha));
	const FRotator WantedRotation(Tilt, Yaw, 0.f);
	if (!bPlaced)
	{
		Placement = FTransform(WantedRotation, Wanted);
		bPlaced = true;
	}
	const float Blend = FMath::Clamp(DeltaSeconds * 6.f, 0.f, 1.f);
	Placement.SetLocation(FMath::Lerp(Placement.GetLocation(), Wanted, Blend));
	Placement.SetRotation(FQuat::Slerp(Placement.GetRotation(), WantedRotation.Quaternion(), Blend));
	Placement.SetScale3D(FVector(FMath::Lerp(0.7f, bFront ? FrontScale : SideScale, Alpha)));
	SetActorTransform(Placement);

	// Grabbed: the hand turning spins the table, rising tilts it up a little, turning also leans it a little sideways
	if (bDragging && (Mode != EMuseumMinimapMode::Front || Alpha < 0.5f)) EndDrag();
	FVector RayStart, RayDirection;
	if (bDragging && UMuseumMRPointer::GetRay(DragPlayer.Get(), RayStart, RayDirection))
	{
		const FRotator Ray = RayDirection.Rotation();
		const float TurnedBy = FMath::FindDeltaAngleDegrees(DragStartRay.Yaw, Ray.Yaw);
		const float RoseBy = FMath::FindDeltaAngleDegrees(DragStartRay.Pitch, Ray.Pitch);
		SpinYaw = DragStartSpin - TurnedBy * SpinPerDegree;
		TiltPitch = FMath::Clamp(DragStartPitch + RoseBy * PitchPerDegree, -MaxPitch, MaxPitch);
		TiltRoll = FMath::Clamp(TurnedBy * RollPerDegree, -MaxRoll, MaxRoll);
	}
	else
	{
		TiltRoll = 0.f;
	}
	const float Follow = FMath::Clamp(DeltaSeconds * 10.f, 0.f, 1.f);
	ShownSpin += (SpinYaw - ShownSpin) * Follow;
	ShownPitch += (TiltPitch - ShownPitch) * Follow;
	ShownRoll += (TiltRoll - ShownRoll) * Follow;

	// The model: landscape on the table, then the visitor's spin, then the small tilts
	const FQuat ModelTurn = FRotator(ShownPitch, 0.f, ShownRoll).Quaternion() * FRotator(0.f, BaseYaw + ShownSpin, 0.f).Quaternion();
	Model->SetRelativeRotation(ModelTurn);

	SetAlpha(Alpha);
	UpdateHover(DeltaSeconds, Mode == EMuseumMinimapMode::Front && Alpha > 0.5f && !IsConfirmOpen() && !bDragging, Player);

	// A newly guided route draws itself again (also each time the map opens)
	if (ActiveNumber != RouteNumber || Alpha <= 0.003f)
	{
		RouteNumber = ActiveNumber;
		RouteReveal = 0.f;
	}
	if (Alpha <= 0.003f) return;

	const FVector Up = GetActorUpVector();

	// Stops: number (each once), visited check, the guided one pulsing; pointed at or chosen: larger and brighter
	int32 Count = 0;
	for (int32 Index = 0; Index < Stops.Num(); ++Index)
	{
		FMuseumMinimapStop& Stop = Stops[Index];
		const bool bVisited = Visited.IsValidIndex(Index) && Visited[Index];
		const bool bActive = Stop.Number == ActiveNumber;
		Count += bVisited ? 1 : 0;

		const float Lit = Ease(Stop.Hover);
		const float Pop = PressPop * FMath::Sin(PI * FMath::Clamp(1.f - Stop.Press / PressTime, 0.f, 1.f)) * (Stop.Press > 0.f ? 1.f : 0.f);
		Stop.Press = FMath::Max(Stop.Press - DeltaSeconds, 0.f);
		Stop.Pivot->SetRelativeScale3D(FVector((1.f + HoverGrow * Lit + Pop) * FMath::Lerp(0.6f, 1.f, Alpha)));

		const FLinearColor Color = bVisited ? Green() : (bActive || Lit > 0.5f) ? Cyan() : Text();
		const float Pulse = bActive ? 1.f + 0.35f * (0.5f + 0.5f * FMath::Sin(Time * 5.f)) : 1.f;
		SetColor(Stop.Halo, bVisited ? Green() : Cyan(), (bActive ? 10.f : 4.f) + 6.f * Lit);
		Stop.Halo->SetRelativeScale3D(FVector(0.065f * Pulse * (1.f + 0.2f * Lit)));
		SetColor(Stop.Plinth, FMath::Lerp(PlinthWhite(), Cyan(), 0.5f * Lit), 0.6f + 1.4f * Lit);
		SetColor(Stop.Glow, Cyan(), 2.f + 3.f * Lit);
		SetFade(Stop.Glow, Alpha * 0.55f * FMath::Max(Lit, Pop * 3.f));
		Stop.Label->SetTextRenderColor(Color.ToFColorSRGB());

		// Numbers turned to the eye
		const FVector LabelLocation = Stop.Label->GetComponentLocation();
		const FRotator Facing = (Eye - LabelLocation).Rotation();
		Stop.Label->SetWorldRotation(FRotator(0.f, Facing.Yaw, 0.f));

		// A check right of the number
		const FVector Right = -FRotator(0.f, Facing.Yaw, 0.f).Quaternion().GetRightVector();
		const float Size = 1.3f * Stop.Pivot->GetComponentScale().X;
		const FVector Base = LabelLocation + Right * 2.9f * Size;
		const FVector A = Base + (Right * -0.8f + Up * 0.1f) * Size;
		const FVector B = Base + (Right * -0.25f + Up * -0.55f) * Size;
		const FVector C = Base + (Right * 0.9f + Up * 0.9f) * Size;
		SetLine(Stop.CheckShort, A, B, 0.5f * Size);
		SetLine(Stop.CheckLong, B, C, 0.5f * Size);
		SetFade(Stop.CheckShort, bVisited ? Alpha : 0.f);
		SetFade(Stop.CheckLong, bVisited ? Alpha : 0.f);
	}

	// The guided route on the tray: draws itself from the visitor to the stop (a bright tip leading), then a glint runs along it
	const int32 Points = Route.Num();
	const int32 Step = Points > RouteSegmentCount + 1 ? FMath::CeilToInt(float(Points - 1) / RouteSegmentCount) : 1;
	TArray<FVector, TInlineAllocator<64>> Path;
	for (int32 Index = 0; Index < Points; Index += Step) Path.Add(ToModel(Route[Index]) + FVector(0., 0., 0.4));
	if (Points > 1 && (Points - 1) % Step != 0) Path.Add(ToModel(Route.Last()) + FVector(0., 0., 0.4));
	float Length = 0.f;
	for (int32 Index = 0; Index + 1 < Path.Num(); ++Index) Length += static_cast<float>(FVector::Dist(Path[Index], Path[Index + 1]));
	if (Path.Num() > 1 && Alpha > 0.5f && RouteReveal < 1.f)
	{
		RouteReveal = FMath::Min(RouteReveal + DeltaSeconds / RouteDrawTime, 1.f);
		if (RouteReveal >= 1.f)
		{
			// Arrived: the stop pops
			for (FMuseumMinimapStop& Stop : Stops)
			{
				if (Stop.Number == ActiveNumber) Stop.Press = PressTime;
			}
		}
	}
	const float Drawn = Length * (1.f - FMath::Square(1.f - RouteReveal));
	const float Glint = RouteReveal < 1.f ? Drawn : FMath::Fmod(Time * 0.45f, 1.3f) * Length;
	const float GlintWidth = FMath::Max(Length * 0.1f, 1.f);
	int32 Used = 0;
	float Along = 0.f;
	FVector Tip = Path.Num() > 0 ? Path[0] : FVector::ZeroVector;
	for (int32 Index = 0; Index + 1 < Path.Num() && Used < RouteSegmentCount; ++Index)
	{
		if (Along >= Drawn) break;

		const float Piece = static_cast<float>(FVector::Dist(Path[Index], Path[Index + 1]));
		const float Part = Piece > 0.f ? FMath::Clamp((Drawn - Along) / Piece, 0.f, 1.f) : 1.f;
		const FVector To = FMath::Lerp(Path[Index], Path[Index + 1], Part);
		UStaticMeshComponent* Segment = RouteSegments[Used++];
		SetBar(Segment, Path[Index], To, 0.9f, 0.5f);
		const float Middle = Along + Piece * Part * 0.5f;
		SetColor(Segment, Cyan(), 8.f + 22.f * FMath::Exp(-FMath::Square((Middle - Glint) / GlintWidth)));
		SetFade(Segment, Alpha);
		Tip = To;
		Along += Piece;
	}
	for (int32 Index = Used; Index < RouteSegments.Num(); ++Index) SetFade(RouteSegments[Index], 0.f);
	RouteHead->SetRelativeLocation(Tip + FVector(0., 0., 0.4));
	RouteHead->SetRelativeScale3D(FVector(0.024f * (1.f + 0.25f * FMath::Sin(Time * 18.f))));
	SetFade(RouteHead, Path.Num() > 1 && RouteReveal < 1.f ? Alpha : 0.f);

	// The visitor: dot, pulse, an arrow the way they face, "Anda di sini" above, turned to the eye
	const FVector Dot = ToModel(Visitor) + FVector(0., 0., 1.2);
	VisitorDot->SetRelativeLocation(Dot);
	const FVector Facing = FRotator(0.f, VisitorYaw, 0.f).Vector();
	VisitorArrow->SetRelativeLocationAndRotation(Dot + Facing * 2.6, FRotator(-90.f, VisitorYaw, 0.f));
	SetFade(VisitorArrow, Alpha);
	if (VisitorLabelEnglish != int8(bEnglish))
	{
		VisitorLabelEnglish = int8(bEnglish);
		VisitorLabel->SetText(bEnglish ? NSLOCTEXT("Museum", "MapYouAreHere", "You are here") : NSLOCTEXT("Museum", "MapAndaDiSini", "Anda di sini"));
	}
	VisitorLabel->SetRelativeLocation(Dot + FVector(0., 0., 3.));
	VisitorLabel->SetWorldRotation(FRotator(0.f, (Eye - VisitorLabel->GetComponentLocation()).Rotation().Yaw, 0.f));
	VisitorLabel->SetVisibility(Alpha > 0.35f);
	const float Wave = FMath::Fmod(Time * 0.8f, 1.f);
	VisitorPulse->SetRelativeLocation(Dot - FVector(0., 0., 0.9));
	VisitorPulse->SetRelativeScale3D(FVector(0.035f + 0.07f * Wave));
	SetFade(VisitorPulse, Alpha * (1.f - Wave));

	// Header beyond the tray's far edge (as the table turns with the museum), upright, turned to the visitor
	float Far = 0.f;
	for (const FVector2D Corner : { FVector2D(-1., -1.), FVector2D(1., -1.), FVector2D(1., 1.), FVector2D(-1., 1.) })
	{
		Far = FMath::Max(Far, static_cast<float>(ModelTurn.RotateVector(FVector(Corner.X * TrayHalf.X, Corner.Y * TrayHalf.Y, 0.)).X));
	}
	Header->SetRelativeLocation(FVector(Far + 4.f, 0., 8.f));
	Header->SetRelativeRotation(FRotator(Tilt, 180.f, 0.f));
	if (UMuseumMRMapHeaderWidget* Widget = Cast<UMuseumMRMapHeaderWidget>(Header->GetUserWidgetObject()))
	{
		Widget->SetCount(Count, Stops.Num(), bEnglish);
		Widget->SetRenderOpacity(Alpha);
	}

	// The pop-up stands above the header, beyond the tray, growing out of it
	ConfirmShown = FMath::FInterpConstantTo(ConfirmShown, IsConfirmOpen() ? 1.f : 0.f, DeltaSeconds, 5.f);
	const float ConfirmAlpha = Ease(ConfirmShown) * Alpha;
	Confirm->SetVisibility(ConfirmAlpha > 0.003f);
	Confirm->SetCollisionEnabled(ConfirmAlpha > 0.5f ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Confirm->SetRelativeLocation(FVector(Far + 4.f, 0., 20.f + 6.f * ConfirmAlpha));
	Confirm->SetRelativeRotation(FRotator(Tilt, 180.f, 0.f));
	Confirm->SetRelativeScale3D(FVector(ConfirmScale * FMath::Lerp(0.85f, 1.f, ConfirmAlpha)));
	if (UUserWidget* Widget = Confirm->GetUserWidgetObject()) Widget->SetRenderOpacity(ConfirmAlpha);
}

bool AMuseumMRMinimap::GetStopLocation(const int32 Number, FVector& OutLocation) const
{
	if (Number == 0)
	{
		// The tray between its centre and its near edge (a place to grab it)
		if (!Tray) return false;
		OutLocation = GetActorTransform().TransformPosition(FVector(-0.5f * FMath::Min(TrayHalf.X, TrayHalf.Y), 0., 0.5));

		return true;
	}
	for (const FMuseumMinimapStop& Stop : Stops)
	{
		if (Stop.Number != Number || !Stop.Hit) continue;

		OutLocation = Stop.Hit->GetComponentLocation();

		return true;
	}

	return false;
}

void AMuseumMRMinimap::PressStop(const int32 Number, const FText& Name, UTexture2D* Thumbnail, const bool bEnglish)
{
	for (FMuseumMinimapStop& Stop : Stops)
	{
		if (Stop.Number != Number) continue;

		Stop.Press = PressTime;
		ConfirmNumber = Number;
		if (UMuseumMRMapConfirmWidget* Widget = Cast<UMuseumMRMapConfirmWidget>(Confirm->GetUserWidgetObject())) Widget->SetPlace(Number, Name, Thumbnail, bEnglish);

		return;
	}
}

void AMuseumMRMinimap::CloseConfirm()
{
	ConfirmNumber = 0;
}

bool AMuseumMRMinimap::BeginDrag(const APlayerController* Player)
{
	if (!Tray || CurrentMode != EMuseumMinimapMode::Front || Shown < 0.9f || IsConfirmOpen() || HoveredNumber != 0) return false;

	FVector Start, Direction;
	if (!UMuseumMRPointer::GetRay(Player, Start, Direction)) return false;

	// Only on the tray: where the ray crosses its plane, inside its edge
	const FTransform& Frame = Model->GetComponentTransform();
	const FVector LocalStart = Frame.InverseTransformPosition(Start);
	const FVector LocalDirection = Frame.InverseTransformVectorNoScale(Direction);
	if (FMath::Abs(LocalDirection.Z) < UE_KINDA_SMALL_NUMBER) return false;
	const double Distance = -LocalStart.Z / LocalDirection.Z;
	if (Distance <= 0.) return false;
	const FVector Point = LocalStart + LocalDirection * Distance;
	if (FMath::Abs(Point.X) > TrayHalf.X + GrabMargin || FMath::Abs(Point.Y) > TrayHalf.Y + GrabMargin) return false;

	bDragging = true;
	DragPlayer = Player;
	DragStartRay = Direction.Rotation();
	DragStartSpin = SpinYaw;
	DragStartPitch = TiltPitch;

	return true;
}

void AMuseumMRMinimap::EndDrag()
{
	bDragging = false;
	DragPlayer.Reset();
}

// ====== Helpers ====== //

FVector AMuseumMRMinimap::ToModel(const FVector& World) const
{
	return FVector((World.X - Centre.X) * MapScale, (World.Y - Centre.Y) * MapScale, 0.);
}

void AMuseumMRMinimap::UpdateHover(const float DeltaSeconds, const bool bCanPoint, const APlayerController* Player)
{
	// The nearest stop the pointer crosses (in front only, not while the pop-up asks)
	HoveredNumber = 0;
	FVector Start, Direction;
	if (bCanPoint && UMuseumMRPointer::GetRay(Player, Start, Direction))
	{
		const FVector End = Start + Direction * 1000.f;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(MuseumMRMinimapStop), false);
		float Nearest = TNumericLimits<float>::Max();
		for (const FMuseumMinimapStop& Stop : Stops)
		{
			FHitResult Hit;
			if (!Stop.Hit || !Stop.Hit->LineTraceComponent(Hit, Start, End, Params) || Hit.Distance >= Nearest) continue;

			Nearest = Hit.Distance;
			HoveredNumber = Stop.Number;
		}
	}

	// Pointed at or chosen in the pop-up: lit; the laser may end on the stops while they can be pressed
	for (FMuseumMinimapStop& Stop : Stops)
	{
		const bool bLit = Stop.Number == HoveredNumber || Stop.Number == ConfirmNumber;
		Stop.Hover = FMath::FInterpConstantTo(Stop.Hover, bLit ? 1.f : 0.f, DeltaSeconds, 7.f);
		if (Stop.Hit) Stop.Hit->SetCollisionEnabled(bCanPoint ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AMuseumMRMinimap::SetAlpha(const float Alpha)
{
	SetFade(Tray, Alpha);
	for (UStaticMeshComponent* Rim : Rims) SetFade(Rim, Alpha);
	for (UStaticMeshComponent* Wall : Walls) SetFade(Wall, Alpha * 0.8f);
	SetFade(StartMark, Alpha);
	SetFade(VisitorDot, Alpha);
	if (Alpha <= 0.003f)
	{
		SetFade(VisitorArrow, 0.f);
		SetFade(RouteHead, 0.f);
		VisitorLabel->SetVisibility(false);
	}
	for (FMuseumMinimapStop& Stop : Stops)
	{
		SetFade(Stop.Plinth, Alpha);
		SetFade(Stop.Halo, Alpha);
		if (Stop.Miniature) Stop.Miniature->SetVisibility(Alpha > 0.2f);
		Stop.Label->SetVisibility(Alpha > 0.35f);
	}
	if (Alpha <= 0.003f)
	{
		for (UStaticMeshComponent* Segment : RouteSegments) SetFade(Segment, 0.f);
		SetFade(VisitorPulse, 0.f);
		for (FMuseumMinimapStop& Stop : Stops)
		{
			SetFade(Stop.Glow, 0.f);
			SetFade(Stop.CheckShort, 0.f);
			SetFade(Stop.CheckLong, 0.f);
		}
	}
	Header->SetVisibility(Alpha > 0.003f);
	Header->SetCollisionEnabled(Alpha > 0.5f ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (Alpha <= 0.003f) Confirm->SetVisibility(false);
}
