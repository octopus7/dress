#include "UI/LaceLoftTitleWidget.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
void ULaceLoftTitleWidget::NativeConstruct()
{
    Super::NativeConstruct();CreationButton->OnClicked.RemoveAll(this);CreationButton->OnClicked.AddUniqueDynamic(this,&ThisClass::NewProject);
    ContinueButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ContinueProject);
    auto* Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
    Session->OnChanged.AddUObject(this,&ThisClass::Refresh);Refresh();
}
void ULaceLoftTitleWidget::NativeDestruct()
{if(GetGameInstance())GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>()->OnChanged.RemoveAll(this);Super::NativeDestruct();}
void ULaceLoftTitleWidget::Refresh()
{
    auto* Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();ContinueButton->SetIsEnabled(Session->GetCurrentProject()!=nullptr);
    TitleStatus->SetText(Session->GetStatusMessage().IsEmpty()?FText::FromString(TEXT("옷본을 고르고 재단과 봉제를 시작하세요.")):Session->GetStatusMessage());
}
void ULaceLoftTitleWidget::NewProject()
{
    auto* Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
    if(Session->CreateProject(PatternChoice->GetSelectedIndex()==1?TEXT("DolphinShorts"):TEXT("TShirt")).IsValid())TravelToLevel(TEXT("/Game/Maps/L_GarmentCreation"));
}
void ULaceLoftTitleWidget::ContinueProject(){TravelToLevel(TEXT("/Game/Maps/L_GarmentCreation"));}
