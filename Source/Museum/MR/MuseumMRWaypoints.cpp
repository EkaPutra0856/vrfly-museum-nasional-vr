#include "MR/MuseumMRWaypoints.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "MR/MuseumMRLabelWidget.h"
#include "MR/MuseumMRStyle.h"

using namespace MuseumMR;

namespace
{
	/** The dotted path: dots this far apart, every ArrowEvery-th an arrow; the first one ahead of the feet, gone far away. */
	constexpr int32 DotCount = 64;
	constexpr float DotSpacing = 50.f;
	constexpr float DotSize = 7.f;
	constexpr int32 ArrowEvery = 3;
	constexpr float ArrowLength = 12.f;
	constexpr float ArrowWidth = 9.f;
	constexpr float ArrowThickness = 2.6f;
	constexpr float FirstDot = 80.f;
	constexpr float FarFadeStart = 1500.f;
	constexpr float FarFadeEnd = 2400.f;
	/** A wave of light runs along the dots toward the goal: speed, length between two waves. */
	constexpr float WaveSpeed = 220.f;
	constexpr float WaveLength = 320.f;

	/** The pin floats at head height over the goal; head size; label beside it. */
	constexpr float PinHeight = 190.f;
	constexpr float PinSize = 12.f;
	constexpr float LabelGap = 6.f;
	/** The pin keeps at least the size it has this far away. */
	constexpr float PinNear = 500.f;
	/** Wide enough for the longest name with its distance ("Maket Miniatur Arsitektur Masjid At-Thohir · 18 m"); the plate hugs the text. */
	const FVector2D LabelSize(2200., 220.);

	/** The label keeps about this angular size (cm of label per cm of distance). */
	constexpr float LabelPerDistance = 0.00045f;
}

// ====== Lifecycle ====== //

AMuseumMRWaypoints::AMuseumMRWaypoints()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Marker = CreateDefaultSubobject<USceneComponent>(TEXT("Marker"));
	Marker->SetupAttachment(RootComponent);

	Label = CreateDefaultSubobject<UWidgetComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetWidgetSpace(EWidgetSpace::World);
	Label->SetWidgetClass(UMuseumMRLabelWidget::StaticClass());
	Label->SetDrawSize(LabelSize);
	Label->SetPivot(FVector2D(.5, 1.));
	Label->SetTwoSided(false);
	Label->SetBlendMode(EWidgetBlendMode::Transparent);
	Label->SetCastShadow(false);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetCanEverAffectNavigation(false);
	Label->SetTranslucentSortPriority(2);
}

void AMuseumMRWaypoints::BeginPlay()
{
	Super::BeginPlay();

	Label->InitWidget();
	if (UMuseumMRLabelWidget* Widget = Cast<UMuseumMRLabelWidget>(Label->GetUserWidgetObject())) Widget->SetScale(34, 52);

	for (int32 Index = 0; Index < DotCount; ++Index)
	{
		UStaticMeshComponent* Dot = MakeMesh(this, RootComponent, EShape::Sphere, EMaterial::Glow);
		SetColor(Dot, Cyan(), 6.f);
		Dot->SetWorldScale3D(FVector(DotSize / 100.f, DotSize / 100.f, 1.2f / 100.f));
		SetFade(Dot, 0.f);
		Dots.Add(Dot);
	}
	for (int32 Index = 0; Index < DotCount / ArrowEvery + 1; ++Index)
	{
		FMuseumWaypointChevron& Arrow = Arrows.AddDefaulted_GetRef();
		Arrow.Left = MakeMesh(this, RootComponent, EShape::Cube, EMaterial::Glow);
		Arrow.Right = MakeMesh(this, RootComponent, EShape::Cube, EMaterial::Glow);
		SetColor(Arrow.Left, Cyan(), 7.f);
		SetColor(Arrow.Right, Cyan(), 7.f);
		SetFade(Arrow.Left, 0.f);
		SetFade(Arrow.Right, 0.f);
	}

	// The destination: a map pin (round head, light eye, cone pointing down), a slim beam to the floor, a ring around the spot
	PinHead = MakeMesh(this, Marker, EShape::Sphere, EMaterial::Glow, TEXT("PinHead"));
	SetColor(PinHead, Cyan(), 10.f);
	PinHead->SetRelativeScale3D(FVector(PinSize / 100.f));
	PinEye = MakeMesh(this, Marker, EShape::Sphere, EMaterial::Glow, TEXT("PinEye"));
	SetColor(PinEye, Text(), 4.f);
	PinEye->SetRelativeScale3D(FVector(PinSize * 0.38f / 100.f));
	PinTip = MakeMesh(this, Marker, EShape::Cone, EMaterial::Glow, TEXT("PinTip"));
	SetColor(PinTip, Cyan(), 10.f);
	PinTip->SetRelativeLocationAndRotation(FVector(0., 0., -PinSize * 0.75f), FRotator(180.f, 0.f, 0.f));
	PinTip->SetRelativeScale3D(FVector(PinSize * 0.72f / 100.f, PinSize * 0.72f / 100.f, PinSize * 0.95f / 100.f));
	Beam = MakeMesh(this, RootComponent, EShape::Cube, EMaterial::Glow, TEXT("Beam"));
	SetColor(Beam, Cyan(), 3.f);
	GroundRing = MakeMesh(this, RootComponent, EShape::Plane, EMaterial::Ring, TEXT("GroundRing"));
	SetColor(GroundRing, Cyan(), 6.f);
	GroundRing->SetWorldScale3D(FVector(1.2));
	GroundPulse = MakeMesh(this, RootComponent, EShape::Plane, EMaterial::Ring, TEXT("GroundPulse"));
	SetColor(GroundPulse, Cyan(), 6.f);
	for (UStaticMeshComponent* Mesh : { PinHead.Get(), PinEye.Get(), PinTip.Get(), Beam.Get(), GroundRing.Get(), GroundPulse.Get() }) SetFade(Mesh, 0.f);
	Label->SetVisibility(false);
}

// ====== API ====== //

void AMuseumMRWaypoints::UpdateWaypoints(const float DeltaSeconds, const TArray<FVector>& Route, const FVector& Eye, const FText& Name, const float Remaining)
{
	if (Dots.IsEmpty()) return;
	Time += DeltaSeconds;

	const bool bActive = Route.Num() >= 2;
	Shown = FMath::FInterpConstantTo(Shown, bActive ? 1.f : 0.f, DeltaSeconds, 2.5f);
	const float Alpha = Ease(Shown);

	// Dots along the route from the visitor (every third an arrow), a wave of light running to the goal, fading far away
	int32 UsedDots = 0;
	int32 UsedArrows = 0;
	if (bActive && Alpha > 0.003f)
	{
		const float Floor = FloorAt(Route[0]) + 1.2f;
		float Total = 0.f;
		for (int32 Index = 1; Index < Route.Num(); ++Index) Total += FVector::Dist2D(Route[Index - 1], Route[Index]);

		int32 Segment = 1;
		float SegmentStart = 0.f;
		int32 Count = 0;
		for (float Along = FirstDot; Along < Total - 90.f && UsedDots < Dots.Num(); Along += DotSpacing, ++Count)
		{
			// The segment holding this distance
			while (Segment < Route.Num() && SegmentStart + FVector::Dist2D(Route[Segment - 1], Route[Segment]) < Along)
			{
				SegmentStart += FVector::Dist2D(Route[Segment - 1], Route[Segment]);
				++Segment;
			}
			if (Segment >= Route.Num()) break;

			const FVector From(Route[Segment - 1].X, Route[Segment - 1].Y, Floor);
			const FVector To(Route[Segment].X, Route[Segment].Y, Floor);
			const FVector Direction = (To - From).GetSafeNormal2D();
			const FVector Point = From + Direction * FMath::Clamp(Along - SegmentStart, 0.f, FVector::Dist2D(From, To));

			const float Near = FMath::Clamp((Along - FirstDot + DotSpacing) / (2.f * DotSpacing), 0.f, 1.f);
			const float Far = 1.f - FMath::Clamp((Along - FarFadeStart) / (FarFadeEnd - FarFadeStart), 0.f, 1.f);
			const float Phase = FMath::Fmod(Along - Time * WaveSpeed, WaveLength) / WaveLength;
			const float Wave = FMath::Pow(0.5f + 0.5f * FMath::Cos(2.f * PI * Phase), 4.f);
			const float Fade = Alpha * Near * Far * (0.45f + 0.55f * Wave);

			if (Count % ArrowEvery == ArrowEvery - 1 && UsedArrows < Arrows.Num())
			{
				const FVector Right(-Direction.Y, Direction.X, 0.);
				const FVector Tip = Point + Direction * (ArrowLength * 0.5f);
				const FVector Back = Point - Direction * (ArrowLength * 0.5f);
				FMuseumWaypointChevron& Arrow = Arrows[UsedArrows++];
				SetLine(Arrow.Left, Back - Right * ArrowWidth, Tip, ArrowThickness);
				SetLine(Arrow.Right, Back + Right * ArrowWidth, Tip, ArrowThickness);
				Arrow.Left->SetWorldScale3D(Arrow.Left->GetComponentScale() * FVector(1., 1., 0.35));
				Arrow.Right->SetWorldScale3D(Arrow.Right->GetComponentScale() * FVector(1., 1., 0.35));
				SetFade(Arrow.Left, Fade);
				SetFade(Arrow.Right, Fade);
				continue;
			}

			UStaticMeshComponent* Dot = Dots[UsedDots++];
			Dot->SetWorldLocation(Point);
			SetFade(Dot, Fade);
		}
	}
	for (int32 Index = UsedDots; Index < Dots.Num(); ++Index) SetFade(Dots[Index], 0.f);
	for (int32 Index = UsedArrows; Index < Arrows.Num(); ++Index)
	{
		SetFade(Arrows[Index].Left, 0.f);
		SetFade(Arrows[Index].Right, 0.f);
	}

	// The destination
	SetFade(PinHead, Alpha);
	SetFade(PinEye, Alpha);
	SetFade(PinTip, Alpha);
	SetFade(Beam, Alpha * 0.45f);
	SetFade(GroundRing, Alpha);
	Label->SetVisibility(Alpha > 0.003f);
	if (!bActive || Alpha <= 0.003f)
	{
		SetFade(GroundPulse, 0.f);
		return;
	}

	const FVector Goal = Route.Last();
	const float Floor = FloorAt(Goal);
	const float Bob = 4.f * FMath::Sin(Time * 2.f);
	const FVector Pin(Goal.X, Goal.Y, Floor + PinHeight + Bob);
	const FVector Look = (Pin - Eye).GetSafeNormal2D();
	Marker->SetWorldLocation(Pin);
	Marker->SetWorldRotation(FRotator(0.f, Look.Rotation().Yaw, 0.f));
	PinEye->SetRelativeLocation(FVector(-PinSize * 0.42f, 0., 0.));
	SetLine(Beam, FVector(Goal.X, Goal.Y, Floor + 2.f), Pin - FVector(0., 0., PinSize * 1.3f * FMath::Max(1.f, static_cast<float>(FVector::Dist(Eye, Pin)) / PinNear)), 0.8f);
	GroundRing->SetWorldLocation(FVector(Goal.X, Goal.Y, Floor + 1.f));
	const float Wave = FMath::Fmod(Time * 0.6f, 1.f);
	GroundPulse->SetWorldLocation(FVector(Goal.X, Goal.Y, Floor + 1.2f));
	GroundPulse->SetWorldScale3D(FVector(1.f + 2.f * Wave));
	SetFade(GroundPulse, Alpha * (1.f - Wave));

	// The pin grows with distance so it reads from afar; "Arca Dwarapala · 11 m" above it, the same size at any distance, turned to the eye
	const float Distance = FMath::Max(150.f, static_cast<float>(FVector::Dist(Eye, Pin)));
	const float PinScale = FMath::Max(1.f, Distance / PinNear);
	Marker->SetWorldScale3D(FVector(PinScale));
	const float Scale = Distance * LabelPerDistance;
	Label->SetWorldLocation(Pin + FVector(0., 0., (PinSize * 0.7f + LabelGap) * PinScale));
	Label->SetWorldScale3D(FVector(Scale));
	Label->SetWorldRotation(FRotator(0.f, (-Look).Rotation().Yaw, 0.f));

	const int32 Metres = FMath::Max(0, FMath::RoundToInt(Remaining / 100.f));
	if (UMuseumMRLabelWidget* Widget = Cast<UMuseumMRLabelWidget>(Label->GetUserWidgetObject()))
	{
		if (Metres != LastMetres || !Name.EqualTo(LastName))
		{
			LastMetres = Metres;
			LastName = Name;
			Widget->SetLabel(FText::GetEmpty(), FText::Format(NSLOCTEXT("MuseumMRWaypoints", "Label", "{0}  ·  {1} m"), Name, Metres));
		}
		Widget->SetRenderOpacity(Alpha);
	}
}

// ====== Helpers ====== //

float AMuseumMRWaypoints::FloorAt(const FVector& Location) const
{
	// The (hidden) floor under the point (static world only: not the visitor, not the widgets), else the route's own height
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MuseumMRWaypointsFloor), false);
	Params.AddIgnoredActor(this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0., 0., 120.), Location - FVector(0., 0., 400.), Objects, Params)) return static_cast<float>(Hit.ImpactPoint.Z);

	return static_cast<float>(Location.Z);
}
