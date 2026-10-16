#include "GarmentFitLabGameMode.h"
#include "GameFramework/SpectatorPawn.h"

AGarmentFitLabGameMode::AGarmentFitLabGameMode()
{
    DefaultPawnClass = ASpectatorPawn::StaticClass();
}
