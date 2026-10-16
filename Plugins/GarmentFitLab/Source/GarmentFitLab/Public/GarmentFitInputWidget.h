#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GarmentFitInputWidget.generated.h"

class AGarmentFitPreviewActor;
class UTextBlock;

/** Display bindings only. Layout and style belong to WBP_GarmentFitLab. */
UCLASS(Abstract)
class GARMENTFITLAB_API UGarmentFitInputWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPreviewActor(AGarmentFitPreviewActor* InActor);
    void RefreshDisplay();

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> InputStatusText;

    UPROPERTY(meta=(BindWidget))
    TObjectPtr<UTextBlock> InputAssetText;

private:
    TWeakObjectPtr<AGarmentFitPreviewActor> PreviewActor;
};
