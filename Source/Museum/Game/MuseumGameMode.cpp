// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/MuseumGameMode.h"

#include "Player/MuseumCharacter.h"
#include "Player/MuseumPlayerController.h"
#include "UI/MuseumHUD.h"

AMuseumGameMode::AMuseumGameMode()
{
	DefaultPawnClass = AMuseumCharacter::StaticClass();
	PlayerControllerClass = AMuseumPlayerController::StaticClass();
	HUDClass = AMuseumHUD::StaticClass();
}
