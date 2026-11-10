#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetInteractionComponent.h"
#include "MuseumMRPointer.generated.h"

class APlayerController;
class UPrimitiveComponent;
class UWidgetComponent;

/**
 * The visitor's pointer on one MR actor's world widgets: the VR laser (UMuseumVRComponent::GetPointerRay), else the mouse
 * cursor or the view centre. Each frame its owner names the widget components that can take it; the nearest one the ray
 * crosses becomes the custom hit, so Slate hover, clicks and the wheel reach the widget under it. Every owner needs its
 * own PointerIndex (shared ones cancel each other's clicks).
 */
UCLASS(ClassGroup = (Museum), meta = (BlueprintSpawnableComponent))
class UMuseumMRPointer : public UWidgetInteractionComponent
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	UMuseumMRPointer();

public:
	// ====== API ====== //

	/** The ray of the controller's pointer (laser, mouse or view centre). */
	static bool GetRay(const APlayerController* Controller, FVector& OutStart, FVector& OutDirection);

	/** Aims at the nearest visible target the ray crosses (none: no hit). Returns the hit target. */
	UWidgetComponent* UpdateTargets(const APlayerController* Controller, const TArray<UWidgetComponent*>& Targets);

	/** True when the ray crosses this component (any primitive, e.g. a hologram's click box). */
	bool RayHits(const APlayerController* Controller, const UPrimitiveComponent* Component) const;

	/** Presses on the widget under the pointer; false when there is none. */
	bool Press();

	void Release();

	/** Wheel notches on the widget under the pointer. */
	void Scroll(float Notches);

	UWidgetComponent* GetTarget() const { return Target.Get(); }

private:
	TWeakObjectPtr<UWidgetComponent> Target;
	bool bPressed = false;
};
