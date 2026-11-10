// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/MuseumGameMode.h"
#include "MuseumVRGameMode.generated.h"

class UMuseumVRComponent;

/** The Museum's rules plus VR: adds the VR component to every local player. Pawn, controller and HUD classes stay the Museum's. */
UCLASS(Blueprintable)
class AMuseumVRGameMode : public AMuseumGameMode
{
	GENERATED_BODY()

public:
	// ====== Lifecycle ====== //

	virtual void PostLogin(APlayerController* NewPlayer) override;

	// ====== VR ====== //

protected:
	/** The VR component added to each local player controller (a Blueprint child sets its assets). */
	UPROPERTY(EditDefaultsOnly, Category = "Museum|VR")
	TSubclassOf<UMuseumVRComponent> VRComponentClass;
};
