#pragma once
#include "Blueprint/UserWidget.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "LaceLoftPatternWorkspaceWidget.generated.h"
class ULaceLoftWardrobeSubsystem;

/** Data drawing and pointer input only. Its frame and layout live in the saved WBP. */
UCLASS(Abstract)
class LACELOFT_API ULaceLoftPatternWorkspaceWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Pattern Preview") TSoftObjectPtr<ULaceLoftPatternDefinition> PreviewPattern;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Pattern Style") float PixelsPerMm=5;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Pattern Style") FLinearColor CutColor=FLinearColor(.18f,.29f,.28f,1);
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Pattern Style") FLinearColor ProgressColor=FLinearColor(.1f,.65f,.40f,1);
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Pattern Style") FLinearColor ActiveColor=FLinearColor(.86f,.36f,.15f,1);
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Pattern Style") FLinearColor TargetColor=FLinearColor(.3f,.4f,.85f,.25f);
    void SelectPiece(FName Id);
    void SelectSeam(FName Id);
    void ResetView();
    void RefreshView() {Invalidate(EInvalidateWidgetReason::Paint);}
    FName GetSelectedPiece() const {return SelectedPiece;}
    FName GetSelectedSeam() const {return SelectedSeam;}
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual int32 NativePaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply NativeOnMouseWheel(const FGeometry&,const FPointerEvent&) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;
private:
    ULaceLoftPatternDefinition* Pattern() const;
    FVector2D PieceOffset(const FLaceLoftPatternPiece&) const;
    FVector2D ToMm(const FGeometry&,FVector2D Screen) const;
    void FinishInput();
    TWeakObjectPtr<ULaceLoftWardrobeSubsystem> Session;
    FName SelectedPiece,SelectedSeam,InputPiece;
    FVector2D Pan=FVector2D(28,35),LastPointer,DragStartMm,DragOriginal,DragOffset;
    float Zoom=1;
    bool bPanning=false,bDrawing=false,bDragging=false;
};

