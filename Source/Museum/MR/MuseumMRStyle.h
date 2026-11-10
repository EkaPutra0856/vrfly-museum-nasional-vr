#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

class AActor;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

/**
 * The look of the MR layer (Docs/STORYBOARD.md): dark glass with real thickness, a thin cyan edge glow, gold accents for
 * the cultural headings, Inter for reading and Rajdhani for HUD labels. Also the 3D building blocks every MR actor uses:
 * glass slabs, glowing lines and rings made from the engine's basic shapes and the M_MR_* materials
 * (Tools/Editor/build_mr_assets.py).
 */
namespace MuseumMR
{
	// ====== Colours ====== //

	/** #4FD8FF: edges, buttons, routes. */
	const FLinearColor& Cyan();

	/** Gold accent of the cultural headings. */
	const FLinearColor& Gold();

	const FLinearColor& Text();
	const FLinearColor& MutedText();

	/** Visited / done. */
	const FLinearColor& Green();

	/** Dark glass behind texts drawn inside a widget (the curved HUD pieces have no 3D slab). */
	const FLinearColor& GlassFill();

	// ====== Fonts ====== //

	enum class EFace : uint8
	{
		/** Inter Regular: body text. */
		Body,
		/** Inter Medium. */
		Medium,
		/** Inter SemiBold: titles. */
		Title,
		/** Rajdhani SemiBold: HUD labels, caps headings. */
		Label,
		/** Rajdhani Bold: numbers, big HUD words. */
		LabelBold,
	};

	FSlateFontInfo Font(EFace Face, int32 Size);

	// ====== Brushes ====== //

	/** Rounded box, optionally outlined (the glass edge glow of 2D pieces). */
	FSlateBrush RoundedBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);

	// ====== Materials and meshes ====== //

	enum class EMaterial : uint8
	{
		Glass,
		Glow,
		Ring,
	};

	enum class EShape : uint8
	{
		Cube,
		Plane,
		Cylinder,
		Sphere,
		Cone,
	};

	UStaticMesh* Shape(EShape Shape);

	/** A mesh component made at runtime (registered, no collision, no shadow) with a dynamic instance of the MR material. */
	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, EShape Shape, EMaterial Material, FName Name = NAME_None);

	UMaterialInstanceDynamic* GetMaterial(const UStaticMeshComponent* Mesh);

	/** A box (the cube, 100 cm) sized in cm, centred on its component. */
	void SetBoxSize(UStaticMeshComponent* Mesh, const FVector& Size);

	/** A cube stretched between two world points: a glowing line of the given thickness (cm). */
	void SetLine(UStaticMeshComponent* Mesh, const FVector& From, const FVector& To, float Thickness);

	/** Colour, intensity and fade of a Glow / Ring / Glass material (the glass takes Color as its edge colour). */
	void SetColor(UStaticMeshComponent* Mesh, const FLinearColor& Color, float Intensity = -1.f);
	void SetFade(UStaticMeshComponent* Mesh, float Fade);
	void SetScalar(UStaticMeshComponent* Mesh, FName Name, float Value);

	// ====== Math ====== //

	/** Ease in-out (smoothstep). */
	float Ease(float Alpha);
}
