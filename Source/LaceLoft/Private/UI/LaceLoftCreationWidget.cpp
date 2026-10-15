#include "UI/LaceLoftCreationWidget.h"
#include "UI/LaceLoftPatternWorkspaceWidget.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"

void ULaceLoftSelectionEntryWidget::NativeConstruct()
{Super::NativeConstruct();SelectButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Click);}
void ULaceLoftSelectionEntryWidget::Configure(FText Caption,FName InId,FGuid InProject,bool Enabled)
{Id=InId;Project=InProject;Label->SetText(Caption);SelectButton->SetIsEnabled(Enabled);}
void ULaceLoftSelectionEntryWidget::Click(){OnSelected.ExecuteIfBound(Id,Project);}

void ULaceLoftCreationWidget::NativeConstruct()
{
    Super::NativeConstruct();Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
    NewTopButton->OnClicked.AddUniqueDynamic(this,&ThisClass::NewTop);
    NewBottomButton->OnClicked.AddUniqueDynamic(this,&ThisClass::NewBottom);
    UndoButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Undo);
    RedoButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Redo);
    SaveButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Save);
    ResetViewButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ResetView);
    CompleteCheatButton->OnClicked.AddUniqueDynamic(this,&ThisClass::CompleteCheat);
    Session->OnChanged.AddUObject(this,&ThisClass::Refresh);Refresh();
}
void ULaceLoftCreationWidget::NativeDestruct()
{if(Session.IsValid())Session->OnChanged.RemoveAll(this);Super::NativeDestruct();}
void ULaceLoftCreationWidget::NewTop(){Session->CreateProject(TEXT("TShirt"));Workspace->SelectPiece(NAME_None);Workspace->ResetView();Refresh();}
void ULaceLoftCreationWidget::NewBottom(){Session->CreateProject(TEXT("DolphinShorts"));Workspace->SelectPiece(NAME_None);Workspace->ResetView();Refresh();}
void ULaceLoftCreationWidget::Undo(){Session->Undo();}
void ULaceLoftCreationWidget::Redo(){Session->Redo();}
void ULaceLoftCreationWidget::Save(){Session->EndStroke();Session->SaveSession();Refresh();}
void ULaceLoftCreationWidget::ResetView(){Workspace->ResetView();}
void ULaceLoftCreationWidget::CompleteCheat(){Session->CompleteCurrentProjectCheat();}
void ULaceLoftCreationWidget::SelectProject(FName,FGuid Id)
{Session->SelectProject(Id);Workspace->SelectPiece(NAME_None);Workspace->ResetView();Refresh();}
void ULaceLoftCreationWidget::SelectPiece(FName Id,FGuid)
{Workspace->SelectPiece(Id);InstructionLabel->SetText(FText::FromString(TEXT("재단선 위에서 누른 채 따라 그으세요. 재단한 조각은 드래그해 정리할 수 있습니다.")));}
void ULaceLoftCreationWidget::SelectSeam(FName Id,FGuid)
{Workspace->SelectSeam(Id);InstructionLabel->SetText(FText::FromString(TEXT("주황 선을 화살표 방향으로 봉제하세요. 파란 선은 맞물리는 짝입니다. 연결선과 목표 윤곽으로 조립 위치를 확인하세요.")));}
void ULaceLoftCreationWidget::Refresh()
{
    if(!Session.IsValid())return;
    ProjectList->ClearChildren();PieceList->ClearChildren();SeamList->ClearChildren();
    StatusLabel->SetText(Session->GetStatusMessage());
    const auto* Current=Session->GetCurrentProject();
    const auto* CurrentModel=Current?Session->GetModel(Current->ProjectId):nullptr;
    CompleteCheatButton->SetIsEnabled(CurrentModel && !CurrentModel->IsCompleted());
    UndoButton->SetIsEnabled(Current && !Current->UndoCommands.IsEmpty());RedoButton->SetIsEnabled(Current && !Current->RedoCommands.IsEmpty());
    // Only instances of authored entry classes are added; their hierarchy is serialized in WBP.
    for(const auto& Project:Session->GetProjects())
    {
        if(!PieceEntryClass)continue;
        auto* Entry=CreateWidget<ULaceLoftSelectionEntryWidget>(GetOwningPlayer(),PieceEntryClass);ProjectList->AddChild(Entry);
        auto* Pattern=Session->GetPattern(Project.PatternId);
        const FString Caption=(Current && Current->ProjectId==Project.ProjectId?TEXT("• "):TEXT(""))+Pattern->DisplayName.ToString()+TEXT("  ")+Project.ProjectId.ToString(EGuidFormats::Digits).Left(4);
        Entry->Configure(FText::FromString(Caption),NAME_None,Project.ProjectId,true);Entry->OnSelected.BindUObject(this,&ThisClass::SelectProject);
    }
    if(!Current){InstructionLabel->SetText(FText::FromString(TEXT("새 티셔츠 또는 새 돌핀쇼츠를 선택해 시작하세요.")));return;}
    auto* Pattern=Session->GetPattern(Current->PatternId);auto* Model=Session->GetModel(Current->ProjectId);
    for(const auto& Piece:Pattern->Pieces)
    {
        if(!PieceEntryClass)continue;
        auto* Entry=CreateWidget<ULaceLoftSelectionEntryWidget>(GetOwningPlayer(),PieceEntryClass);PieceList->AddChild(Entry);
        const double Percent=100*Model->Progress(Piece.CutOutline.PathId).CoveredMm()/Piece.CutOutline.LengthMm();
        Entry->Configure(FText::FromString(FString::Printf(TEXT("%s  %.0f%%"),*Piece.DisplayName.ToString(),Percent)),Piece.PieceId,{},true);
        Entry->OnSelected.BindUObject(this,&ThisClass::SelectPiece);
    }
    for(const auto& Seam:Pattern->Seams)
    {
        if(!SeamEntryClass)continue;
        auto* Entry=CreateWidget<ULaceLoftSelectionEntryWidget>(GetOwningPlayer(),SeamEntryClass);SeamList->AddChild(Entry);
        const bool Complete=Model->IsSeamComplete(Seam.SeamId),Ready=Model->CanStartSeam(Seam.SeamId);
        const FString Caption=(Complete?TEXT("✓ "):Ready?TEXT("→ "):TEXT("잠김 · "))+Seam.DisplayName.ToString();
        Entry->Configure(FText::FromString(Caption),Seam.SeamId,{},Ready || Complete);Entry->OnSelected.BindUObject(this,&ThisClass::SelectSeam);
    }
    if(Model->IsCompleted())InstructionLabel->SetText(FText::FromString(TEXT("의상이 완성되었습니다. 입어보기에서 장착할 수 있습니다. Undo하면 완성 자격과 착장이 해제됩니다.")));
    else if(!Workspace->GetSelectedSeam().IsNone())
        InstructionLabel->SetText(FText::FromString(TEXT("주황 선을 화살표 방향으로 봉제하세요. 파란 선은 맞물리는 짝입니다. 연결선과 목표 윤곽으로 조립 위치를 확인하세요.")));
    else if(!Workspace->GetSelectedPiece().IsNone())
        InstructionLabel->SetText(FText::FromString(TEXT("재단선 위에서 누른 채 따라 그으세요. 재단한 조각은 드래그해 정리할 수 있습니다.")));
    else
        InstructionLabel->SetText(FText::FromString(TEXT("조각을 선택해 재단하세요. 휠: 확대 · 오른쪽/가운데 버튼: 작업대 이동 · 녹색: 완료 구간")));
    Workspace->RefreshView();
}
