#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TitlePlayerController.generated.h"

class UTitleScreenWidget;

UCLASS()
class GARMENTLAB_API ATitlePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ATitlePlayerController();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, Category = "Title Screen")
    TSubclassOf<UTitleScreenWidget> TitleScreenClass;

    UPROPERTY(Transient)
    TObjectPtr<UTitleScreenWidget> TitleScreen;
};
