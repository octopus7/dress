#pragma once
#include "LaceLoftNavigationWidget.h"
#include "LaceLoftCreationWidget.generated.h"
class UButton;class UTextBlock;class UScrollBox;class ULaceLoftWardrobeSubsystem;class ULaceLoftPatternWorkspaceWidget;

UCLASS(Abstract)
class LACELOFT_API ULaceLoftSelectionEntryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(FText Caption,FName InId,FGuid InProject,bool Enabled);
    DECLARE_DELEGATE_TwoParams(FSelected,FName,FGuid);
    FSelected OnSelected;
protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SelectButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Label;
private:
    UFUNCTION() void Click();
    FName Id;FGuid Project;
};

UCLASS(Abstract)
class LACELOFT_API ULaceLoftCreationWidget : public ULaceLoftNavigationWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<ULaceLoftPatternWorkspaceWidget> Workspace;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> NewTopButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> NewBottomButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> UndoButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RedoButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SaveButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ResetViewButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CompleteCheatButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> InstructionLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> ProjectList;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> PieceList;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> SeamList;
    UPROPERTY(EditDefaultsOnly,Category="Entries") TSubclassOf<ULaceLoftSelectionEntryWidget> PieceEntryClass;
    UPROPERTY(EditDefaultsOnly,Category="Entries") TSubclassOf<ULaceLoftSelectionEntryWidget> SeamEntryClass;
private:
    UFUNCTION() void NewTop();
    UFUNCTION() void NewBottom();
    UFUNCTION() void Undo();
    UFUNCTION() void Redo();
    UFUNCTION() void Save();
    UFUNCTION() void ResetView();
    UFUNCTION() void CompleteCheat();
    void Refresh();
    void SelectProject(FName,FGuid Id);
    void SelectPiece(FName Id,FGuid);
    void SelectSeam(FName Id,FGuid);
    TWeakObjectPtr<ULaceLoftWardrobeSubsystem> Session;
};
