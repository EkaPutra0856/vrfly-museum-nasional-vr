#include "Exhibit/MuseumExhibitData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "MuseumExhibitData"

// ====== Validation ====== //

#if WITH_EDITOR
EDataValidationResult UMuseumExhibitData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!PlaceTag.IsValid())
	{
		Context.AddError(LOCTEXT("NoPlace", "The exhibit has no PlaceTag."));
		Result = EDataValidationResult::Invalid;
	}
	if (Mesh.IsNull())
	{
		Context.AddError(LOCTEXT("NoMesh", "The exhibit has no hologram mesh."));
		Result = EDataValidationResult::Invalid;
	}
	if (Panels.Num() != 3)
	{
		Context.AddWarning(FText::Format(LOCTEXT("PanelCount", "The script has 3 panels per object, this exhibit has {0}."), Panels.Num()));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
