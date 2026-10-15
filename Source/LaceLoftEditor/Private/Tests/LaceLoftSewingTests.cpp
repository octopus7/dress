#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Garments/LaceLoftSewingModel.h"
#include "Garments/LaceLoftPatternDefinition.h"

namespace
{
void Trace(FLaceLoftSewingModel& Model,const FLaceLoftPatternPath& Path)
{
    for(int32 I=1;I<Path.PointsMm.Num();++I)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Path.PointsMm[I-1],Path.PointsMm[I])));
        for(int32 Step=1;Step<=Steps;++Step)Model.UpdateStroke(FMath::Lerp(Path.PointsMm[I-1],Path.PointsMm[I],double(Step)/Steps));
    }
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftSewingRulesTest,"LaceLoft.Sewing.GuidedAssembly",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftSewingRulesTest::RunTest(const FString& Parameters)
{
    for(const FString Id:{TEXT("TShirt"),TEXT("DolphinShorts")})
    {
        auto* Pattern=LoadObject<ULaceLoftPatternDefinition>(nullptr,*(TEXT("/Game/Garments/Patterns/DA_")+Id+TEXT(".DA_")+Id));
        if(!TestNotNull(TEXT("Pattern"),Pattern))continue;
        FLaceLoftSewingModel Model;
        TestTrue(TEXT("Valid pattern configured"),Model.Configure(*Pattern,{}));
        TestFalse(TEXT("Cannot sew uncut fabric"),Model.CanStartSeam(Pattern->Seams[0].SeamId));
        TestFalse(TEXT("Unknown operation rejected"),Model.BeginSeam(TEXT("wrong_pair"),0,FVector2D::ZeroVector));
        for(const auto& Piece:Pattern->Pieces)
        {
            TestTrue(TEXT("Begin cut"),Model.BeginCut(Piece.PieceId,Piece.CutOutline.PointsMm[0]));
            Trace(Model,Piece.CutOutline);Model.EndStroke();
            TestTrue(TEXT("Continuous closed outline frees piece"),Model.IsPieceCut(Piece.PieceId));
        }
        TestFalse(TEXT("Later operation requires prerequisites"),Model.CanStartSeam(Pattern->Seams.Last().SeamId));
        TOptional<FLaceLoftStrokeChange> Last;
        for(const auto& Seam:Pattern->Seams)
        {
            if(Model.IsSeamComplete(Seam.SeamId))continue;
            // Source order need not be topological; choose any currently available step.
            for(int32 Pass=0;Pass<Pattern->Seams.Num();++Pass)for(const auto& Ready:Pattern->Seams)
            {
                if(!Model.CanStartSeam(Ready.SeamId))continue;
                TestFalse(TEXT("Invalid side pairing rejected"),Model.BeginSeam(Ready.SeamId,999,FVector2D::ZeroVector));
                for(int32 J=0;J<Ready.SideA.Num();++J)
                {
                    const auto& Ref=Ready.SideA[J];const auto& Path=Pattern->FindPiece(Ref.PieceId)->FindEdge(Ref.EdgeId)->Path;
                    if(Model.BeginSeam(Ready.SeamId,J,Path.PointsMm[0])){Trace(Model,Path);Last=Model.EndStroke();}
                }
                TestTrue(TEXT("All required guided edge spans complete operation"),Model.IsSeamComplete(Ready.SeamId));
                TestFalse(TEXT("Completed seam cannot complete twice"),Model.CanStartSeam(Ready.SeamId));
            }
        }
        TestTrue(TEXT("All cut and seam operations unlock the result"),Model.IsCompleted());
        if(Last)
        {
            Model.ApplyChange(*Last,true);TestFalse(TEXT("Undo revokes result eligibility"),Model.IsCompleted());
            Model.ApplyChange(*Last,false);TestTrue(TEXT("Redo restores actual coverage"),Model.IsCompleted());
        }
    }
    return !HasAnyErrors();
}
#endif
