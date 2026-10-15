#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Garments/LaceLoftCuttingModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftCuttingInputTest,"LaceLoft.Cutting.ContinuousIntervals",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftCuttingInputTest::RunTest(const FString& Parameters)
{
    FLaceLoftPatternPath Line;Line.PathId=TEXT("line");Line.PointsMm={FVector2D(0,0),FVector2D(10,0)};
    FLaceLoftPatternPath Bend;Bend.PathId=TEXT("bend");Bend.PointsMm={FVector2D(0,0),FVector2D(0,2),FVector2D(2,2),FVector2D(2,0)};
    FLaceLoftCuttingModel Model;Model.SetPaths({Line,Bend});
    TestFalse(TEXT("No hover progress"),Model.UpdateStroke(FVector2D(1,0)));
    TestFalse(TEXT("Off-path press rejected"),Model.BeginStroke(TEXT("line"),FVector2D(0,3)));
    TestTrue(TEXT("On-path press"),Model.BeginStroke(TEXT("line"),FVector2D(0,0)));
    TestFalse(TEXT("Click without travel is not a command"),Model.EndStroke().IsSet());
    Model.BeginStroke(TEXT("line"),FVector2D(0,0));
    for(int32 I=1;I<=4;++I)TestTrue(TEXT("Continuous samples accepted"),Model.UpdateStroke(FVector2D(I,0)));
    for(int32 I=3;I>=0;--I)Model.UpdateStroke(FVector2D(I,0));
    auto Change=Model.EndStroke();
    TestTrue(TEXT("One stroke produces one command"),Change.IsSet());
    TestEqual(TEXT("Reverse repeat does not double progress"),Model.Progress(TEXT("line")).CoveredMm(),4.);
    TestFalse(TEXT("Release outside window ends capture"),Model.UpdateStroke(FVector2D(5,0)));
    if(Change){Model.ApplyChange(*Change,true);TestEqual(TEXT("Undo entire stroke"),Model.Progress(TEXT("line")).CoveredMm(),0.);
        Model.ApplyChange(*Change,false);TestEqual(TEXT("Redo exact union"),Model.Progress(TEXT("line")).CoveredMm(),4.);}
    Model.BeginStroke(TEXT("line"),FVector2D(4,0));
    Model.UpdateStroke(FVector2D(4,4));
    TestFalse(TEXT("Leaving path loses continuity until next press"),Model.UpdateStroke(FVector2D(5,0)));Model.EndStroke();
    Model.BeginStroke(TEXT("bend"),FVector2D(0,0));
    TestFalse(TEXT("Short spatial jump across U curve rejected"),Model.UpdateStroke(FVector2D(2,0)));Model.EndStroke();
    TestEqual(TEXT("Jump cannot fill curve"),Model.Progress(TEXT("bend")).CoveredMm(),0.);
    Model.BeginStroke(TEXT("line"),FVector2D(4,0),true);
    TestFalse(TEXT("Sewing reverse direction rejected"),Model.UpdateStroke(FVector2D(3,0)));Model.EndStroke();
    for(double Dpi:{1.,1.5,2.})for(double Zoom:{.5,1.,2.})
    {
        const FVector2D Mm(6,9),Origin(120,80),Pan(23,17);const double Px=4;
        const FVector2D Screen=Origin+(Mm*Px*Zoom+Pan)*Dpi;
        TestTrue(TEXT("Shared inverse transform survives DPI/zoom/pan"),FLaceLoftCuttingModel::ScreenToMm(Screen,Origin,Dpi,Zoom,Pan,Px).Equals(Mm,.00001));
    }
    FLaceLoftPathProgress Invalid;Invalid.Intervals={{0,100}};
    TestFalse(TEXT("Saved intervals outside path rejected"),Model.RestoreProgress(TEXT("line"),Invalid));
    FLaceLoftPatternPath Closed;Closed.PathId=TEXT("closed");Closed.PointsMm={{0,0},{10,0},{10,10},{0,10},{0,0}};
    Model.SetPaths({Closed});Model.BeginStroke(Closed.PathId,{0,0});
    for(int32 Segment=1;Segment<Closed.PointsMm.Num();++Segment)for(int32 I=1;I<=10;++I)
    {
        auto P=FMath::Lerp(Closed.PointsMm[Segment-1],Closed.PointsMm[Segment],I/10.);
        if(Segment==4 && I==10)P={.00003,.000005}; // Slate DPI float round-trip at the shared endpoint.
        TestTrue(TEXT("Closing corner keeps the continuous arc despite subpixel noise"),Model.UpdateStroke(P));
    }
    Model.EndStroke();TestTrue(TEXT("DPI rounding cannot leave the final cut segment incomplete"),Model.IsComplete(Closed.PathId));
    return !HasAnyErrors();
}
#endif
