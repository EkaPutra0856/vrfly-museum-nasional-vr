#include "Language/MuseumLanguageSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"

// ====== Texts ====== //

const FText& FMuseumText::Get(const EMuseumLanguage Language) const
{
	return Language == EMuseumLanguage::English && !English.IsEmpty() ? English : Indonesian;
}

USoundBase* FMuseumSound::Load(const EMuseumLanguage Language) const
{
	const TSoftObjectPtr<USoundBase>& Clip = Language == EMuseumLanguage::English && !English.IsNull() ? English : Indonesian;

	return Clip.LoadSynchronous();
}

// ====== API ====== //

void UMuseumLanguageSubsystem::SetLanguage(const EMuseumLanguage InLanguage)
{
	if (Language == InLanguage) return;

	Language = InLanguage;
	OnLanguageChanged.Broadcast(Language);
}

void UMuseumLanguageSubsystem::ToggleLanguage()
{
	SetLanguage(Language == EMuseumLanguage::Indonesian ? EMuseumLanguage::English : EMuseumLanguage::Indonesian);
}

FText UMuseumLanguageSubsystem::GetLanguageCode(const EMuseumLanguage InLanguage)
{
	return InLanguage == EMuseumLanguage::English ? NSLOCTEXT("Museum", "LanguageEN", "EN") : NSLOCTEXT("Museum", "LanguageID", "ID");
}

UMuseumLanguageSubsystem* UMuseumLanguageSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;

	return Instance ? Instance->GetSubsystem<UMuseumLanguageSubsystem>() : nullptr;
}

EMuseumLanguage UMuseumLanguageSubsystem::GetCurrent(const UObject* WorldContext)
{
	const UMuseumLanguageSubsystem* Subsystem = Get(WorldContext);

	return Subsystem ? Subsystem->GetLanguage() : EMuseumLanguage::Indonesian;
}
