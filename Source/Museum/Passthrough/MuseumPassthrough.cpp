#include "Passthrough/MuseumPassthrough.h"

#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Exhibit/MuseumExhibit.h"
#include "FileMediaSource.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "PathGuide/Components/PathGuideComponent.h"
#include "Player/MuseumPlayerController.h"
#include "Components/BoxComponent.h"
#include "ProceduralMeshComponent.h"

namespace
{
	/** A recording counts as on screen once this much of it has played (its first frames are decoded). */
	constexpr double StationReadySeconds = 0.05;

	/** Unit quad in the YZ plane facing -X (toward whoever looks along +X), UV (0, 0) at the top left as seen from there. */
	void BuildQuad(UProceduralMeshComponent* Plate)
	{
		const TArray<FVector> Vertices { { 0., -.5, .5 }, { 0., .5, .5 }, { 0., .5, -.5 }, { 0., -.5, -.5 } };
		const TArray<int32> Triangles { 0, 2, 1, 0, 3, 2 };
		const TArray<FVector> Normals { FVector::BackwardVector, FVector::BackwardVector, FVector::BackwardVector, FVector::BackwardVector };
		const TArray<FVector2D> UVs { { 0., 0. }, { 1., 0. }, { 1., 1. }, { 0., 1. } };
		Plate->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, {}, {}, false);
	}

	UProceduralMeshComponent* CreatePlate(AActor* Owner, const FName Name)
	{
		UProceduralMeshComponent* Plate = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		Plate->SetupAttachment(Owner->GetRootComponent());
		Plate->SetUsingAbsoluteLocation(true);
		Plate->SetUsingAbsoluteRotation(true);
		Plate->SetUsingAbsoluteScale(true);
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plate->SetCastShadow(false);
		Plate->bVisibleInRayTracing = false;
		Plate->bReceivesDecals = false;
		Plate->SetVisibility(false);
		return Plate;
	}
}

// ====== Lifecycle ====== //

AMuseumPassthrough::AMuseumPassthrough()
{
	// After the camera moved this frame, so the head-locked plate does not lag a frame behind
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	WalkPlate = CreatePlate(this, TEXT("WalkPlate"));
	StationPlate = CreatePlate(this, TEXT("StationPlate"));
}

void AMuseumPassthrough::BeginPlay()
{
	Super::BeginPlay();

	BuildQuad(WalkPlate);
	BuildQuad(StationPlate);
	CreateMedia(WalkPlate, WalkPlayer, WalkTexture);
	StationMaterial = CreateMedia(StationPlate, StationPlayer, StationTexture);

	for (TActorIterator<AMuseumExhibit> It(GetWorld()); It; ++It) Exhibits.Add(*It);

	// The lobby: the first walk; it pauses on its first frames until the visitor moves
	if (WalkClips.IsValidIndex(0) && WalkClips[0])
	{
		WalkPlayer->OpenSource(WalkClips[0]);
		bWalkPlaying = true;
	}
	WalkPlate->SetVisibility(true);
}

void AMuseumPassthrough::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WalkPlayer) WalkPlayer->Close();
	if (StationPlayer) StationPlayer->Close();
	Super::EndPlay(EndPlayReason);
}

void AMuseumPassthrough::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bGuiding = IsGuiding();
	UpdateWalkClip(bGuiding);
	UpdateStation(bGuiding);
	UpdatePlates();
	UpdateDetection();
	bWasGuiding = bGuiding;
}

// ====== Media ====== //

UMaterialInstanceDynamic* AMuseumPassthrough::CreateMedia(UProceduralMeshComponent* Plate, TObjectPtr<UMediaPlayer>& OutPlayer, TObjectPtr<UMediaTexture>& OutTexture)
{
	OutPlayer = NewObject<UMediaPlayer>(this);
	OutPlayer->SetLooping(true);
	OutPlayer->PlayOnOpen = true;

	OutTexture = NewObject<UMediaTexture>(this);
	OutTexture->AutoClear = true;
	OutTexture->ClearColor = FLinearColor::Black;
	OutTexture->SetMediaPlayer(OutPlayer);
	OutTexture->UpdateResource();

	if (!PlateMaterial) return nullptr;
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(PlateMaterial, this);
	Material->SetTextureParameterValue(VideoParameter, OutTexture);
	Plate->SetMaterial(0, Material);

	return Material;
}

// ====== State ====== //

bool AMuseumPassthrough::IsGuiding() const
{
	const AMuseumPlayerController* Visitor = Cast<AMuseumPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	const UWzPathGuideComponent* PathGuide = Visitor ? Visitor->GetPathGuide() : nullptr;

	return PathGuide && PathGuide->GetState() == EPathGuideState::Guiding;
}

void AMuseumPassthrough::UpdateWalkClip(const bool bGuiding)
{
	if (!bGuiding || bWasGuiding || WalkClips.IsEmpty()) return;

	// The first guided walk continues the lobby clip; each later one starts the next clip
	if (bWalkClipUsed)
	{
		WalkClipIndex = (WalkClipIndex + 1) % WalkClips.Num();
		if (WalkClips[WalkClipIndex])
		{
			WalkPlayer->PlayOnOpen = true;
			WalkPlayer->OpenSource(WalkClips[WalkClipIndex]);
			bWalkPlaying = true;
		}
	}
	bWalkClipUsed = true;
}

void AMuseumPassthrough::UpdateStation(const bool bGuiding)
{
	AMuseumExhibit* Wanted = nullptr;
	if (!bGuiding)
	{
		for (AMuseumExhibit* Exhibit : Exhibits)
		{
			if (IsValid(Exhibit) && Exhibit->PlateVideo && Exhibit->IsZoneActive())
			{
				Wanted = Exhibit;
				break;
			}
		}
	}
	if (Wanted == ActiveExhibit) return;

	ActiveExhibit = Wanted;
	if (!ActiveExhibit)
	{
		StationPlayer->Close();
		return;
	}

	const FTransform Plate = ActiveExhibit->GetPlateTransform();
	StationPlate->SetWorldTransform(FTransform(Plate.GetRotation(), Plate.GetLocation(), FVector(1., ActiveExhibit->PlateSize.X, ActiveExhibit->PlateSize.Y)));
	if (StationMaterial) StationMaterial->SetScalarParameterValue(ContentScaleParameter, ActiveExhibit->PlateContentScale);
	StationPlayer->OpenSource(ActiveExhibit->PlateVideo);
}

void AMuseumPassthrough::UpdatePlates()
{
	// The station recording takes over once it really plays
	const bool bStationReady = ActiveExhibit && StationPlayer->IsPlaying() && StationPlayer->GetTime().GetTotalSeconds() > StationReadySeconds;
	ShownExhibit = bStationReady ? ActiveExhibit.Get() : nullptr;
	StationPlate->SetVisibility(bStationReady);
	WalkPlate->SetVisibility(!bStationReady);

	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!CameraManager) return;

	// The walk recording runs while the visitor walks and holds its frame while they stand
	const APawn* Pawn = PC->GetPawn();
	const bool bMoving = !bStationReady && Pawn && Pawn->GetVelocity().Size2D() > WalkPlaySpeed;
	if (bMoving && !bWalkPlaying && WalkPlayer->IsReady())
	{
		bWalkPlaying = WalkPlayer->Play();
	}
	else if (!bMoving && bWalkPlaying && WalkPlayer->GetTime().GetTotalSeconds() > StationReadySeconds)
	{
		// Paused only once a frame is up (a pause right after opening would leave the plate black)
		bWalkPlaying = !WalkPlayer->Pause();
	}
	if (bStationReady) return;

	// In front of the eyes, filling the view plus a margin, in the recording's aspect
	int32 ViewWidth = 16, ViewHeight = 9;
	PC->GetViewportSize(ViewWidth, ViewHeight);
	const float ViewAspect = ViewHeight > 0 ? float(ViewWidth) / ViewHeight : VideoAspect;
	const float HalfFov = FMath::DegreesToRadians(CameraManager->GetFOVAngle() * .5f);
	const float HalfFovV = FMath::Atan(FMath::Tan(HalfFov) / ViewAspect);
	const float Margin = FMath::DegreesToRadians(WalkPlateMargin);
	const float Width = FMath::Max(2.f * WalkPlateDistance * FMath::Tan(HalfFov + Margin), 2.f * WalkPlateDistance * FMath::Tan(HalfFovV + Margin) * VideoAspect);

	const FRotator View = CameraManager->GetCameraRotation();
	WalkPlate->SetWorldTransform(FTransform(View, CameraManager->GetCameraLocation() + View.Vector() * WalkPlateDistance, FVector(1., Width, Width / VideoAspect)));
}

void AMuseumPassthrough::UpdateDetection()
{
	const AMuseumExhibit* Exhibit = ShownExhibit.Get();
	if (!StationMaterial || !Exhibit || !Exhibit->RealObject) return;

	StationMaterial->SetScalarParameterValue(DetectAmountParameter, Exhibit->GetDetectAmount());
	StationMaterial->SetScalarParameterValue(DetectTimeParameter, Exhibit->GetDetectTime());

	// The real object's box seen from the viewing spot, projected onto the plate (plate space: X toward the plate, Y right, Z up)
	const FTransform Plate = Exhibit->GetPlateTransform();
	const FVector Eye = Plate.InverseTransformPosition(Exhibit->GetViewPoint());
	const FTransform& Box = Exhibit->RealObject->GetComponentTransform();
	const FVector Extent = Exhibit->RealObject->GetUnscaledBoxExtent();
	FVector2D Min(1.), Max(0.);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Local((Corner & 1) ? Extent.X : -Extent.X, (Corner & 2) ? Extent.Y : -Extent.Y, (Corner & 4) ? Extent.Z : -Extent.Z);
		const FVector Point = Plate.InverseTransformPosition(Box.TransformPosition(Local));
		const double Depth = Point.X - Eye.X;
		if (Depth <= 1.) continue;

		// Along the ray from the eye to the plate plane (X = 0)
		const double T = -Eye.X / Depth;
		const FVector2D OnPlate(Eye.Y + (Point.Y - Eye.Y) * T, Eye.Z + (Point.Z - Eye.Z) * T);
		const FVector2D UV(OnPlate.X / Exhibit->PlateSize.X + .5, .5 - OnPlate.Y / Exhibit->PlateSize.Y);
		Min = FVector2D::Min(Min, UV);
		Max = FVector2D::Max(Max, UV);
	}
	StationMaterial->SetScalarParameterValue(TEXT("RectU0"), Min.X);
	StationMaterial->SetScalarParameterValue(TEXT("RectV0"), Min.Y);
	StationMaterial->SetScalarParameterValue(TEXT("RectU1"), Max.X);
	StationMaterial->SetScalarParameterValue(TEXT("RectV1"), Max.Y);
}
