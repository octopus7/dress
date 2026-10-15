#pragma once
#include "LaceLoftNavigationWidget.h"
#include "Components/ComboBoxString.h"
#include "LaceLoftFittingWidget.generated.h"
class UButton;class UTextBlock;class UScrollBox;class UBorder;class USlider;
class ULaceLoftWardrobeSubsystem;class ULaceLoftFittingController;class ULaceLoftSelectionEntryWidget;

UCLASS(Abstract)
class LACELOFT_API ULaceLoftPoseControlsWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> NeutralButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RaisedButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ForwardButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SittingButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ResetJointsButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> JointChoice;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> PitchSlider;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> YawSlider;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> RollSlider;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JointValueLabel;
private:
    UFUNCTION() void Neutral();UFUNCTION() void Raised();UFUNCTION() void Forward();UFUNCTION() void Sitting();
    UFUNCTION() void ResetJoints();
    UFUNCTION() void JointSelected(FString Selected,ESelectInfo::Type Type);
    UFUNCTION() void JointChanged(float Value);
    UFUNCTION() void CommitJoint();
    void Refresh();
    TWeakObjectPtr<ULaceLoftWardrobeSubsystem> Session;
    bool bRefreshing=false;
};

UCLASS(Abstract)
class LACELOFT_API ULaceLoftFittingWidget : public ULaceLoftNavigationWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseWheel(const FGeometry&,const FPointerEvent&) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> GarmentList;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RemoveTopButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RemoveBottomButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ResetCameraButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> WireframeButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> WireframeButtonCaption;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> FittingStatus;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> CameraInput;
    UPROPERTY(EditDefaultsOnly,Category="Entries") TSubclassOf<ULaceLoftSelectionEntryWidget> GarmentEntryClass;
private:
    UFUNCTION() void RemoveTop();UFUNCTION() void RemoveBottom();UFUNCTION() void ResetCamera();
    UFUNCTION() void ToggleWireframe();
    void Equip(FName,FGuid Id);
    void Refresh();
    UPROPERTY() TObjectPtr<ULaceLoftFittingController> Fitting;
    TWeakObjectPtr<ULaceLoftWardrobeSubsystem> Session;
    FVector2D LastPointer;bool bOrbiting=false;
};
