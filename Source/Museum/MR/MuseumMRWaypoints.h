#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MuseumMRWaypoints.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;

/** One floor arrow: two glowing arms forming ">" along the route. */
USTRUCT()
struct FMuseumWaypointChevron
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Left;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Right;
};

/**
 * The guided walk in MR (owner's reference W2): a dotted path of small glowing dots flat on the floor along the Path Guide route,
 * every third one a small arrow, a wave of light running along it toward the goal; at the destination a ring on the floor, a
 * slim beam of light and a map pin floating at head height with its label "Arca Dwarapala · 11 m".
 * Replaces the plugin's own route visuals (hidden). Owned and fed by AMuseumMRHud.
 */
UCLASS(NotPlaceable)
class AMuseumMRWaypoints : public AActor
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	AMuseumMRWaypoints();

protected:
	virtual void BeginPlay() override;

public:
	// ====== API ====== //

	/** Route in the world (empty = nothing shows), the visitor, the destination's name and the distance left (cm). */
	void UpdateWaypoints(float DeltaSeconds, const TArray<FVector>& Route, const FVector& Eye, const FText& Name, float Remaining);

private:
	float FloorAt(const FVector& Location) const;

	UPROPERTY(VisibleAnywhere, Category = "Museum|Waypoints")
	TObjectPtr<USceneComponent> Marker;

	UPROPERTY(VisibleAnywhere, Category = "Museum|Waypoints")
	TObjectPtr<UWidgetComponent> Label;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Dots;

	UPROPERTY(Transient)
	TArray<FMuseumWaypointChevron> Arrows;

	/** The pin: a round head with a dark eye, a cone pointing down. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PinHead;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PinEye;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PinTip;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Beam;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GroundRing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GroundPulse;

	float Shown = 0.f;
	float Time = 0.f;
	FText LastName;
	int32 LastMetres = -1;
};
