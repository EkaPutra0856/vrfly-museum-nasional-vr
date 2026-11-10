// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MuseumGameMode.generated.h"

/** Museum rules: first-person visitor pawn, controller and HUD (the HUD's overlay widget is set on a Blueprint child of the HUD). */
UCLASS(Blueprintable)
class AMuseumGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMuseumGameMode();
};
