#include "GarmentFitInputWidget.h"
#include "GarmentFitPreviewActor.h"
#include "Components/TextBlock.h"

void UGarmentFitInputWidget::SetPreviewActor(AGarmentFitPreviewActor* InActor)
{
    PreviewActor = InActor;
    RefreshDisplay();
}

void UGarmentFitInputWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshDisplay();
}

void UGarmentFitInputWidget::RefreshDisplay()
{
    if (const AGarmentFitPreviewActor* Actor = PreviewActor.Get())
    {
        if (InputStatusText)
        {
            InputStatusText->SetText(Actor->GetInputMessage());
        }
        if (InputAssetText)
        {
            InputAssetText->SetText(Actor->InputMesh.IsNull()
                ? NSLOCTEXT("GarmentFitLab", "NoAsset", "입력 애셋: 없음")
                : FText::FromString(Actor->InputMesh.ToSoftObjectPath().ToString()));
        }
    }
}
