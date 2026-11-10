// Copyright Epic Games, Inc. All Rights Reserved.

#include "VR/MuseumVRGameMode.h"

#include "GameFramework/PlayerController.h"
#include "VR/MuseumVRComponent.h"

// ====== Lifecycle ====== //

void AMuseumVRGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer || !NewPlayer->IsLocalController() || !VRComponentClass) return;

	// VR layer on the player (it stays idle without a headset)
	UMuseumVRComponent* VR = NewObject<UMuseumVRComponent>(NewPlayer, VRComponentClass, TEXT("MuseumVR"));
	VR->RegisterComponent();
}
