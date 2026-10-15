#include "LaceGarmentSampling.h"
#include "Misc/AutomationTest.h"
using namespace LaceGarment;
namespace
{
ULaceGarmentDocument* SamplingFixture(double Width=20)
{
    auto* Doc=NewObject<ULaceGarmentDocument>();
    FLGPiece P; P.Id=FGuid::NewGuid(); P.Name=TEXT("Editable panel");
    for(auto V : TArray<FVector2D>{{0,0},{Width,0},{Width,30},{0,30},{Width*.25,-10},{Width*.75,-10}})
    { FLGControlPoint C; C.Id=FGuid::NewGuid(); C.PositionMm=V; P.Points.Add(C); }
    for(int32 I=0;I<4;++I)
    { FLGEdge E; E.Id=FGuid::NewGuid(); E.Start=P.Points[I].Id; E.End=P.Points[(I+1)%4].Id; P.Edges.Add(E); P.Outer.Edges.Add(E.Id); }
    Doc->Pieces.Add(P);
    FLGAnchor A; A.Id=FGuid::NewGuid(); A.Piece=P.Id; A.Point=P.Points[0].Id; Doc->Anchors.Add(A);
    return Doc;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGSampleCurves,"LaceGarment.Document.SampleCurvedBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGSampleCurves::RunTest(const FString&)
{
    auto* D=SamplingFixture(); auto& P=D->Pieces[0];
    P.Edges[0].Curve=ELGCurveType::Cubic; P.Edges[0].ControlA=P.Points[4].Id; P.Edges[0].ControlB=P.Points[5].Id;
    FLGNotch N; N.Id=FGuid::NewGuid(); N.Edge=P.Edges[0].Id; N.Parameter=.37; P.Notches.Add(N);
    FSampledDocument S; FString Error;
    if(!TestTrue(TEXT("Sample authored cubic into a closed panel"),SampleDocument(*D,S,Error))) { AddInfo(Error); return false; }
    bool HasNotch=false; double MinY=0,Longest=0;
    const auto& B=S.Input.Panels[0].BoundaryMm;
    for(int32 I=0;I<B.Num();++I)
    {
        MinY=FMath::Min(MinY,B[I].Y); Longest=FMath::Max(Longest,(B[I]-B[(I+1)%B.Num()]).Length());
        const auto& Ref=S.BoundarySource[0][I];
        if(Ref.Edge==N.Edge && FMath::IsNearlyEqual(Ref.Parameter,N.Parameter,1.e-10)) HasNotch=true;
    }
    TestTrue(TEXT("Cubic shape retained"),MinY<-7.4);
    TestTrue(TEXT("Notch is an exact source parameter sample"),HasNotch);
    TestTrue(TEXT("Boundary spacing enforced"),Longest<=D->BoundarySpacingMm*1.001);
    TestEqual(TEXT("Stable point anchor maps to first boundary point"),S.Input.Pins[0].BoundaryPoint,0);
    FPanelMesh M; TestTrue(TEXT("Sampled curve triangulates"),BuildPanel(S.Input.Panels[0],M,Error));
    FSampledDocument Again; TestTrue(TEXT("Repeat sampling"),SampleDocument(*D,Again,Error));
    TestTrue(TEXT("Sampling is deterministic"),Again.Input.Panels[0].BoundaryMm==B);
    TestEqual(TEXT("Source control count is unchanged"),P.Points.Num(),6);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGSampleDraft,"LaceGarment.Document.GenerationRejectsDraftGeometry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGSampleDraft::RunTest(const FString&)
{
    auto* D=SamplingFixture(); FString Error; FSampledDocument S; S.SeamIds.Add(FGuid::NewGuid());
    D->Pieces[0].Outer.Edges.RemoveAt(2);
    TestTrue(TEXT("Open loop remains a structurally valid editable draft"),D->ValidateStructure(Error));
    TestFalse(TEXT("Open draft cannot generate"),SampleDocument(*D,S,Error));
    TestTrue(TEXT("Error identifies closure"),Error.Contains(TEXT("closed")));
    TestEqual(TEXT("Failed generation leaves previous output intact"),S.SeamIds.Num(),1);
    D=SamplingFixture(); D->Anchors[0].Point=D->Pieces[0].Points[4].Id;
    TestFalse(TEXT("A control handle cannot silently become an anchor on the mesh"),SampleDocument(*D,S,Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGSampleSeam,"LaceGarment.Document.SampleSeamIntervalsAndEase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGSampleSeam::RunTest(const FString&)
{
    auto* D=SamplingFixture(20); auto* Other=SamplingFixture(21); D->Pieces.Add(Other->Pieces[0]);
    FLGSeam Seam; Seam.Id=FGuid::NewGuid(); Seam.A.Piece=D->Pieces[0].Id; Seam.A.Edge=D->Pieces[0].Edges[0].Id;
    Seam.B.Piece=D->Pieces[1].Id; Seam.B.Edge=D->Pieces[1].Edges[0].Id;
    Seam.A.From=.2; Seam.A.To=.8; Seam.B.From=.2; Seam.B.To=.8;
    Seam.EaseRatio=20.0/21.0; Seam.LengthToleranceMm=.001; D->Seams.Add(Seam);
    FSampledDocument S; FString Error;
    if(!TestTrue(TEXT("Explicit length ratio accepted for partial seam"),SampleDocument(*D,S,Error))) { AddInfo(Error); return false; }
    TestEqual(TEXT("Source seam remains one seam with provenance"),S.SeamIds.Num(),1);
    TestTrue(TEXT("Logical seam identity preserved"),S.SeamIds[0]==Seam.Id);
    D->Seams[0].EaseRatio=1;
    TestFalse(TEXT("Unspecified excess length rejected"),SampleDocument(*D,S,Error));
    TestTrue(TEXT("Length failure identifies seam"),Error.Contains(Seam.Id.ToString()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGSampleIntegration,"LaceGarment.Document.SampleToNativeDrape",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGSampleIntegration::RunTest(const FString&)
{
    auto* D=SamplingFixture(20); auto* Other=SamplingFixture(21);
    Other->Pieces[0].InitialPlacementCm.SetTranslation(FVector(0,-3,0)); D->Pieces.Add(Other->Pieces[0]);
    FLGSeam Seam; Seam.Id=FGuid::NewGuid();
    Seam.A.Piece=D->Pieces[0].Id; Seam.A.Edge=D->Pieces[0].Edges[0].Id;
    Seam.B.Piece=D->Pieces[1].Id; Seam.B.Edge=D->Pieces[1].Edges[2].Id;
    Seam.EaseRatio=20.0/21.0; Seam.LengthToleranceMm=.001; D->Seams.Add(Seam);
    FLGNotch A,B; A.Id=FGuid::NewGuid(); B.Id=FGuid::NewGuid(); A.MatchingNotch=B.Id; B.MatchingNotch=A.Id;
    A.Edge=Seam.A.Edge; A.Parameter=.25; B.Edge=Seam.B.Edge; B.Parameter=.75;
    D->Pieces[0].Notches.Add(A); D->Pieces[1].Notches.Add(B);
    FString Error; FSampledDocument S;
    if(!TestTrue(TEXT("Sample unequal seam lengths with reverse notch pairing"),SampleDocument(*D,S,Error))) { AddInfo(Error); return false; }
    S.Input.GravityCm=0; FBuildResult Build;
    if(!TestTrue(TEXT("Authored seams feed the native drape without proximity matching"),BuildGarment(S.Input,Build,Error))) { AddInfo(Error); return false; }
    int32 VA=INDEX_NONE,VB=INDEX_NONE;
    for(int32 I=0;I<S.BoundarySource[0].Num();++I)
        if(S.BoundarySource[0][I].Edge==A.Edge && FMath::IsNearlyEqual(S.BoundarySource[0][I].Parameter,A.Parameter,1.e-10)) VA=I;
    for(int32 I=0;I<S.BoundarySource[1].Num();++I)
        if(S.BoundarySource[1][I].Edge==B.Edge && FMath::IsNearlyEqual(S.BoundarySource[1][I].Parameter,B.Parameter,1.e-10)) VB=I;
    TestTrue(TEXT("Both authored notch vertices retained"),VA!=INDEX_NONE && VB!=INDEX_NONE);
    TestTrue(TEXT("Paired notches become an explicit stitch"),Build.Stitches.Contains(FIntVector2(VA,Build.Offsets[1]+VB)));
    TestTrue(TEXT("Seam closes within generation tolerance"),Build.MaxSeamGapCm<.02);
    TestTrue(TEXT("Native output records convergence"),Build.bConverged);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGNotchDirection,"LaceGarment.Document.NotchOrderAndOverlappingSeams",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGNotchDirection::RunTest(const FString&)
{
    auto* D=SamplingFixture(); auto* Other=SamplingFixture(); D->Pieces.Add(Other->Pieces[0]);
    FLGSeam Seam; Seam.Id=FGuid::NewGuid(); Seam.A.Piece=D->Pieces[0].Id; Seam.A.Edge=D->Pieces[0].Edges[0].Id;
    Seam.B.Piece=D->Pieces[1].Id; Seam.B.Edge=D->Pieces[1].Edges[0].Id; Seam.ReverseB=false; D->Seams.Add(Seam);
    for(int32 I=0;I<2;++I)
    {
        FLGNotch A,B; A.Id=FGuid::NewGuid(); B.Id=FGuid::NewGuid(); A.MatchingNotch=B.Id; B.MatchingNotch=A.Id;
        A.Edge=Seam.A.Edge; A.Parameter=I==0?.2:.8; B.Edge=Seam.B.Edge; B.Parameter=I==0?.7:.3;
        D->Pieces[0].Notches.Add(A); D->Pieces[1].Notches.Add(B);
    }
    FString Error; FSampledDocument S;
    TestFalse(TEXT("Crossing notch correspondence blocked"),SampleDocument(*D,S,Error));
    TestTrue(TEXT("Error specifies seam direction"),Error.Contains(TEXT("direction")));
    D->Seams[0].ReverseB=true;
    TestTrue(TEXT("Correct direction preserves nonuniform notch layout"),SampleDocument(*D,S,Error));
    auto Duplicate=D->Seams[0]; Duplicate.Id=FGuid::NewGuid(); Duplicate.A.From=.3; Duplicate.A.To=.6;
    Duplicate.B.From=.3; Duplicate.B.To=.6; D->Seams.Add(Duplicate);
    TestFalse(TEXT("Two seams cannot consume the same interval"),SampleDocument(*D,S,Error));
    TestTrue(TEXT("Overlap diagnosed"),Error.Contains(TEXT("twice")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCurveSourceValidity,"LaceGarment.Document.SourceCurveSelfIntersection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGCurveSourceValidity::RunTest(const FString&)
{
    auto* D=SamplingFixture(10); auto& P=D->Pieces[0];
    P.Points[2].PositionMm={10,-15}; P.Points[3].PositionMm={0,-15};
    P.Points[4].PositionMm={30,30}; P.Points[5].PositionMm={-20,30};
    P.Edges[0].Curve=ELGCurveType::Cubic; P.Edges[0].ControlA=P.Points[4].Id; P.Edges[0].ControlB=P.Points[5].Id;
    D->BoundarySpacingMm=100; D->MeshMaxEdgeMm=100;
    FString Error; FSampledDocument S;
    TestTrue(TEXT("Self-intersecting curve remains an editable source draft"),D->ValidateStructure(Error));
    TestFalse(TEXT("Coarse sampling cannot hide the original curve crossing"),SampleDocument(*D,S,Error));
    TestTrue(TEXT("Original curve intersection identified"),Error.Contains(TEXT("intersect")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCurveSeamTolerance,"LaceGarment.Document.CurveSeamDiscretizationTolerance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGCurveSeamTolerance::RunTest(const FString&)
{
    auto* D=SamplingFixture(20); auto& P=D->Pieces[0];
    P.Points[4].PositionMm={0,20}; P.Points[5].PositionMm={20,20};
    P.Edges[0].Curve=ELGCurveType::Cubic; P.Edges[0].ControlA=P.Points[4].Id; P.Edges[0].ControlB=P.Points[5].Id;
    auto* Other=SamplingFixture(40); D->Pieces.Add(Other->Pieces[0]);
    FLGSeam Seam; Seam.Id=FGuid::NewGuid(); Seam.A.Piece=D->Pieces[0].Id; Seam.A.Edge=D->Pieces[0].Edges[0].Id;
    Seam.B.Piece=D->Pieces[1].Id; Seam.B.Edge=D->Pieces[1].Edges[0].Id; Seam.LengthToleranceMm=.001; D->Seams.Add(Seam);
    FString Error; FSampledDocument S;
    if(!TestTrue(TEXT("40mm cubic and 40mm line have compatible source lengths"),SampleDocument(*D,S,Error))) { AddInfo(Error); return false; }
    auto Length=[&](int32 Panel,const TArray<int32>& Edges)
    {
        const auto& B=S.Input.Panels[Panel].BoundaryMm; double Sum=0;
        for(int32 I : Edges) Sum+=(B[I]-B[(I+1)%B.Num()]).Length();
        return Sum;
    };
    const auto& S0=S.Input.Seams[0];
    TestTrue(TEXT("Generated chord chains also satisfy authored seam tolerance"),FMath::Abs(Length(0,S0.BoundaryEdgesA)-Length(1,S0.BoundaryEdgesB))<=.001);
    TestEqual(TEXT("Adaptive resampling retains equal paired counts"),S0.BoundaryEdgesA.Num(),S0.BoundaryEdgesB.Num());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGDocumentCutout,"LaceGarment.Document.SampleInnerLoop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGDocumentCutout::RunTest(const FString&)
{
    auto* D=SamplingFixture(); auto& P=D->Pieces[0]; const int32 Start=P.Points.Num();
    for(auto V : TArray<FVector2D>{{5,5},{10,5},{10,10},{5,10}})
    { FLGControlPoint C; C.Id=FGuid::NewGuid(); C.PositionMm=V; P.Points.Add(C); }
    FLGLoop Hole;
    for(int32 I=0;I<4;++I)
    { FLGEdge E; E.Id=FGuid::NewGuid(); E.Start=P.Points[Start+I].Id; E.End=P.Points[Start+(I+1)%4].Id; P.Edges.Add(E); Hole.Edges.Add(E.Id); }
    P.Holes.Add(Hole); FSampledDocument S; FString Error;
    if(!TestTrue(TEXT("Editable inner loop feeds mesh generation"),SampleDocument(*D,S,Error))) { AddInfo(Error); return false; }
    TestEqual(TEXT("Outer and hole offsets preserved"),S.Input.Panels[0].LoopStarts.Num(),2);
    S.Input.GravityCm=0; FBuildResult B;
    if(!TestTrue(TEXT("Cutout survives native garment generation"),BuildGarment(S.Input,B,Error))) { AddInfo(Error); return false; }
    double Area=0;
    for(auto T : B.Panels[0].Triangles)
    { auto A=B.Panels[0].RestMm[T.Y]-B.Panels[0].RestMm[T.X],C=B.Panels[0].RestMm[T.Z]-B.Panels[0].RestMm[T.X]; Area+=(A.X*C.Y-A.Y*C.X)*.5; }
    TestTrue(TEXT("Rest surface excludes authored 25 square-mm cutout"),FMath::IsNearlyEqual(Area,575.0,1.e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGAnchorTarget,"LaceGarment.Document.AnchorTargetGuides",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGAnchorTarget::RunTest(const FString&)
{
    auto* D=SamplingFixture(); D->Anchors[0].bUseTarget=true; D->Anchors[0].TargetCm=FVector(.1,.2,.3);
    FString Error; FSampledDocument S;
    if(!TestTrue(TEXT("Source anchor with a fitting target samples"),SampleDocument(*D,S,Error))) { AddInfo(Error); return false; }
    S.Input.GravityCm=0; FBuildResult B;
    if(!TestTrue(TEXT("Target-guided fitting converges"),BuildGarment(S.Input,B,Error))) { AddInfo(Error); return false; }
    const int32 V=B.Offsets[0]+S.Input.Pins[0].BoundaryPoint;
    TestTrue(TEXT("Initial placement remains independent from the anchor target"),B.InitialCm[V].Equals(FVector3d::ZeroVector,1.e-10));
    TestTrue(TEXT("Settled anchor reaches its authored target"),B.SettledCm[V].Equals(D->Anchors[0].TargetCm,1.e-8));
    TestTrue(TEXT("Anchor remains fixed in generated Cloth"),B.PinnedVertices.Contains(V));
    S.Input.Colliders.Add({D->Anchors[0].TargetCm,D->Anchors[0].TargetCm,.05});
    TestFalse(TEXT("Target inside body is rejected"),BuildGarment(S.Input,B,Error));
    TestTrue(TEXT("Target error is actionable"),Error.Contains(TEXT("target")));
    return true;
}
