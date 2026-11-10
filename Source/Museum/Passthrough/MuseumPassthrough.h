#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MuseumPassthrough.generated.h"

class AMuseumExhibit;
class UFileMediaSource;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMediaPlayer;
class UMediaTexture;
class UProceduralMeshComponent;

/**
 * The "real world" of the demo mode: recorded video standing in for Meta Quest passthrough.
 * - Walk plate: a gallery walk recording, head-locked far behind everything, shown while the visitor is between exhibits
 *   (a new clip each time guidance starts; it only plays while the visitor moves).
 * - Station plate: the exhibit's own recording (AMuseumExhibit::PlateVideo, stabilized on the real object), world-locked
 *   at the exhibit's plate transform, shown while the visitor stands in that exhibit's ViewingZone and is not being guided.
 *   It replaces the walk plate once its first frames are decoded, so the cut never shows black.
 * The plates use PlateMaterial (texture parameter VideoParameter): the passthrough look (grain, desaturation, vignette).
 */
UCLASS()
class AMuseumPassthrough : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AMuseumPassthrough();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// ====== Settings ====== //

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	TObjectPtr<UMaterialInterface> PlateMaterial;

	/** Texture parameter of PlateMaterial that receives the video. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	FName VideoParameter = TEXT("Video");

	/** Gallery walks (Content/Movies/Walk), in tour order; the first plays from the start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	TArray<TObjectPtr<UFileMediaSource>> WalkClips;

	/** Distance of the head-locked walk plate: beyond the route, the beacons and every hologram. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough", meta = (ClampMin = "100", Units = "cm"))
	float WalkPlateDistance = 4000.f;

	/** Extra angle the walk plate covers past each edge of the view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough", meta = (ClampMin = "0", Units = "deg"))
	float WalkPlateMargin = 4.f;

	/** The walk recording plays only while the visitor moves faster than this (it holds its frame when they stop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough", meta = (ClampMin = "0", Units = "cm/s"))
	float WalkPlaySpeed = 30.f;

	/** Scalar parameter of PlateMaterial: the share of the plate the recording fills (AMuseumExhibit::PlateContentScale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	FName ContentScaleParameter = TEXT("ContentScale");

	/** Scalar parameters of PlateMaterial for the MR detection: strength 0..1, seconds since it started, and the real object's rect on the plate (UV). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	FName DetectAmountParameter = TEXT("DetectAmount");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough")
	FName DetectTimeParameter = TEXT("DetectTime");

	/** Width / height of the recordings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum|Passthrough", meta = (ClampMin = "0.1"))
	float VideoAspect = 16.f / 9.f;

public:
	// ====== API ====== //

	/** The exhibit whose recording is on screen, or null while the walk plate shows. */
	UFUNCTION(BlueprintPure, Category = "Museum|Passthrough")
	AMuseumExhibit* GetShownExhibit() const { return ShownExhibit; }

private:
	// ====== Components ====== //

	UPROPERTY(VisibleAnywhere, Category = "Museum|Passthrough")
	TObjectPtr<UProceduralMeshComponent> WalkPlate;

	UPROPERTY(VisibleAnywhere, Category = "Museum|Passthrough")
	TObjectPtr<UProceduralMeshComponent> StationPlate;

private:
	// ====== Media ====== //

	/** A player, its texture and the plate's material instance showing it (returned). */
	UMaterialInstanceDynamic* CreateMedia(UProceduralMeshComponent* Plate, TObjectPtr<UMediaPlayer>& OutPlayer, TObjectPtr<UMediaTexture>& OutTexture);

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> WalkPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> WalkTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> StationPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> StationTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StationMaterial;

private:
	// ====== State ====== //

	/** Follows guidance: the next walk clip each time a guided walk starts. */
	void UpdateWalkClip(bool bGuiding);

	/** The exhibit the visitor is visiting (in its zone, not guided away); opens its recording. */
	void UpdateStation(bool bGuiding);

	/** Which plate shows, and the walk plate in front of the eyes. */
	void UpdatePlates();

	/** The shown exhibit's detection look on the station plate (strength, time, the object's rect seen from its viewing spot). */
	void UpdateDetection();

	bool IsGuiding() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMuseumExhibit>> Exhibits;

	UPROPERTY(Transient)
	TObjectPtr<AMuseumExhibit> ActiveExhibit;

	UPROPERTY(Transient)
	TObjectPtr<AMuseumExhibit> ShownExhibit;

	int32 WalkClipIndex = 0;
	bool bWalkClipUsed = false;
	bool bWasGuiding = false;
	bool bWalkPlaying = false;
};
