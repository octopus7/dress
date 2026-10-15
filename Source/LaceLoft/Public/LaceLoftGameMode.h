#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LaceLoftGameMode.generated.h"

UCLASS()
class LACELOFT_API ALaceLoftGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALaceLoftGameMode();

protected:
    virtual void InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer) override;
};
