#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TitleScreenWidget.generated.h"

class UButton;

// Authoring model and reasoning effort: not exposed to this session; not inferred.
// The entire widget hierarchy is saved in the WBP asset and editable in UMG Designer.
UCLASS(Abstract)
class GARMENTLAB_API UTitleScreenWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> ExitButton;

private:
    UFUNCTION()
    void HandleExit();
};
