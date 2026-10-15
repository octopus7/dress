#include "LaceLoftGameMode.h"

#include "LaceLoftPlayerController.h"

ALaceLoftGameMode::ALaceLoftGameMode()
{
    PlayerControllerClass = ALaceLoftPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    SpectatorClass = nullptr;
    HUDClass = nullptr;
    bStartPlayersAsSpectators = true;
}

void ALaceLoftGameMode::InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer)
{
    // Screens are authored UMG assets owned by the player controller; no HUD actor is needed.
}
