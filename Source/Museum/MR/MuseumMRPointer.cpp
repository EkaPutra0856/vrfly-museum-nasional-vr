#include "MR/MuseumMRPointer.h"

#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "VR/MuseumVRComponent.h"

namespace
{
	constexpr float RayLength = 15000.f;
}

// ====== Lifecycle ====== //

UMuseumMRPointer::UMuseumMRPointer()
{
	InteractionSource = EWidgetInteractionSource::Custom;
	InteractionDistance = RayLength;
	bShowDebug = false;
}

// ====== API ====== //

bool UMuseumMRPointer::GetRay(const APlayerController* Controller, FVector& OutStart, FVector& OutDirection)
{
	if (!Controller) return false;

	// VR: the right-hand laser
	if (UMuseumVRComponent::GetPointerRay(Controller, OutStart, OutDirection)) return true;

	// Desktop: the cursor when it shows, else the view centre
	FRotator Rotation;
	Controller->GetPlayerViewPoint(OutStart, Rotation);
	OutDirection = Rotation.Vector();
	float X, Y;
	if (Controller->bShowMouseCursor && Controller->GetMousePosition(X, Y)) Controller->DeprojectScreenPositionToWorld(X, Y, OutStart, OutDirection);

	return true;
}

UWidgetComponent* UMuseumMRPointer::UpdateTargets(const APlayerController* Controller, const TArray<UWidgetComponent*>& Targets)
{
	FHitResult Best;
	UWidgetComponent* BestTarget = nullptr;
	FVector Start, Direction;
	if (GetRay(Controller, Start, Direction))
	{
		const FVector End = Start + Direction * RayLength;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(MuseumMRPointer), false);
		for (UWidgetComponent* Candidate : Targets)
		{
			FHitResult Hit;
			if (!Candidate || !Candidate->IsVisible() || !Candidate->LineTraceComponent(Hit, Start, End, Params)) continue;
			if (BestTarget && Hit.Distance >= Best.Distance) continue;

			Best = Hit;
			BestTarget = Candidate;
		}
		if (BestTarget)
		{
			Best.bBlockingHit = true;
			Best.Component = BestTarget;
			Best.HitObjectHandle = FActorInstanceHandle(BestTarget->GetOwner());
			Best.TraceStart = Start;
			Best.TraceEnd = End;
		}
	}

	Target = BestTarget;
	SetCustomHitResult(Best);

	return BestTarget;
}

bool UMuseumMRPointer::RayHits(const APlayerController* Controller, const UPrimitiveComponent* Component) const
{
	FVector Start, Direction;
	if (!Component || !GetRay(Controller, Start, Direction)) return false;

	// Query only this component, even when its collision is off for the world
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(MuseumMRPointerProbe), false);

	return const_cast<UPrimitiveComponent*>(Component)->LineTraceComponent(Hit, Start, Start + Direction * RayLength, Params);
}

bool UMuseumMRPointer::Press()
{
	if (!Target.IsValid() || !IsOverHitTestVisibleWidget()) return false;

	PressPointerKey(EKeys::LeftMouseButton);
	bPressed = true;

	return true;
}

void UMuseumMRPointer::Release()
{
	if (bPressed) ReleasePointerKey(EKeys::LeftMouseButton);
	bPressed = false;
}

void UMuseumMRPointer::Scroll(const float Notches)
{
	if (Target.IsValid() && Notches != 0.f) ScrollWheel(Notches);
}
