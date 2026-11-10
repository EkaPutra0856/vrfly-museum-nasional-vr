// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hologram/MuseumHologramComponent.h"

#include "Components/MeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Hologram/MuseumHologramProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"

/** The parameter contract with M_MuseumHologram and the Niagara systems. Heights are mesh-local and unscaled: Z up from the mesh pivot (the feet). */
static const FName RevealHeightParameter(TEXT("RevealHeight"));
static const FName HoloHeightParameter(TEXT("HoloHeight"));
static const FName GlitchParameter(TEXT("Glitch"));
static const FName SpeakingParameter(TEXT("Speaking"));
static const FName NiagaraRevealHeight(TEXT("User.RevealHeight"));
static const FName NiagaraHoloHeight(TEXT("User.HoloHeight"));
static const FName NiagaraDirection(TEXT("User.Direction"));
static const FName NiagaraRingRadius(TEXT("User.RingRadius"));
static const FName NiagaraTint(TEXT("User.Tint"));
static const FName NiagaraMesh(TEXT("User.Mesh"));

/** A reveal height above any mesh: fully visible. */
static constexpr float RevealVisible = 10000.f;

/** The speaking glow snaps to its goal inside this distance, so the tick can sleep. */
static constexpr float BlendTolerance = 0.01f;

// ====== Lifecycle ====== //

UMuseumHologramComponent::UMuseumHologramComponent()
{
	// Ticks only while something animates.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMuseumHologramComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickPhase(DeltaTime);
	TickGlitch(DeltaTime);
	TickSpeaking(DeltaTime);
	TickAmbient(DeltaTime);

	// Sleep when nothing animates.
	SetComponentTickEnabled(IsAnimating());
}

// ====== API ====== //

void UMuseumHologramComponent::SetTarget(UMeshComponent* InTarget)
{
	Target = InTarget;
	if (!Target) return;

	// Dynamic material on the first slot.
	Material = Target->CreateAndSetMaterialInstanceDynamic(0);
	if (!Profile || !Material) return;

	// Measure the mesh: height in mesh units, ring radius in world units (local bounds work for skinned and static meshes).
	const FBoxSphereBounds Bounds = Target->CalcBounds(FTransform::Identity);
	HoloHeight = Bounds.Origin.Z + Bounds.BoxExtent.Z;
	RingRadius = FMath::Min(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * Profile->RingRadiusScale * Target->GetComponentScale().X, Profile->MaxRingRadius);

	// Hidden until Appear (the material's front-layer mask reads custom depth); the stencil tells the camera halo whose color to use.
	Target->SetRenderCustomDepth(true);
	Target->SetCustomDepthStencilValue(Profile->HaloStencil);
	Material->SetScalarParameterValue(HoloHeightParameter, HoloHeight);
	SetRevealHeight(0.f);
}

void UMuseumHologramComponent::Appear(const float Seconds)
{
	if (Phase == EMuseumHologramPhase::Appearing || Phase == EMuseumHologramPhase::Shown) return;

	// Without a profile there is nothing to play.
	if (!Profile)
	{
		Phase = EMuseumHologramPhase::Shown;
		OnAppeared.Broadcast();

		return;
	}

	// Resume from wherever the line is (a Disappear may be cut short).
	AppearSeconds = Seconds > 0.f ? Seconds : Profile->AppearSeconds;
	PhaseTime = RevealAlpha * AppearSeconds;
	Phase = EMuseumHologramPhase::Appearing;

	// Ring and particles, the hover effect, the sound.
	SpawnMaterialize(1.f);
	StopEffect(AmbientComponent);
	AmbientComponent = SpawnEffect(Profile->AmbientEffect);
	if (AmbientComponent) AmbientComponent->Activate(true);
	AmbientRemaining = 0.f;
	PlaySound(Profile->AppearSound);

	SetComponentTickEnabled(true);
}

void UMuseumHologramComponent::Disappear(const float Seconds, const bool bLinger)
{
	if (Phase == EMuseumHologramPhase::Disappearing) return;

	// Nothing to dematerialize: never shown, or no profile.
	if (Phase == EMuseumHologramPhase::Hidden || !Profile)
	{
		Phase = EMuseumHologramPhase::Hidden;
		OnDisappeared.Broadcast();

		return;
	}

	// Start from wherever the line is (an Appear may be cut short).
	DisappearSeconds = Seconds > 0.f ? Seconds : Profile->DisappearSeconds;
	LingerSeconds = bLinger ? Profile->LingerSeconds : 0.f;
	PhaseTime = (1.f - RevealAlpha) * DisappearSeconds;
	Phase = EMuseumHologramPhase::Disappearing;

	// The hover effect ends; ring and particles take over.
	StopEffect(AmbientComponent);
	SpawnMaterialize(-1.f);
	PlaySound(Profile->DisappearSound);

	SetComponentTickEnabled(true);
}

void UMuseumHologramComponent::Glitch()
{
	if (!Profile || Phase == EMuseumHologramPhase::Hidden) return;

	// Start the burst.
	GlitchRemaining = Profile->GlitchSeconds;
	PlaySound(Profile->GlitchSound);
	SetComponentTickEnabled(true);
}

void UMuseumHologramComponent::SetSpeaking(const bool bInSpeaking)
{
	if (!Profile) return;

	// The tick blends the glow toward the new goal.
	bSpeaking = bInSpeaking;
	SetComponentTickEnabled(true);
}

// ====== Helpers ====== //

void UMuseumHologramComponent::TickPhase(const float DeltaTime)
{
	const bool bAppearing = Phase == EMuseumHologramPhase::Appearing;
	if (!bAppearing && Phase != EMuseumHologramPhase::Disappearing) return;

	// The line follows the phase time: up when appearing, down when disappearing.
	PhaseTime += DeltaTime;
	RevealAlpha = bAppearing ? FMath::Clamp(PhaseTime / AppearSeconds, 0.f, 1.f) : 1.f - FMath::Clamp(PhaseTime / DisappearSeconds, 0.f, 1.f);
	SetRevealHeight(RevealHeightAt(RevealAlpha));

	// Appearing ends with the line at the top.
	if (bAppearing)
	{
		if (RevealAlpha >= 1.f) FinishAppear();

		return;
	}

	// Disappearing: the ring ends at the feet, the last particles get their linger.
	if (RevealAlpha <= 0.f) StopEffect(MaterializeComponent);
	if (PhaseTime >= DisappearSeconds + LingerSeconds) FinishDisappear();
}

void UMuseumHologramComponent::TickGlitch(const float DeltaTime)
{
	if (GlitchRemaining <= 0.f) return;

	// Fades out over the glitch time.
	GlitchRemaining = FMath::Max(GlitchRemaining - DeltaTime, 0.f);
	if (Material) Material->SetScalarParameterValue(GlitchParameter, GlitchRemaining / Profile->GlitchSeconds);
}

void UMuseumHologramComponent::TickSpeaking(const float DeltaTime)
{
	const float Goal = bSpeaking ? 1.f : 0.f;
	if (FMath::IsNearlyEqual(SpeakingValue, Goal)) return;

	// Blend toward the goal, snapping when close.
	SpeakingValue = FMath::FInterpTo(SpeakingValue, Goal, DeltaTime, Profile->SpeakingBlendSpeed);
	if (FMath::IsNearlyEqual(SpeakingValue, Goal, BlendTolerance)) SpeakingValue = Goal;
	if (Material) Material->SetScalarParameterValue(SpeakingParameter, SpeakingValue);
}

void UMuseumHologramComponent::TickAmbient(const float DeltaTime)
{
	if (AmbientRemaining <= 0.f) return;

	// The spawn disc has shown long enough: let it finish (a Disappear / Appear starts a fresh one)
	AmbientRemaining -= DeltaTime;
	if (AmbientRemaining <= 0.f && Phase == EMuseumHologramPhase::Shown) StopEffect(AmbientComponent);
}

bool UMuseumHologramComponent::IsAnimating() const
{
	const float Goal = bSpeaking ? 1.f : 0.f;

	return Phase == EMuseumHologramPhase::Appearing || Phase == EMuseumHologramPhase::Disappearing || GlitchRemaining > 0.f || AmbientRemaining > 0.f || !FMath::IsNearlyEqual(SpeakingValue, Goal);
}

void UMuseumHologramComponent::SetRevealHeight(const float Height) const
{
	if (Material) Material->SetScalarParameterValue(RevealHeightParameter, Height);
	if (IsValid(MaterializeComponent)) MaterializeComponent->SetVariableFloat(NiagaraRevealHeight, Height);
}

float UMuseumHologramComponent::RevealHeightAt(const float Alpha) const
{
	// Eased by the profile's curve, smoothstep when there is none.
	const UCurveFloat* Curve = Profile->RevealCurve;
	const float Eased = Curve ? FMath::Clamp(Curve->GetFloatValue(Alpha), 0.f, 1.f) : FMath::SmoothStep(0.f, 1.f, Alpha);

	return (HoloHeight + Profile->BandOvershoot) * Eased;
}

void UMuseumHologramComponent::FinishAppear()
{
	// The line leaves the mesh; the particles live out their linger. The hover disc stays only for the profile's seconds.
	SetRevealHeight(RevealVisible);
	StopEffect(MaterializeComponent);
	AmbientRemaining = Profile->AmbientSeconds;

	Phase = EMuseumHologramPhase::Shown;
	OnAppeared.Broadcast();
}

void UMuseumHologramComponent::FinishDisappear()
{
	// The ring already ended at the feet; the hover effect is gone too.
	StopEffect(AmbientComponent);

	Phase = EMuseumHologramPhase::Hidden;
	OnDisappeared.Broadcast();
}

FVector UMuseumHologramComponent::GetEffectLocation() const
{
	const FVector Pivot = Target->GetComponentLocation();
	if (!Profile->bRingOnGround) return Pivot;

	// Under the body, not the pivot: some meshes sit far off their pivot
	const FVector Center = Target->CalcBounds(Target->GetComponentTransform()).Origin;
	const FVector Body { Center.X, Center.Y, Pivot.Z };

	// The ground under the body (the owner and its parts are not the ground)
	FCollisionQueryParams Params { SCENE_QUERY_STAT(MuseumHologramGround), false, GetOwner() };
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Body + FVector{ 0.f, 0.f, 60.f }, Body - FVector{ 0.f, 0.f, Profile->GroundSearch }, ECC_Visibility, Params)) return Hit.ImpactPoint + FVector{ 0.f, 0.f, 3.f };

	return Body;
}

UNiagaraComponent* UMuseumHologramComponent::SpawnEffect(UNiagaraSystem* System) const
{
	if (!System || !Target) return nullptr;

	// Attached at the mesh pivot (or on the ground under it, scanning the terrain), riding along at world scale; activated by the caller.
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Target, NAME_None, GetEffectLocation(), FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition, true, false);
	if (Effect) Effect->SetUsingAbsoluteScale(true);
	if (Effect) Effect->SetWorldScale3D(FVector::OneVector);
	if (!Effect) return nullptr;

	// Shared parameters: the mesh to sample, its size, the accent color.
	Effect->SetVariableObject(NiagaraMesh, Target);
	Effect->SetVariableFloat(NiagaraHoloHeight, HoloHeight);
	Effect->SetVariableFloat(NiagaraRingRadius, RingRadius);
	Effect->SetVariableLinearColor(NiagaraTint, Profile->AccentTint);

	return Effect;
}

void UMuseumHologramComponent::SpawnMaterialize(const float Direction)
{
	StopEffect(MaterializeComponent);

	MaterializeComponent = SpawnEffect(Profile->MaterializeEffect);
	if (!MaterializeComponent) return;

	// The effect reads the same reveal height as the material, so the ring and the particles can never drift from it.
	MaterializeComponent->SetVariableFloat(NiagaraDirection, Direction);
	MaterializeComponent->SetVariableFloat(NiagaraRevealHeight, RevealHeightAt(RevealAlpha));
	MaterializeComponent->Activate(true);
}

void UMuseumHologramComponent::StopEffect(TObjectPtr<UNiagaraComponent>& Effect)
{
	// Deactivate, not destroy: the live particles finish, then the component cleans itself up.
	if (IsValid(Effect)) Effect->Deactivate();
	Effect = nullptr;
}

void UMuseumHologramComponent::PlaySound(USoundBase* Sound) const
{
	if (Sound && Target) UGameplayStatics::SpawnSoundAttached(Sound, Target);
}