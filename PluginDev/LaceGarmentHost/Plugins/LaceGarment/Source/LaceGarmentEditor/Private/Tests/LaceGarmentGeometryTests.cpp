#include "LaceGarmentGeometry.h"
#include "Misc/AutomationTest.h"

using namespace LaceGarment;
namespace
{
double Area(const FPanelMesh& Mesh)
{
    double Sum = 0;
    for (auto T : Mesh.Triangles)
    {
        FVector2d A = Mesh.RestMm[T.Y] - Mesh.RestMm[T.X];
        FVector2d B = Mesh.RestMm[T.Z] - Mesh.RestMm[T.X];
        Sum += (A.X * B.Y - A.Y * B.X) * 0.5;
    }
    return Sum;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGPanelDimensions, "LaceGarment.Geometry.DimensionsAndRefinement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGPanelDimensions::RunTest(const FString&)
{
    FPanel Panel;
    Panel.BoundaryMm = {{0,0}, {20,0}, {20,30}, {0,30}};
    Panel.InitialPlacementCm.SetTranslation(FVector(1,2,7));
    FPanelMesh Mesh;
    FString Error;
    if (!TestTrue(TEXT("Rectangle generates"), BuildPanel(Panel, Mesh, Error))) { AddInfo(Error); return false; }
    TestTrue(TEXT("600 mm2 area preserved"), FMath::IsNearlyEqual(Area(Mesh), 600.0, 1.e-6));
    TestTrue(TEXT("Interior vertices generated for simulation"), Mesh.RestMm.Num() > 4);
    TestEqual(TEXT("Rest/initial correspondence"), Mesh.RestMm.Num(), Mesh.InitialCm.Num());
    TestEqual(TEXT("20mm converted to 2cm once"), Mesh.InitialCm[1], FVector3d(3,2,7));
    for (auto T : Mesh.Triangles)
        for (int K=0; K<3; ++K)
            TestTrue(TEXT("Requested maximum edge length"), (Mesh.RestMm[T[K]]-Mesh.RestMm[T[(K+1)%3]]).Length() <= 3.000001);
    TestEqual(TEXT("Original edges retained"), Mesh.BoundaryVertices.Num(), 4);
    FPanelMesh Again;
    TestTrue(TEXT("Repeat generates"), BuildPanel(Panel, Again, Error));
    TestTrue(TEXT("Deterministic topology"), Mesh.Triangles == Again.Triangles && Mesh.RestMm == Again.RestMm);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGPanelConcave, "LaceGarment.Geometry.ConcaveWinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGPanelConcave::RunTest(const FString&)
{
    FPanel Panel;
    Panel.BoundaryMm = {{0,0}, {0,30}, {10,30}, {10,10}, {20,10}, {20,0}};
    FPanelMesh Mesh;
    FString Error;
    if (!TestTrue(TEXT("Clockwise concave panel generates"), BuildPanel(Panel, Mesh, Error))) return false;
    TestTrue(TEXT("L shape area 400mm2 and positive winding"), FMath::IsNearlyEqual(Area(Mesh), 400.0, 1.e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGPanelReject, "LaceGarment.Geometry.InvalidInputPreservesOutput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGPanelReject::RunTest(const FString&)
{
    FPanel Panel;
    FPanelMesh Mesh;
    Mesh.RestMm.Add({123,456});
    FString Error;
    for (const TArray<FVector2d>& Boundary : TArray<TArray<FVector2d>>{
        {{0,0},{20,30},{20,0},{0,30}}, {{0,0},{1,0},{2,0}}, {{0,0},{20,0},{20,0},{0,30}}, {{0,0},{1,0}}})
    {
        Panel.BoundaryMm = Boundary;
        TestFalse(TEXT("Invalid loop rejected"), BuildPanel(Panel, Mesh, Error));
        TestFalse(TEXT("Reason available"), Error.IsEmpty());
        TestEqual(TEXT("Failed build keeps prior result"), Mesh.RestMm[0], FVector2d(123,456));
    }
    Panel.BoundaryMm = {{0,0},{20,0},{20,30},{0,30}};
    Panel.MaxEdgeMm = 0;
    TestFalse(TEXT("Zero spacing rejected"), BuildPanel(Panel, Mesh, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGPanelBudget, "LaceGarment.Geometry.RefinementBudget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGPanelBudget::RunTest(const FString&)
{
    FPanel Panel;
    Panel.BoundaryMm={{0,0},{20,0},{20,30},{0,30}};
    Panel.MaxEdgeMm=0.2;
    FPanelMesh Mesh;
    FString Error;
    TestFalse(TEXT("A final refinement cannot exceed the 32768 triangle budget"),BuildPanel(Panel,Mesh,Error));
    TestTrue(TEXT("Oversized result never published"),Mesh.Triangles.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGPanelPlacementRange, "LaceGarment.Geometry.PlacementRange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGPanelPlacementRange::RunTest(const FString&)
{
    FPanel Panel;
    Panel.BoundaryMm={{0,0},{20,0},{20,30},{0,30}};
    Panel.InitialPlacementCm.SetTranslation(FVector(0,0,1.e100));
    FPanelMesh Mesh;
    FString Error;
    TestFalse(TEXT("Finite double beyond Cloth float space is rejected"),BuildPanel(Panel,Mesh,Error));
    TestTrue(TEXT("Invalid placement publishes no geometry"),Mesh.InitialCm.IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGAdaptiveBoundary, "LaceGarment.Geometry.LocalBoundaryRefinement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGAdaptiveBoundary::RunTest(const FString&)
{
    FPanel A; A.BoundaryMm={{0,0},{20,0},{20,30},{0,30}}; A.MaxEdgeMm=3;
    FPanel B=A; B.BoundaryMm={{0,0},{20,0},{20,60},{0,60}};
    FPanelMesh MA,MB; FString Error;
    if(!TestTrue(TEXT("First panel"),BuildPanel(A,MA,Error)) || !TestTrue(TEXT("Taller panel"),BuildPanel(B,MB,Error))) return false;
    TestEqual(TEXT("Same seam sampling is independent of the other panel edges"),MA.BoundaryVertices[0].Num(),MB.BoundaryVertices[0].Num());
    TestEqual(TEXT("20mm edge at 3mm resolution needs eight dyadic segments"),MA.BoundaryVertices[0].Num(),9);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGDenseCurveBoundary, "LaceGarment.Geometry.DenseCurvedBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGDenseCurveBoundary::RunTest(const FString&)
{
    FPanel Panel; Panel.MaxEdgeMm=2;
    for(int32 I=0; I<128; ++I) { const double A=2*PI*I/128; Panel.BoundaryMm.Add(FVector2d(20*FMath::Cos(A),20*FMath::Sin(A))); }
    FPanelMesh Mesh; FString Error;
    if(!TestTrue(TEXT("Sampled curve can be triangulated within the local refinement budget"),BuildPanel(Panel,Mesh,Error))) { AddInfo(Error); return false; }
    TestTrue(TEXT("Refinement scales with surface resolution"),Mesh.Triangles.Num()<10000);
    for(const auto& Edge : Mesh.BoundaryVertices) TestEqual(TEXT("Sub-resolution authored boundary samples are retained"),Edge.Num(),2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCollinearBoundary, "LaceGarment.Geometry.CollinearBoundarySamples", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGCollinearBoundary::RunTest(const FString&)
{
    FPanel Panel; Panel.MaxEdgeMm=3;
    for(int32 X=0; X<20; ++X) Panel.BoundaryMm.Add(FVector2d(X,0));
    for(int32 Y=0; Y<30; ++Y) Panel.BoundaryMm.Add(FVector2d(20,Y));
    for(int32 X=20; X>0; --X) Panel.BoundaryMm.Add(FVector2d(X,30));
    for(int32 Y=30; Y>0; --Y) Panel.BoundaryMm.Add(FVector2d(0,Y));
    FPanelMesh Mesh; FString Error;
    if(!TestTrue(TEXT("Dense straight edge samples remain usable seam vertices"),BuildPanel(Panel,Mesh,Error))) { AddInfo(Error); return false; }
    TestTrue(TEXT("Sampled rectangle area preserved"),FMath::IsNearlyEqual(Area(Mesh),600.0,1.e-6));
    TSet<int32> Used;
    for(auto T : Mesh.Triangles) { Used.Add(T.X); Used.Add(T.Y); Used.Add(T.Z); }
    for(int32 I=0; I<Panel.BoundaryMm.Num(); ++I) TestTrue(TEXT("Every authored boundary sample participates in a face"),Used.Contains(I));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGPanelHoles, "LaceGarment.Geometry.InnerLoops", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGPanelHoles::RunTest(const FString&)
{
    FPanel Panel; Panel.BoundaryMm={{0,0},{40,0},{40,40},{0,40},{10,10},{20,10},{20,20},{10,20}};
    Panel.LoopStarts={0,4}; FPanelMesh Mesh; FString Error;
    if(!TestTrue(TEXT("Panel with a cutout generates"),BuildPanel(Panel,Mesh,Error))) { AddInfo(Error); return false; }
    TestTrue(TEXT("Hole area is removed"),FMath::IsNearlyEqual(Area(Mesh),1500.0,1.e-6));
    TestEqual(TEXT("Outer and inner boundary edges preserve provenance"),Mesh.BoundaryVertices.Num(),8);
    TestEqual(TEXT("Outer final edge closes to outer first vertex"),Mesh.BoundaryVertices[3].Last(),0);
    TestEqual(TEXT("Hole final edge closes to hole first vertex"),Mesh.BoundaryVertices[7].Last(),4);
    for(auto T : Mesh.Triangles)
    {
        auto Center=(Mesh.RestMm[T.X]+Mesh.RestMm[T.Y]+Mesh.RestMm[T.Z])/3;
        TestFalse(TEXT("No triangle fills the cutout"),Center.X>10 && Center.X<20 && Center.Y>10 && Center.Y<20);
    }
    Swap(Panel.BoundaryMm[5],Panel.BoundaryMm[7]);
    TestTrue(TEXT("Hole winding does not alter its meaning"),BuildPanel(Panel,Mesh,Error));
    TestTrue(TEXT("Reversed hole area stays excluded"),FMath::IsNearlyEqual(Area(Mesh),1500.0,1.e-6));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGInvalidHoles, "LaceGarment.Geometry.InvalidInnerLoops", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGInvalidHoles::RunTest(const FString&)
{
    FPanel Panel; Panel.BoundaryMm={{0,0},{40,0},{40,40},{0,40},{50,10},{60,10},{60,20},{50,20}};
    Panel.LoopStarts={0,4}; FPanelMesh Mesh; FString Error;
    TestFalse(TEXT("Hole outside the outer loop rejected"),BuildPanel(Panel,Mesh,Error));
    Panel.BoundaryMm={{0,0},{40,0},{40,40},{0,40},{0,10},{10,10},{10,20},{0,20}};
    TestFalse(TEXT("Touching cutout rejected"),BuildPanel(Panel,Mesh,Error));
    Panel.BoundaryMm={{0,0},{40,0},{40,40},{0,40},{10,10},{30,10},{30,30},{10,30},{15,15},{20,15},{20,20},{15,20}};
    Panel.LoopStarts={0,4,8};
    TestFalse(TEXT("Nested holes are not silently converted to islands"),BuildPanel(Panel,Mesh,Error));
    Panel.LoopStarts={0,8,4};
    TestFalse(TEXT("Unordered loop offsets rejected before indexing"),BuildPanel(Panel,Mesh,Error));
    return true;
}
