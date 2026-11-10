#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Language/MuseumLanguageSubsystem.h"
#include "MuseumExhibitData.generated.h"

class UFileMediaSource;
class UMuseumHologramProfile;
class UStaticMesh;
class UTexture2D;

/** One information card of an exhibit: its title, its text and the voice-over read while it is open (Skrip_AR_Voiceover.txt). */
USTRUCT(BlueprintType)
struct FMuseumExhibitPanel
{
	GENERATED_BODY()

	/** Small caps line above the title, e.g. "IDENTITAS & ASAL USUL". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FMuseumText Heading;

	/** e.g. "Mengenal Arca Dwarapala". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FMuseumText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FMuseumText Body;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FMuseumSound VoiceOver;

	/** Optional supporting photo: the open card shows it above a shorter text (not every card is all text). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	TSoftObjectPtr<UTexture2D> Image;
};

/** Everything one museum object shows and says. Pure data, read by AMuseumExhibit and the destination list. */
UCLASS(BlueprintType)
class UMuseumExhibitData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ====== Identity ====== //

	/** Tour number (01..06): the badge on the cards and the tour order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0"))
	int32 Number = 0;

	/** The Path Guide place of this object (PathGuide.Place.Museum.*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (Categories = "PathGuide.Place"))
	FGameplayTag PlaceTag;

	/** e.g. "Arca Dwarapala". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FMuseumText Name;

	/** One line under the name: period, origin, material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FMuseumText Subtitle;

	/** Sent to the narrative when the visitor has heard this object (all its cards opened). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Narrative.Event"))
	FGameplayTag VisitedEvent;

public:
	// ====== Hologram ====== //

	/** The 3D artifact (from Model_3D_*.glb). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hologram")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Real height of the object (cm); the mesh is scaled to it, then by HologramScale. 0 = the mesh as imported. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hologram", meta = (ClampMin = "0", Units = "cm"))
	float RealHeight = 0.f;

	/** Hologram size relative to the real object (a smaller twin floats beside the original). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hologram", meta = (ClampMin = "0.05"))
	float HologramScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hologram")
	TObjectPtr<UMuseumHologramProfile> HologramProfile;

public:
	// ====== Information ====== //

	/** Played when the hologram appears (01_Judul). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information")
	FMuseumSound TitleVoiceOver;

	/** The three cards, in order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information", meta = (TitleProperty = "Title"))
	TArray<FMuseumExhibitPanel> Panels;

	/** Optional video card after the three cards (Content/Movies/Exhibits); it plays on the card and on a large 3D screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information")
	TSoftObjectPtr<UFileMediaSource> MediaVideo;

	/** Destination card and MEDIA card picture (Gambar_Pilihan_*). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Information")
	TSoftObjectPtr<UTexture2D> Thumbnail;

#if WITH_EDITOR
public:
	// ====== Validation ====== //

	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
