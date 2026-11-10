#include "MR/MuseumMRStyle.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** Basic shapes are 100 cm. */
	constexpr double ShapeSize = 100.;

	UObject* LoadCached(const TCHAR* Path, TMap<FString, TWeakObjectPtr<UObject>>& Cache)
	{
		if (const TWeakObjectPtr<UObject>* Found = Cache.Find(Path); Found && Found->IsValid()) return Found->Get();

		UObject* Loaded = LoadObject<UObject>(nullptr, Path);
		Cache.Add(Path, Loaded);

		return Loaded;
	}

	UObject* Load(const TCHAR* Path)
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;

		return LoadCached(Path, Cache);
	}
}

namespace MuseumMR
{
	// ====== Colours ====== //

	const FLinearColor& Cyan()
	{
		static const FLinearColor Color = FLinearColor::FromSRGBColor(FColor(0x4F, 0xD8, 0xFF));
		return Color;
	}

	const FLinearColor& Gold()
	{
		static const FLinearColor Color = FLinearColor::FromSRGBColor(FColor(0xE9, 0xC4, 0x7C));
		return Color;
	}

	const FLinearColor& Text()
	{
		static const FLinearColor Color(0.93f, 0.95f, 0.96f, 1.f);
		return Color;
	}

	const FLinearColor& MutedText()
	{
		static const FLinearColor Color = FLinearColor::FromSRGBColor(FColor(0xA8, 0xBC, 0xC6));
		return Color;
	}

	const FLinearColor& Green()
	{
		static const FLinearColor Color = FLinearColor::FromSRGBColor(FColor(0x6B, 0xF2, 0xA6));
		return Color;
	}

	const FLinearColor& GlassFill()
	{
		static const FLinearColor Color(0.004f, 0.012f, 0.018f, 0.78f);
		return Color;
	}

	// ====== Fonts ====== //

	FSlateFontInfo Font(const EFace Face, const int32 Size)
	{
		const bool bLabel = Face == EFace::Label || Face == EFace::LabelBold;
		UObject* Family = Load(bLabel ? TEXT("/Game/Zoo/Fonts/F_UI_Rajdhani.F_UI_Rajdhani") : TEXT("/Game/Zoo/Fonts/F_UI_Inter.F_UI_Inter"));
		FName Typeface;
		switch (Face)
		{
		case EFace::Body: Typeface = TEXT("Regular"); break;
		case EFace::Medium: Typeface = TEXT("Medium"); break;
		case EFace::Title: Typeface = TEXT("SemiBold"); break;
		case EFace::Label: Typeface = TEXT("SemiBold"); break;
		case EFace::LabelBold: Typeface = TEXT("Bold"); break;
		}
		if (!Family) return FCoreStyle::GetDefaultFontStyle(Face == EFace::Body ? TEXT("Regular") : TEXT("Bold"), Size);

		return FSlateFontInfo(Family, Size, Typeface);
	}

	// ====== Brushes ====== //

	FSlateBrush RoundedBrush(const FLinearColor& Fill, const float Radius, const FLinearColor& Outline, const float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;

		return Brush;
	}

	// ====== Materials and meshes ====== //

	UStaticMesh* Shape(const EShape Shape)
	{
		switch (Shape)
		{
		case EShape::Cube: return Cast<UStaticMesh>(Load(TEXT("/Engine/BasicShapes/Cube.Cube")));
		case EShape::Plane: return Cast<UStaticMesh>(Load(TEXT("/Engine/BasicShapes/Plane.Plane")));
		case EShape::Cylinder: return Cast<UStaticMesh>(Load(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		case EShape::Sphere: return Cast<UStaticMesh>(Load(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		case EShape::Cone: return Cast<UStaticMesh>(Load(TEXT("/Engine/BasicShapes/Cone.Cone")));
		}

		return nullptr;
	}

	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, const EShape InShape, const EMaterial Material, const FName Name)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner, Name == NAME_None ? MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("MRMesh")) : Name);
		Mesh->SetStaticMesh(Shape(InShape));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Never part of the NavMesh (the waypoint chevrons wait at the origin, the visitor's spawn point, and cut a hole there)
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->bVisibleInRayTracing = false;
		Mesh->bReceivesDecals = false;
		Mesh->SetMobility(EComponentMobility::Movable);
		if (Parent) Mesh->SetupAttachment(Parent);
		Mesh->RegisterComponent();
		Owner->AddInstanceComponent(Mesh);

		const TCHAR* Path = Material == EMaterial::Glass ? TEXT("/Game/Museum/MR/M_MR_Glass.M_MR_Glass") : Material == EMaterial::Glow ? TEXT("/Game/Museum/MR/M_MR_Glow.M_MR_Glow") : TEXT("/Game/Museum/MR/M_MR_Ring.M_MR_Ring");
		if (UMaterialInterface* Base = Cast<UMaterialInterface>(Load(Path))) Mesh->SetMaterial(0, UMaterialInstanceDynamic::Create(Base, Mesh));

		return Mesh;
	}

	UMaterialInstanceDynamic* GetMaterial(const UStaticMeshComponent* Mesh)
	{
		return Mesh ? Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)) : nullptr;
	}

	void SetBoxSize(UStaticMeshComponent* Mesh, const FVector& Size)
	{
		if (Mesh) Mesh->SetRelativeScale3D(Size / ShapeSize);
	}

	void SetLine(UStaticMeshComponent* Mesh, const FVector& From, const FVector& To, const float Thickness)
	{
		if (!Mesh) return;

		const FVector Delta = To - From;
		const double Length = Delta.Size();
		Mesh->SetWorldLocationAndRotation((From + To) * 0.5, Length > KINDA_SMALL_NUMBER ? Delta.Rotation() : FRotator::ZeroRotator);
		Mesh->SetWorldScale3D(FVector(FMath::Max(Length, 0.01) / ShapeSize, Thickness / ShapeSize, Thickness / ShapeSize));
	}

	void SetColor(UStaticMeshComponent* Mesh, const FLinearColor& Color, const float Intensity)
	{
		UMaterialInstanceDynamic* Material = GetMaterial(Mesh);
		if (!Material) return;

		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetVectorParameterValue(TEXT("EdgeColor"), Color);
		if (Intensity >= 0.f) Material->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	}

	void SetFade(UStaticMeshComponent* Mesh, const float Fade)
	{
		SetScalar(Mesh, TEXT("Fade"), Fade);
		if (Mesh) Mesh->SetVisibility(Fade > 0.003f);
	}

	void SetScalar(UStaticMeshComponent* Mesh, const FName Name, const float Value)
	{
		if (UMaterialInstanceDynamic* Material = GetMaterial(Mesh)) Material->SetScalarParameterValue(Name, Value);
	}

	// ====== Math ====== //

	float Ease(const float Alpha)
	{
		const float A = FMath::Clamp(Alpha, 0.f, 1.f);

		return A * A * (3.f - 2.f * A);
	}
}
