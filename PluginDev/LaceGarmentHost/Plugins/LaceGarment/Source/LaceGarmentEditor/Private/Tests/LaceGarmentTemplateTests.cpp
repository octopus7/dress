#include "LaceGarmentTemplates.h"
#include "LaceGarmentSampling.h"
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
using namespace LaceGarment;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGTemplateNeckLength,"LaceGarment.Templates.NeckAnchorLength",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGTemplateNeckLength::RunTest(const FString&)
{
    auto* D=NewObject<ULaceGarmentDocument>(); FString Error;
    if(!TestTrue(TEXT("Create shirt source"),ApplyTemplate(*D,ETemplate::TShirt,{},Error))) return false;
    TArray<FCapsule> Body; TArray<FConvexCollider> Convexes;
    if(!TestTrue(TEXT("Read actual saved collision"),ReadBodyCollision(LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11")),Body,Convexes,Error))) return false;
    // Bezier control polygon subdivision gives an upper bound on rest length.
    // A sphere at either capsule endpoint is a subset of the collision body.
    // Its shortest exterior path is therefore a necessary lower bound, not a
    // guarantee that the full capsule union or the complete garment will fit.
    auto ArcUpper=[](auto&& Self,FVector2d A,FVector2d B,FVector2d C,FVector2d E,int32 Depth)->double
    {
        if(Depth==0) return (B-A).Length()+(C-B).Length()+(E-C).Length();
        const auto AB=(A+B)*.5,BC=(B+C)*.5,CE=(C+E)*.5;
        const auto ABC=(AB+BC)*.5,BCE=(BC+CE)*.5,Mid=(ABC+BCE)*.5;
        return Self(Self,A,AB,ABC,Mid,Depth-1)+Self(Self,Mid,BCE,CE,E,Depth-1);
    };
    int32 Checked=0;
    for(const auto& Piece : D->Pieces) for(const auto& Edge : Piece.Edges)
    {
        const auto* Start=D->Anchors.FindByPredicate([&](const auto& A){return A.Piece==Piece.Id && A.Point==Edge.Start && A.bUseTarget;});
        const auto* End=D->Anchors.FindByPredicate([&](const auto& A){return A.Piece==Piece.Id && A.Point==Edge.End && A.bUseTarget;});
        if(!Start || !End) continue;
        auto Point=[&](FGuid Id){return FVector2d(Piece.Points.FindByPredicate([&](const auto& P){return P.Id==Id;})->PositionMm);};
        const double RestCm=(Edge.Curve==ELGCurveType::Cubic?ArcUpper(ArcUpper,Point(Edge.Start),Point(Edge.ControlA),Point(Edge.ControlB),Point(Edge.End),10):(Point(Edge.End)-Point(Edge.Start)).Length())/10;
        double Lower=(Start->TargetCm-End->TargetCm).Length();
        for(const auto& Capsule : Body) for(const auto Center : {Capsule.A,Capsule.B})
        {
            // Even the final face gate permits only .005cm penetration. Shrink
            // the physical sphere by that full allowance to avoid overstating
            // the obstruction using the solver's larger collision thickness.
            const double R=FMath::Max(0.0,Capsule.RadiusCm-.005);
            const auto A=FVector3d(Start->TargetCm)-Center,B=FVector3d(End->TargetCm)-Center;
            const double DA=A.Length(),DB=B.Length();
            if(DA<R || DB<R) { AddError(TEXT("Template endpoint is inside the permitted collision surface")); return false; }
            const double Angle=FMath::Acos(FMath::Clamp(FVector3d::DotProduct(A,B)/(DA*DB),-1.0,1.0));
            const double Arc=Angle-FMath::Acos(R/DA)-FMath::Acos(R/DB);
            if(Arc>0) Lower=FMath::Max(Lower,FMath::Sqrt(DA*DA-R*R)+FMath::Sqrt(DB*DB-R*R)+R*Arc);
        }
        ++Checked;
        TestTrue(*FString::Printf(TEXT("%s neck has enough rest length: required >= %.6fcm, 10%% limit %.6fcm"),*Piece.Name,Lower,RestCm*1.1),Lower<=RestCm*1.1+1.e-6);
    }
    TestEqual(TEXT("Both neck edges of front and back checked"),Checked,4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGTShirtTemplate,"LaceGarment.Templates.TShirtSleeveEdit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGTShirtTemplate::RunTest(const FString&)
{
    auto* D=NewObject<ULaceGarmentDocument>(); FString Error;
    if(!TestTrue(TEXT("Create embedded editable T-shirt source"),ApplyTemplate(*D,ETemplate::TShirt,{},Error))) { AddInfo(Error); return false; }
    TestTrue(TEXT("Template source is structurally valid"),D->ValidateStructure(Error));
    FSampledDocument S;
    TestTrue(TEXT("Default T-shirt source samples and triangulates"),SampleDocument(*D,S,Error)); AddInfo(Error);
    TArray<FCapsule> Body; TArray<FConvexCollider> Convexes;
    if(!TestTrue(TEXT("Template placement uses the actual body collision"),ReadBodyCollision(LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11")),Body,Convexes,Error))) return false;
    for(const auto& Pin : S.Input.Pins)
    {
        const auto& Panel=S.Input.Panels[Pin.Panel];
        const auto UV=Panel.BoundaryMm[Pin.BoundaryPoint];
        const FVector3d Start=Panel.InitialPlacementCm.TransformPosition(FVector(UV.X/10,UV.Y/10,0));
        for(int32 Step=0;Step<=60;++Step)
        {
            const auto Position=FMath::Lerp(Start,Pin.TargetCm.Get(Start),Step/60.0);
            for(const auto& C : Body)
            {
                const auto Axis=C.B-C.A;
                const auto Q=C.A+Axis*FMath::Clamp(FVector3d::DotProduct(Position-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
                TestTrue(TEXT("Template guide path remains outside the body"),(Position-Q).Length()>=C.RadiusCm+S.Input.ThicknessCm-.005);
            }
            for(const auto& C : Convexes)
            {FVector3d N;TestTrue(TEXT("Template guide path remains outside cooked convex"),C.SignedDistance(Position,N)>=S.Input.ThicknessCm-.005);}
        }
    }
    const auto Old=D->Pieces; const auto Seams=D->Seams;
    double OldExtent=0; for(const auto& P : Old[0].Points) OldExtent=FMath::Max(OldExtent,FMath::Abs(P.PositionMm.X));
    if(!TestTrue(TEXT("Extend the authored sleeve"),ApplyTemplate(*D,ETemplate::TShirt,{{TEXT("sleeveLength"),22}},Error))) return false;
    double NewExtent=0; for(const auto& P : D->Pieces[0].Points) NewExtent=FMath::Max(NewExtent,FMath::Abs(P.PositionMm.X));
    TestTrue(TEXT("Longer sleeve changes the actual pattern"),NewExtent>OldExtent+2);
    TestEqual(TEXT("Piece count remains stable"),D->Pieces.Num(),Old.Num());
    for(int32 I=0;I<Old.Num();++I)
    {
        TestTrue(TEXT("Piece IDs survive parameter edits"),Old[I].Id==D->Pieces[I].Id);
        for(int32 J=0;J<Old[I].Points.Num();++J) TestTrue(TEXT("Control point IDs survive parameter edits"),Old[I].Points[J].Id==D->Pieces[I].Points[J].Id);
    }
    for(int32 I=0;I<Seams.Num();++I) TestTrue(TEXT("Seam IDs survive parameter edits"),Seams[I].Id==D->Seams[I].Id);
    TestFalse(TEXT("Parameter edits invalidate derived state"),D->bDerivedCurrent);
    TestTrue(TEXT("Modified T-shirt resamples"),SampleDocument(*D,S,Error)); AddInfo(Error);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGShortsTemplate,"LaceGarment.Templates.ShortsLengthAndCorrection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGShortsTemplate::RunTest(const FString&)
{
    auto* D=NewObject<ULaceGarmentDocument>(); FString Error;
    if(!TestTrue(TEXT("Create editable dolphin shorts source"),ApplyTemplate(*D,ETemplate::DolphinShorts,{},Error))) { AddInfo(Error); return false; }
    FLGCorrection C; C.Id=FGuid::NewGuid(); C.Target=FGuid::NewGuid(); C.TopologyHash=FString::ChrN(64,TEXT('a')); C.Property=TEXT("maxDistance"); C.Value=1; C.State=ELGCorrectionState::Resolved; D->Corrections.Add(C);
    const auto Old=D->Pieces;
    if(!TestTrue(TEXT("Change shorts length"),ApplyTemplate(*D,ETemplate::DolphinShorts,{{TEXT("shortsLength"),27}},Error))) return false;
    TestTrue(TEXT("Length changes source positions"),!Old[0].Points[0].PositionMm.Equals(D->Pieces[0].Points[0].PositionMm));
    TestEqual(TEXT("Manual corrections retained"),D->Corrections.Num(),1);
    TestTrue(TEXT("Retained corrections await topology resolution"),D->HasUnresolvedCorrections());
    TestTrue(TEXT("Shorts source structurally valid"),D->ValidateStructure(Error));
    FSampledDocument S; TestTrue(TEXT("Modified shorts mesh can be generated"),SampleDocument(*D,S,Error)); AddInfo(Error);
    const auto Previous=D->Pieces[0].Points[0].PositionMm;
    TestFalse(TEXT("Unsupported length rejected"),ApplyTemplate(*D,ETemplate::DolphinShorts,{{TEXT("shortsLength"),-1}},Error));
    TestTrue(TEXT("Failed update leaves source untouched"),D->Pieces[0].Points[0].PositionMm==Previous);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGShortsAnchorLength,"LaceGarment.Templates.ShortsAnchorLength",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGShortsAnchorLength::RunTest(const FString&)
{
    auto* D=NewObject<ULaceGarmentDocument>();FString Error;FSampledDocument Source;
    if(!TestTrue(TEXT("Create actual shorts mesh source"),ApplyTemplate(*D,ETemplate::DolphinShorts,{},Error) && SampleDocument(*D,Source,Error))) return false;
    double Sum=0;TArray<FVector3d> Targets;
    for(int32 I=0;I<Source.Input.Panels.Num();++I)
    {
        FPanelMesh Mesh;if(!TestTrue(TEXT("Triangulate actual shorts panel"),BuildPanel(Source.Input.Panels[I],Mesh,Error))) return false;
        const auto* Pin=Source.Input.Pins.FindByPredicate([&](const FPin& P){return P.Panel==I;});
        if(!TestNotNull(TEXT("Shorts waist anchor exists"),Pin)) return false;
        const int32 Crotch=Source.BoundarySource[I].IndexOfByPredicate([&](const FBoundarySource& P){return P.Point==D->Pieces[I].Points[5].Id;});
        if(!TestTrue(TEXT("Crotch source point resolves"),Crotch!=INDEX_NONE)) return false;
        TArray<TArray<int32>> Neighbors;Neighbors.SetNum(Mesh.RestMm.Num());
        for(auto T : Mesh.Triangles) for(int32 K=0;K<3;++K) {Neighbors[T[K]].AddUnique(T[(K+1)%3]);Neighbors[T[(K+1)%3]].AddUnique(T[K]);}
        TArray<double> Distance;Distance.Init(DBL_MAX,Mesh.RestMm.Num());Distance[Pin->BoundaryPoint]=0;TSet<int32> Visited;
        for(int32 Iter=0;Iter<Distance.Num();++Iter)
        {
            int32 Next=INDEX_NONE;
            for(int32 V=0;V<Distance.Num();++V) if(!Visited.Contains(V) && (Next==INDEX_NONE || Distance[V]<Distance[Next])) Next=V;
            if(Next==Crotch) break;
            if(Next==INDEX_NONE || Distance[Next]==DBL_MAX) return false;
            Visited.Add(Next);
            for(int32 V : Neighbors[Next]) Distance[V]=FMath::Min(Distance[V],Distance[Next]+(Mesh.RestMm[V]-Mesh.RestMm[Next]).Length()/10);
        }
        Sum+=Distance[Crotch];Targets.Add(Pin->TargetCm.Get(Mesh.InitialCm[Pin->BoundaryPoint]));
    }
    if(!TestEqual(TEXT("Two waist anchors checked"),Targets.Num(),2)) return false;
    // Every edge on these paths is bounded by 10% strain, and the crotch weld
    // can leave at most .02cm seam gap. Triangle inequality gives this necessary
    // condition even without any body obstruction. Passing does not prove fit.
    const double Required=(Targets[0]-Targets[1]).Length(),Available=Sum*1.1+.02;
    TestTrue(*FString::Printf(TEXT("Waist anchors can connect through the crotch: %.4fcm required, %.4fcm maximum"),Required,Available),Required<=Available);
    TArray<FCapsule> Capsules;TArray<FConvexCollider> Convexes;
    if(!TestTrue(TEXT("Read actual body for waist guide paths"),ReadBodyCollision(LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11")),Capsules,Convexes,Error))) return false;
    for(const auto& Pin : Source.Input.Pins)
    {
        const auto& Panel=Source.Input.Panels[Pin.Panel];const auto UV=Panel.BoundaryMm[Pin.BoundaryPoint];
        const FVector3d Start=Panel.InitialPlacementCm.TransformPosition(FVector(UV.X/10,UV.Y/10,0));
        for(int32 Step=0;Step<=60;++Step)
        {
            const auto P=FMath::Lerp(Start,Pin.TargetCm.Get(Start),Step/60.0);double Distance=DBL_MAX;
            for(const auto& C : Capsules)
            {const auto Axis=C.B-C.A;const auto Q=C.A+Axis*FMath::Clamp(FVector3d::DotProduct(P-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);Distance=FMath::Min(Distance,(P-Q).Length()-C.RadiusCm);}
            for(const auto& C : Convexes) {FVector3d N;Distance=FMath::Min(Distance,C.SignedDistance(P,N));}
            TestTrue(TEXT("Entire waist guide path stays outside the saved body"),Distance>=Source.Input.ThicknessCm-.005);
        }
    }
    return true;
}
