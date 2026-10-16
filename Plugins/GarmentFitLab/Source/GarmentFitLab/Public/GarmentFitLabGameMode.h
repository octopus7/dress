#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GarmentFitLabGameMode.generated.h"

/** Used only by the lab map; leaves the project's game flow untouched. */
UCLASS()
class GARMENTFITLAB_API AGarmentFitLabGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AGarmentFitLabGameMode();
};
