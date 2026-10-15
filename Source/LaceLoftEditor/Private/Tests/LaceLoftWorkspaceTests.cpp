#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/LaceLoftPatternWorkspaceWidget.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PlayInEditorDataTypes.h"
#include "Tests/AutomationEditorCommon.h"
#include "Widgets/SWidget.h"
#include "Algo/Reverse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftWorkspaceAssetsTest,"LaceLoft.UI.CreationAssets",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftWorkspaceAssetsTest::RunTest(const FString& Parameters)
{
    for(const TCHAR* Name:{TEXT("WBP_Title"),TEXT("WBP_GarmentCreation"),TEXT("WBP_FittingRoom")})
    {
        auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*FString::Printf(TEXT("/Game/UI/%s.%s"),Name,Name));
        if(!BP || !BP->WidgetTree)continue;
        if(FString(Name)==TEXT("WBP_Title"))
        {
            auto* Choice=Cast<UComboBoxString>(BP->WidgetTree->FindWidget(TEXT("PatternChoice")));
            if(TestNotNull(TEXT("Saved title pattern picker"),Choice))
            {
                TestEqual(TEXT("Both pattern options survive asset reload"),Choice->GetOptionCount(),2);
                Choice->TakeWidget(); // RebuildWidget resolves the serialized default selection.
                TestEqual(TEXT("Title defaults to the first authored pattern"),Choice->GetSelectedOption(),FString(TEXT("반팔 티셔츠")));
            }
        }
        BP->WidgetTree->ForEachWidget([this,Name](UWidget* Widget)
        {
            AddInfo(FString::Printf(TEXT("Designer %s: %s %s parent=%s slot=%s"),Name,*Widget->GetName(),
                *Widget->GetClass()->GetName(),*GetNameSafe(Widget->GetParent()),*GetNameSafe(Widget->Slot)));
        });
    }
    for(const TCHAR* Name:{TEXT("WBP_PatternWorkspace"),TEXT("WBP_PatternPieceEntry"),TEXT("WBP_SewingStepEntry"),TEXT("WBP_ConfirmDialog")})
    {
        auto* BP=LoadObject<UWidgetBlueprint>(nullptr,*FString::Printf(TEXT("/Game/UI/%s.%s"),Name,Name));
        if(!TestNotNull(FString::Printf(TEXT("Saved Designer asset %s"),Name),BP))continue;
        TestNotNull(TEXT("Saved widget tree"),BP->WidgetTree.Get());
        if(BP->WidgetTree)TestNotNull(TEXT("Saved Designer root"),BP->WidgetTree->RootWidget.Get());
        if(FString(Name)==TEXT("WBP_PatternWorkspace") && BP->WidgetTree && BP->WidgetTree->RootWidget)
            TestEqual(TEXT("Workspace background participates in pointer hit testing"),BP->WidgetTree->RootWidget->GetVisibility(),ESlateVisibility::Visible);
    }
    return !HasAnyErrors();
}

namespace
{
class FExerciseCreation final:public IAutomationLatentCommand
{
public:
    explicit FExerciseCreation(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
    bool Update() override
    {
        UWorld* World=GEditor?GEditor->PlayWorld:nullptr;
        TArray<UUserWidget*> Screens;
        if(World)UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Screens,UUserWidget::StaticClass(),true);
        UUserWidget* Screen=Screens.IsEmpty()?nullptr:Screens[0];
        auto* View=Screen?Cast<ULaceLoftPatternWorkspaceWidget>(Screen->GetWidgetFromName(TEXT("Workspace"))):nullptr;
        if(!View)
        {
            if(FPlatformTime::Seconds()-Started>30){Test->AddError(TEXT("Creation WBP did not appear"));return true;}
            return false;
        }
        auto* Session=World->GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
        Session->SetSaveSlotForTesting(TEXT("LaceLoft_UIAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits));Session->LoadSession();
        auto Slate=View->TakeWidget();
        const auto Button=[&](const TCHAR* Name)
        {auto* B=Cast<UButton>(Screen->GetWidgetFromName(Name));if(Test->TestNotNull(Name,B))B->OnClicked.Broadcast();};
        for(int32 Garment=0;Garment<2;++Garment)
        {
            Button(Garment==0?TEXT("NewTopButton"):TEXT("NewBottomButton"));
            auto* Instruction=Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("InstructionLabel")));
            Test->TestTrue(TEXT("New unfinished project never retains completed instructions"),Instruction && !Instruction->GetText().ToString().Contains(TEXT("완성되었습니다")));
            const auto* Project=Session->GetCurrentProject();if(!Project){Test->AddError(TEXT("New project button was not connected"));return true;}
            const FGuid Id=Project->ProjectId;auto* Pattern=Session->GetPattern(Project->PatternId);
            // Exercise actual SObjectWidget -> native UMG input dispatch under two DPI transforms.
            const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(1000,800),FSlateLayoutTransform(Garment==0?1.f:1.5f,FVector2D(17,33)));
            const auto Position=[&](FVector2D P,FVector2D Offset){return Geometry.LocalToAbsolute(FVector2D(28,35)+(P+Offset)*View->PixelsPerMm);};
            const auto Event=[](FVector2D P,bool Down,FKey Key)
            {return FPointerEvent(0,0,P,P,Down?TSet<FKey>{EKeys::LeftMouseButton}:TSet<FKey>{},Key,0,FModifierKeysState());};
            const auto Trace=[&](const FLaceLoftPatternPath& Path,FVector2D Offset,bool LoseCapture=false)
            {
                const auto Start=Position(Path.PointsMm[0],Offset);
                Slate->OnMouseButtonDown(Geometry,Event(Start,true,EKeys::LeftMouseButton));
                for(int32 I=1;I<Path.PointsMm.Num();++I)
                {
                    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Path.PointsMm[I-1],Path.PointsMm[I])));
                    for(int32 J=1;J<=Steps;++J)
                        Slate->OnMouseMove(Geometry,Event(Position(FMath::Lerp(Path.PointsMm[I-1],Path.PointsMm[I],double(J)/Steps),Offset),true,EKeys::Invalid));
                }
                if(LoseCapture)Slate->OnMouseCaptureLost(FCaptureLostEvent(0,0));
                else Slate->OnMouseButtonUp(Geometry,Event(Position(Path.PointsMm.Last(),Offset),false,EKeys::LeftMouseButton));
            };
            for(int32 I=0;I<Pattern->Pieces.Num();++I)
            {
                const auto& Piece=Pattern->Pieces[I];View->SelectPiece(Piece.PieceId);
                Slate->OnMouseMove(Geometry,Event(Position(Piece.CutOutline.PointsMm[0],Piece.LayoutMm),false,EKeys::Invalid));
                Test->TestEqual(TEXT("Hover cannot cut"),Session->GetModel(Id)->Progress(Piece.CutOutline.PathId).CoveredMm(),0.);
                Trace(Piece.CutOutline,Piece.LayoutMm,I==0);
                Test->TestTrue(TEXT("UMG pointer trace cuts the selected piece"),Session->GetModel(Id)->IsPieceCut(Piece.PieceId));
            }
            for(int32 Pass=0;Pass<Pattern->Seams.Num();++Pass)for(const auto& Seam:Pattern->Seams)
            {
                if(!Session->GetModel(Id)->CanStartSeam(Seam.SeamId))continue;
                View->SelectSeam(Seam.SeamId);
                for(const auto& Ref:Seam.SideA)
                {
                    const auto* Piece=Pattern->FindPiece(Ref.PieceId);auto Path=Piece->FindEdge(Ref.EdgeId)->Path;
                    if(Ref.Direction<0)Algo::Reverse(Path.PointsMm);Trace(Path,Piece->LayoutMm);
                }
            }
            Test->TestTrue(TEXT("UMG pointer traces complete all guided seams"),Session->CanEquip(Id));
            Button(TEXT("UndoButton"));Test->TestFalse(TEXT("UI Undo revokes completion"),Session->CanEquip(Id));
            Test->TestFalse(TEXT("UI Undo also revokes the completed instruction"),Instruction->GetText().ToString().Contains(TEXT("완성되었습니다")));
            Button(TEXT("RedoButton"));Test->TestTrue(TEXT("UI Redo restores completion"),Session->CanEquip(Id));
        }
        Button(TEXT("SaveButton"));Session->LoadSession();
        Test->TestEqual(TEXT("Both UI-created projects restore"),Session->GetProjects().Num(),2);
        for(const auto& Project:Session->GetProjects())Test->TestTrue(TEXT("Restored UI result remains qualified"),Session->CanEquip(Project.ProjectId));
        auto* Cheat=Cast<UButton>(Screen->GetWidgetFromName(TEXT("CompleteCheatButton")));
        if(Test->TestNotNull(TEXT("Authored instant completion button"),Cheat))
        {
            const FGuid Untouched=Session->CreateProject(TEXT("TShirt"));
            for(FName PatternId:{FName(TEXT("TShirt")),FName(TEXT("DolphinShorts"))})
            {
                const FGuid Target=Session->CreateProject(PatternId);
                Cheat->OnClicked.Broadcast();
                Test->TestTrue(TEXT("Cheat completes the selected garment"),Session->CanEquip(Target));
                Test->TestFalse(TEXT("Cheat leaves another project unfinished"),Session->CanEquip(Untouched));
                Test->TestEqual(TEXT("Cheat is a single undo command"),Session->GetProject(Target)->UndoCommands.Num(),1);
                Test->TestFalse(TEXT("Completed project disables cheat"),Cheat->GetIsEnabled());
                Session->LoadSession();Test->TestTrue(TEXT("Cheat completion persists"),Session->CanEquip(Target));
                Session->Equip(Target);Button(TEXT("UndoButton"));
                Test->TestFalse(TEXT("One Undo revokes cheat completion"),Session->CanEquip(Target));
                Test->TestFalse(TEXT("Undo removes cheated equipment"),Session->GetEquipped(Session->GetPattern(PatternId)->Slot).IsValid());
                Test->TestTrue(TEXT("Undo enables cheat again"),Cheat->GetIsEnabled());
                Button(TEXT("RedoButton"));Test->TestTrue(TEXT("Redo restores cheat completion"),Session->CanEquip(Target));
            }
        }
        return true;
    }
private:FAutomationTestBase* Test;double Started;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftCreationInputTest,"LaceLoft.UI.CreationInputPIE",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftCreationInputTest::RunTest(const FString& Parameters)
{
    FRequestPlaySessionParams Request;Request.GlobalMapOverride=TEXT("/Game/Maps/L_GarmentCreation");Request.bAllowOnlineSubsystem=false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Request));
    ADD_LATENT_AUTOMATION_COMMAND(FExerciseCreation(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
