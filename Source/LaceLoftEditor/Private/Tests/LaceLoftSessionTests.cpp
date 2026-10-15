#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Fitting/LaceLoftJointLimits.h"

namespace
{
ULaceLoftWardrobeSubsystem* Session(const FString& Slot)
{
    auto* Instance=NewObject<UGameInstance>();auto* Result=NewObject<ULaceLoftWardrobeSubsystem>(Instance);
    Result->SetSaveSlotForTesting(Slot);return Result;
}
void TraceSession(ULaceLoftWardrobeSubsystem* Session,const FLaceLoftPatternPath& Path)
{
    for(int32 I=1;I<Path.PointsMm.Num();++I)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Path.PointsMm[I-1],Path.PointsMm[I])));
        for(int32 J=1;J<=Steps;++J)Session->UpdateStroke(FMath::Lerp(Path.PointsMm[I-1],Path.PointsMm[I],double(J)/Steps));
    }
    Session->EndStroke();
}
bool FinishProject(ULaceLoftWardrobeSubsystem* Session,FGuid Id)
{
    if(!Session->SelectProject(Id))return false;
    const auto* Project=Session->GetProject(Id);auto* Pattern=Session->GetPattern(Project->PatternId);
    for(const auto& Piece:Pattern->Pieces)
    {
        if(!Session->BeginCut(Piece.PieceId,Piece.CutOutline.PointsMm[0]))return false;
        TraceSession(Session,Piece.CutOutline);
    }
    for(int32 Pass=0;Pass<Pattern->Seams.Num();++Pass)for(const auto& Seam:Pattern->Seams)
    {
        if(!Session->GetModel(Id)->CanStartSeam(Seam.SeamId))continue;
        for(int32 I=0;I<Seam.SideA.Num();++I)
        {
            const auto& Ref=Seam.SideA[I];const auto& Path=Pattern->FindPiece(Ref.PieceId)->FindEdge(Ref.EdgeId)->Path;
            if(!Session->BeginSeam(Seam.SeamId,I,Path.PointsMm[0]))return false;
            TraceSession(Session,Path);
        }
    }
    return Session->CanEquip(Id);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftSessionPersistenceTest,"LaceLoft.Session.SaveRestoreUndo",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftSessionPersistenceTest::RunTest(const FString& Parameters)
{
    const FString Slot=TEXT("LaceLoft_Automation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto* First=Session(Slot);
    TestTrue(TEXT("Empty slot opens safely"),First->LoadSession());
    const FGuid Top=First->CreateProject(TEXT("TShirt")),Bottom=First->CreateProject(TEXT("DolphinShorts"));
    if(!TestTrue(TEXT("Projects have independent stable identities"),Top.IsValid() && Bottom.IsValid() && Top!=Bottom))return false;
    TestFalse(TEXT("Unfinished result cannot equip"),First->Equip(Top));
    TestTrue(TEXT("Create complete top through pointer samples"),FinishProject(First,Top));
    TestTrue(TEXT("Create complete bottom through pointer samples"),FinishProject(First,Bottom));
    TestTrue(TEXT("Equip top"),First->Equip(Top));TestTrue(TEXT("Equip bottom"),First->Equip(Bottom));
    First->SetPose(TEXT("ArmsRaised"));First->SetJointRotation(TEXT("head"),FRotator(900,12,-10));
    First->SetJointRotation(TEXT("root"),FRotator(30,0,0));
    TestFalse(TEXT("Unknown or root joints cannot be edited"),First->GetJointRotations().Contains(TEXT("root")));
    TestTrue(TEXT("Explicit save succeeds"),First->SaveSession());
    auto* Restored=Session(Slot);TestTrue(TEXT("Fresh session loads saved values"),Restored->LoadSession());
    TestEqual(TEXT("Both independent projects survive"),Restored->GetProjects().Num(),2);
    TestEqual(TEXT("Pose preset survives reload"),Restored->GetPose(),FName(TEXT("ArmsRaised")));
    TestEqual(TEXT("Joint rotation is clamped and saved"),Restored->GetJointRotations().FindRef(TEXT("head")).Pitch,90.);
    TestEqual(TEXT("Top survives reload"),Restored->GetEquipped(ELaceLoftGarmentSlot::Top),Top);
    TestEqual(TEXT("Bottom survives reload"),Restored->GetEquipped(ELaceLoftGarmentSlot::Bottom),Bottom);
    Restored->SelectProject(Top);TestTrue(TEXT("History survives reload"),Restored->Undo());
    TestFalse(TEXT("Undo revokes current top"),Restored->CanEquip(Top));
    TestFalse(TEXT("Undo removes equipped top"),Restored->GetEquipped(ELaceLoftGarmentSlot::Top).IsValid());
    TestEqual(TEXT("Other garment remains equipped"),Restored->GetEquipped(ELaceLoftGarmentSlot::Bottom),Bottom);
    auto* Again=Session(Slot);TestTrue(TEXT("Undo command boundary was saved"),Again->LoadSession());
    TestFalse(TEXT("Restart does not restore revoked result"),Again->CanEquip(Top));
    TestTrue(TEXT("Other project still complete"),Again->CanEquip(Bottom));
    TestTrue(TEXT("Redo history survives restart"),Again->Redo());
    TestTrue(TEXT("Redo restores top qualification"),Again->CanEquip(Top));
    auto* Pattern=Again->GetPattern(TEXT("TShirt"));const auto SavedCloth=Pattern->ClothAsset;Pattern->ClothAsset.Reset();
    TestFalse(TEXT("Missing cloth cannot equip even completed project"),Again->CanEquip(Top));Pattern->ClothAsset=SavedCloth;
    auto* Stale=const_cast<FLaceLoftGarmentProject*>(Again->GetProject(Top));++Stale->PatternVersion;
    Again->SaveSession();auto* Versioned=Session(Slot);Versioned->LoadSession();
    TestEqual(TEXT("Version recovery keeps other valid project"),Versioned->GetProjects().Num(),1);
    TestFalse(TEXT("Stale project not eligible"),Versioned->CanEquip(Top));
    TestTrue(TEXT("Version mismatch original preserved"),!Versioned->GetRecoverySlot().IsEmpty());
    const TArray<uint8> Corrupt={1,2,3,4,5};UGameplayStatics::SaveDataToSlot(Corrupt,Slot,0);
    auto* Recovery=Session(Slot);Recovery->LoadSession();
    TestEqual(TEXT("Corrupt save recovers empty state"),Recovery->GetProjects().Num(),0);
    TArray<uint8> Backup;UGameplayStatics::LoadDataFromSlot(Backup,Recovery->GetRecoverySlot(),0);
    TestTrue(TEXT("Corrupt bytes preserved exactly"),Backup==Corrupt);
    TestFalse(TEXT("Recovery is explained to user"),Recovery->GetStatusMessage().IsEmpty());
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftSaveLimitsTest,"LaceLoft.Session.SaveLimitsPreservePrevious",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftSaveLimitsTest::RunTest(const FString& Parameters)
{
    const FString Slot=TEXT("LaceLoft_LimitsAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto* Current=Session(Slot);Current->LoadSession();const auto Id=Current->CreateProject(TEXT("TShirt"));
    TArray<uint8> Before,After;UGameplayStatics::LoadDataFromSlot(Before,Slot,0);
    auto* Project=const_cast<FLaceLoftGarmentProject*>(Current->GetProject(Id));
    FLaceLoftEditCommand Large;FLaceLoftPathProgress Intervals;
    Intervals.Intervals.Init(FLaceLoftInterval(0,1),1000);
    Large.Before.Add(Current->GetPattern(TEXT("TShirt"))->Pieces[0].CutOutline.PathId,Intervals);Large.After=Large.Before;
    Project->UndoCommands.Init(Large,600);
    TestFalse(TEXT("Oversized history is rejected before replacing disk data"),Current->SaveSession());
    UGameplayStatics::LoadDataFromSlot(After,Slot,0);TestTrue(TEXT("Last readable save remains byte-identical"),Before==After);
    TestTrue(TEXT("Capacity failure is visible to the user"),Current->GetStatusMessage().ToString().Contains(TEXT("16MB")));
    Project->UndoCommands.Reset();
    Current->SetFailSavePromotionForTesting(true);
    Current->CreateProject(TEXT("DolphinShorts"));
    UGameplayStatics::LoadDataFromSlot(After,Slot,0);TestTrue(TEXT("Failure after staging leaves the previous save byte-identical"),Before==After);
    auto* Interrupted=Session(Slot);TestTrue(TEXT("Previous save remains readable after interrupted promotion"),Interrupted->LoadSession());
    TestEqual(TEXT("Only previously committed project is recovered"),Interrupted->GetProjects().Num(),1);
    Current->SetFailSavePromotionForTesting(false);Current->LoadSession();
    auto* Limits=Current->GetJointLimits();if(!TestNotNull(TEXT("Authored per-joint range data loads"),Limits))return false;
    TestEqual(TEXT("Nine editable joint ranges"),Limits->Joints.Num(),9);
    auto& Head=Limits->Joints.FindChecked(TEXT("head"));const auto Original=Head;Head.Maximum.Pitch=37;
    Current->SetJointRotation(TEXT("head"),FRotator(60,0,0));
    TestEqual(TEXT("Runtime clamp reads joint-specific data"),Current->GetJointRotations().FindRef(TEXT("head")).Pitch,37.);Head=Original;
    for(int32 I=1;I<128;++I)Current->CreateProject(TEXT("TShirt"));
    TestFalse(TEXT("129th project is refused without corrupting the valid collection"),Current->CreateProject(TEXT("TShirt")).IsValid());
    auto* Fresh=Session(Slot);TestTrue(TEXT("Maximum allowed project collection reloads"),Fresh->LoadSession());
    TestEqual(TEXT("All 128 projects remain available"),Fresh->GetProjects().Num(),128);
    return !HasAnyErrors();
}
#endif
