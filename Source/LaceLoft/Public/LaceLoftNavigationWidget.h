#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LaceLoftNavigationWidget.generated.h"

class UButton;

/** Navigation behavior for designer-authored Widget Blueprints. */
UCLASS(Abstract)
class LACELOFT_API ULaceLoftNavigationWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void FocusInitialButton();

protected:
    void TravelToLevel(FName LevelPackage);
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Navigation")
    TObjectPtr<UButton> TitleButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Navigation")
    TObjectPtr<UButton> CreationButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Navigation")
    TObjectPtr<UButton> FittingButton;

private:
    UFUNCTION()
    void HandleTitleClicked();

    UFUNCTION()
    void HandleCreationClicked();

    UFUNCTION()
    void HandleFittingClicked();

    void SetNavigationEnabled(bool bEnabled);

    bool bTravelPending = false;
};
