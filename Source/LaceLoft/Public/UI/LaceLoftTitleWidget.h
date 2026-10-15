#pragma once
#include "LaceLoftNavigationWidget.h"
#include "LaceLoftTitleWidget.generated.h"
class UComboBoxString;class UTextBlock;
UCLASS(Abstract)
class LACELOFT_API ULaceLoftTitleWidget : public ULaceLoftNavigationWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ContinueButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> PatternChoice;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TitleStatus;
private:
    UFUNCTION() void NewProject();
    UFUNCTION() void ContinueProject();
    void Refresh();
};
