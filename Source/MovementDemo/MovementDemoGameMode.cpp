// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovementDemoGameMode.h"
#include "MovementDemoCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMovementDemoGameMode::AMovementDemoGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
