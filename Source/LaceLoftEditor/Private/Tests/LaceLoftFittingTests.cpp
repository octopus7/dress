#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/PoseAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Fitting/LaceLoftDummyActor.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Tests/AutomationEditorCommon.h"
#include "Algo/Reverse.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftFittingAssetTest,"LaceLoft.Fitting.PoseAssets",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftFittingAssetTest::RunTest(const FString& Parameters)
{
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Dummy/Animation/ABP_Dummy.ABP_Dummy"));
    auto* Poses=LoadObject<UPoseAsset>(nullptr,TEXT("/Game/Characters/Dummy/Animation/PA_DummyPoses.PA_DummyPoses"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if(TestNotNull(TEXT("Saved animation Blueprint"),BP))
    {TestEqual(TEXT("ABP shares dummy skeleton"),BP->TargetSkeleton.Get(),Mesh->GetSkeleton());TestNotNull(TEXT("ABP compiles to a class"),BP->GeneratedClass.Get());}
    if(TestNotNull(TEXT("Four saved named poses"),Poses))
    {TestEqual(TEXT("Pose skeleton matches"),Poses->GetSkeleton(),Mesh->GetSkeleton());TestEqual(TEXT("Four pose presets"),Poses->GetNumPoses(),4);}
    return !HasAnyErrors();
}

namespace
{
bool FinishFittingProject(ULaceLoftWardrobeSubsystem& Session,FGuid Id)
{
    Session.SelectProject(Id);auto* Pattern=Session.GetPattern(Session.GetProject(Id)->PatternId);
    const auto Trace=[&](FLaceLoftPatternPath Path)
    {
        for(int32 I=1;I<Path.PointsMm.Num();++I)
        {
            const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Path.PointsMm[I-1],Path.PointsMm[I])));
            for(int32 J=1;J<=Steps;++J)Session.UpdateStroke(FMath::Lerp(Path.PointsMm[I-1],Path.PointsMm[I],double(J)/Steps));
        }
        Session.EndStroke();
    };
    for(const auto& Piece:Pattern->Pieces)
    {if(!Session.BeginCut(Piece.PieceId,Piece.CutOutline.PointsMm[0]))return false;Trace(Piece.CutOutline);}
    for(int32 Pass=0;Pass<Pattern->Seams.Num();++Pass)for(const auto& Seam:Pattern->Seams)
    {
        if(!Session.GetModel(Id)->CanStartSeam(Seam.SeamId))continue;
        for(int32 I=0;I<Seam.SideA.Num();++I)
        {
            const auto& Ref=Seam.SideA[I];auto Path=Pattern->FindPiece(Ref.PieceId)->FindEdge(Ref.EdgeId)->Path;
            if(Ref.Direction<0)Algo::Reverse(Path.PointsMm);
            if(!Session.BeginSeam(Seam.SeamId,I,Path.PointsMm[0]))return false;Trace(Path);
        }
    }
    return Session.CanEquip(Id);
}
class FExerciseFitting final:public IAutomationLatentCommand
{
public:
    explicit FExerciseFitting(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()),StepTime(Started){}
    bool Update() override
    {
        UWorld* World=GEditor?GEditor->PlayWorld:nullptr;ALaceLoftDummyActor* Dummy=nullptr;
        if(World)for(TActorIterator<ALaceLoftDummyActor> It(World);It;++It){Dummy=*It;break;}
        if(!Dummy || !World->HasBegunPlay())
        {if(FPlatformTime::Seconds()-Started>30){Test->AddError(TEXT("Saved fitting scene did not start"));return true;}return false;}
        if(FPlatformTime::Seconds()-StepTime<.5)return false;
        auto* Session=World->GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
        switch(Step)
        {
        case 0:
        {
            Session->SetSaveSlotForTesting(TEXT("LaceLoft_FittingAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits));Session->LoadSession();
            TopId=Session->CreateProject(TEXT("TShirt"));BottomId=Session->CreateProject(TEXT("DolphinShorts"));
            Test->TestFalse(TEXT("Unfinished top stays unequipped"),Session->Equip(TopId));
            Test->TestTrue(TEXT("Finish top for fitting"),FinishFittingProject(*Session,TopId));
            Test->TestTrue(TEXT("Finish bottom for fitting"),FinishFittingProject(*Session,BottomId));
            Session->Equip(TopId);Session->Equip(BottomId);
            Test->TestNotNull(TEXT("Top cloth loaded"),Dummy->Top->GetAsset());Test->TestNotNull(TEXT("Bottom cloth loaded"),Dummy->Bottom->GetAsset());
            Test->TestTrue(TEXT("Two independent cloth simulations enabled"),Dummy->Top->IsSimulationEnabled() && Dummy->Bottom->IsSimulationEnabled());
            Test->TestTrue(TEXT("Top follows posed body"),Dummy->Top->LeaderPoseComponent.Get()==Dummy->Body.Get());
            Test->TestTrue(TEXT("Bottom follows posed body"),Dummy->Bottom->LeaderPoseComponent.Get()==Dummy->Body.Get());
            TArray<UUserWidget*> Screens;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Screens,UUserWidget::StaticClass(),true);
            auto* Toggle=Screens.Num()?Cast<UButton>(Screens[0]->GetWidgetFromName(TEXT("WireframeButton"))):nullptr;
            if(Test->TestNotNull(TEXT("Authored wireframe toggle"),Toggle))
            {
                auto* Base=Dummy->Body->GetMaterial(0);auto* TopBase=Dummy->Top->GetMaterial(0);auto* BottomBase=Dummy->Bottom->GetMaterial(0);
                Toggle->OnClicked.Broadcast();
                Test->TestNotNull(TEXT("Body gains overlay"),Dummy->Body->GetOverlayMaterial());
                Test->TestNotNull(TEXT("Simulated top gains overlay"),Dummy->Top->GetOverlayMaterial());
                Test->TestNotNull(TEXT("Simulated bottom gains overlay"),Dummy->Bottom->GetOverlayMaterial());
                Test->TestTrue(TEXT("Original shaded materials remain"),Base==Dummy->Body->GetMaterial(0) && TopBase==Dummy->Top->GetMaterial(0) && BottomBase==Dummy->Bottom->GetMaterial(0));
                Test->TestTrue(TEXT("Wireframe keeps cloth simulating"),Dummy->Top->IsSimulationEnabled() && Dummy->Bottom->IsSimulationEnabled());
                Toggle->OnClicked.Broadcast();
                Test->TestNull(TEXT("Body overlay clears"),Dummy->Body->GetOverlayMaterial());
                Test->TestNull(TEXT("Top overlay clears"),Dummy->Top->GetOverlayMaterial());
                Test->TestNull(TEXT("Bottom overlay clears"),Dummy->Bottom->GetOverlayMaterial());
            }
            NeutralHand=Dummy->Body->GetBoneLocation(TEXT("hand_l"));NeutralFoot=Dummy->Body->GetBoneLocation(TEXT("foot_l"));
            Session->SetPose(TEXT("ArmsRaised"));break;
        }
        case 1:
            Test->TestTrue(TEXT("Raised preset moves the actual hand upward"),Dummy->Body->GetBoneLocation(TEXT("hand_l")).Z>NeutralHand.Z+1);
            Session->SetPose(TEXT("ArmsForward"));break;
        case 2:
            Test->TestTrue(TEXT("Forward preset moves the actual hand forward"),Dummy->Body->GetBoneLocation(TEXT("hand_l")).X>NeutralHand.X+1);
            Session->SetPose(TEXT("Sitting"));break;
        case 3:
            Test->TestTrue(TEXT("Sitting lowers root"),Dummy->Body->GetBoneLocation(TEXT("root")).Z<-2);
            Test->TestTrue(TEXT("Sitting moves foot forward"),Dummy->Body->GetBoneLocation(TEXT("foot_l")).X>NeutralFoot.X+1);
            HeadBefore=Dummy->Body->GetBoneQuaternion(TEXT("head"));Session->SetJointRotation(TEXT("head"),FRotator(20,30,0));break;
        case 4:
            Test->TestFalse(TEXT("Manual joint rotation changes actual head pose"),Dummy->Body->GetBoneQuaternion(TEXT("head")).Equals(HeadBefore,.01));
            Session->ResetJoints();break;
        case 5:
        {
            Test->TestTrue(TEXT("Joint reset restores preset head"),Dummy->Body->GetBoneQuaternion(TEXT("head")).Equals(HeadBefore,.01));
            const FVector Camera=Dummy->Camera->GetRelativeLocation();Dummy->Orbit(FVector2D(100,30));
            Test->TestFalse(TEXT("Orbit moves camera"),Camera.Equals(Dummy->Camera->GetRelativeLocation()));Dummy->ResetCamera();
            Test->TestTrue(TEXT("Camera reset is deterministic"),Camera.Equals(Dummy->Camera->GetRelativeLocation(),.001));
            Session->SelectProject(TopId);Session->Undo();
            Test->TestNull(TEXT("Undo removes stale top component asset"),Dummy->Top->GetAsset());
            Test->TestNotNull(TEXT("Undo top preserves bottom"),Dummy->Bottom->GetAsset());Session->Unequip(ELaceLoftGarmentSlot::Bottom);
            Test->TestNull(TEXT("Explicit removal clears bottom component asset"),Dummy->Bottom->GetAsset());return true;
        }
        }
        ++Step;StepTime=FPlatformTime::Seconds();return false;
    }
private:FAutomationTestBase* Test;double Started,StepTime;int32 Step=0;FGuid TopId,BottomId;FVector NeutralHand,NeutralFoot;FQuat HeadBefore;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftFittingRuntimeTest,"LaceLoft.Fitting.PoseAndEquipmentPIE",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftFittingRuntimeTest::RunTest(const FString& Parameters)
{
    FRequestPlaySessionParams Request;Request.GlobalMapOverride=TEXT("/Game/Maps/L_FittingRoom");Request.bAllowOnlineSubsystem=false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Request));ADD_LATENT_AUTOMATION_COMMAND(FExerciseFitting(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif

