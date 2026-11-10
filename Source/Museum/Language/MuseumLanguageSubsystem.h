#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MuseumLanguageSubsystem.generated.h"

class USoundBase;

/** The two tour languages. Indonesian is the default (owner decision). */
UENUM(BlueprintType)
enum class EMuseumLanguage : uint8
{
	Indonesian,
	English,
};

/** One text in both tour languages. */
USTRUCT(BlueprintType)
struct FMuseumText
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum", meta = (MultiLine = "true"))
	FText Indonesian;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum", meta = (MultiLine = "true"))
	FText English;

	/** The text in the language; an empty English falls back to the Indonesian. */
	const FText& Get(EMuseumLanguage Language) const;
};

/** One voice-over clip in both tour languages. */
USTRUCT(BlueprintType)
struct FMuseumSound
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum")
	TSoftObjectPtr<USoundBase> Indonesian;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Museum")
	TSoftObjectPtr<USoundBase> English;

	/** The clip in the language, loaded on demand; a missing English falls back to the Indonesian. */
	USoundBase* Load(EMuseumLanguage Language) const;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMuseumLanguageChanged, EMuseumLanguage /*Language*/);

/** The visitor's tour language: every panel, card and voice-over reads it, and refreshes when it changes. */
UCLASS()
class UMuseumLanguageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ====== Events ====== //

	FOnMuseumLanguageChanged OnLanguageChanged;

public:
	// ====== API ====== //

	UFUNCTION(BlueprintPure, Category = "Museum|Language")
	EMuseumLanguage GetLanguage() const { return Language; }

	UFUNCTION(BlueprintCallable, Category = "Museum|Language")
	void SetLanguage(EMuseumLanguage InLanguage);

	/** Indonesian to English and back. */
	UFUNCTION(BlueprintCallable, Category = "Museum|Language")
	void ToggleLanguage();

	/** "ID" or "EN", for the toggle buttons. */
	UFUNCTION(BlueprintPure, Category = "Museum|Language")
	static FText GetLanguageCode(EMuseumLanguage InLanguage);

	/** The subsystem of the world's game instance (null outside a game world). */
	static UMuseumLanguageSubsystem* Get(const UObject* WorldContext);

	/** The current language, Indonesian when there is no game instance (editor previews). */
	static EMuseumLanguage GetCurrent(const UObject* WorldContext);

private:
	// ====== State ====== //

	EMuseumLanguage Language = EMuseumLanguage::Indonesian;
};
