#include "LaceGarmentDocument.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "ScopedTransaction.h"

namespace
{
ULaceGarmentDocument* DocumentFixture()
{
    auto* Doc=NewObject<ULaceGarmentDocument>(); Doc->GarmentId=FGuid::NewGuid(); Doc->PatternId=FGuid::NewGuid();
    FLGPiece Piece; Piece.Id=FGuid::NewGuid(); Piece.Name=TEXT("Panel");
    for(FVector2D P : TArray<FVector2D>{{0,0},{20,0},{20,30},{0,30},{5,10},{15,10}})
    { FLGControlPoint Point; Point.Id=FGuid::NewGuid(); Point.PositionMm=P; Piece.Points.Add(Point); }
    for(int32 I=0; I<4; ++I)
    {
        FLGEdge Edge; Edge.Id=FGuid::NewGuid(); Edge.Start=Piece.Points[I].Id; Edge.End=Piece.Points[(I+1)%4].Id;
        Piece.Edges.Add(Edge); Piece.Outer.Edges.Add(Edge.Id);
    }
    Doc->Pieces.Add(Piece); return Doc;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGDocumentCurves,"LaceGarment.Document.CurvesAndReferences",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGDocumentCurves::RunTest(const FString&)
{
    auto* Doc=DocumentFixture(); FString Error; FVector2D P;
    if(!TestTrue(TEXT("Valid source document"),Doc->ValidateStructure(Error))) { AddInfo(Error); return false; }
    auto& Piece=Doc->Pieces[0]; auto& Edge=Piece.Edges[0];
    TestTrue(TEXT("Evaluate authored line"),Doc->EvaluateEdge(Piece.Id,Edge.Id,0.25,P,Error));
    TestTrue(TEXT("Line parameter in mm"),P.Equals(FVector2D(5,0),1.e-10));
    Edge.Curve=ELGCurveType::Cubic; Edge.ControlA=Piece.Points[4].Id; Edge.ControlB=Piece.Points[5].Id;
    TestTrue(TEXT("Evaluate cubic source"),Doc->EvaluateEdge(Piece.Id,Edge.Id,0.5,P,Error));
    TestTrue(TEXT("Cubic control handles affect source shape"),P.Equals(FVector2D(10,7.5),1.e-10));
    Edge.ControlA=FGuid::NewGuid();
    TestFalse(TEXT("Unknown control point is a structural error even in a draft"),Doc->ValidateStructure(Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGDocumentTopology,"LaceGarment.Document.CorrectionTopology",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGDocumentTopology::RunTest(const FString&)
{
    auto* Doc=DocumentFixture(); FString Error;
    const FString A=FString::ChrN(64,TEXT('a')), B=FString::ChrN(64,TEXT('b'));
    FLGCorrection C; C.Id=FGuid::NewGuid(); C.Target=FGuid::NewGuid(); C.TopologyHash=A; C.Property=TEXT("maxDistance"); C.Value=1;
    Doc->Corrections.Add(C);
    TestTrue(TEXT("Apply source topology"),Doc->ApplyGeneratedTopology(A,Error));
    TestFalse(TEXT("Matching correction resolved"),Doc->HasUnresolvedCorrections());
    Doc->InvalidateDerived();
    TestFalse(TEXT("Edits invalidate derived cache"),Doc->bDerivedCurrent);
    TestTrue(TEXT("Correction waits for regenerated topology"),Doc->HasUnresolvedCorrections());
    TestTrue(TEXT("New topology is recorded"),Doc->ApplyGeneratedTopology(B,Error));
    TestTrue(TEXT("Old correction remains unresolved"),Doc->HasUnresolvedCorrections());
    TestEqual(TEXT("Correction is not silently deleted"),Doc->Corrections.Num(),1);
    TestEqual(TEXT("Original correction topology preserved"),Doc->Corrections[0].TopologyHash,A);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGDocumentUndo,"LaceGarment.Document.PointUndoRedo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGDocumentUndo::RunTest(const FString&)
{
    auto* Doc=DocumentFixture(); Doc->AddToRoot(); FString Error;
    const auto Piece=Doc->Pieces[0].Id,Point=Doc->Pieces[0].Points[0].Id;
    {
        FScopedTransaction Transaction(FText::FromString(TEXT("LaceGarment move point test")));
        TestTrue(TEXT("Edit point through document API"),Doc->MovePoint(Piece,Point,FVector2D(2,3),Error));
    }
    TestTrue(TEXT("Source edited"),Doc->Pieces[0].Points[0].PositionMm.Equals(FVector2D(2,3)));
    GEditor->UndoTransaction();
    TestTrue(TEXT("Undo restores source"),Doc->Pieces[0].Points[0].PositionMm.Equals(FVector2D(0,0)));
    GEditor->RedoTransaction();
    TestTrue(TEXT("Redo restores edit"),Doc->Pieces[0].Points[0].PositionMm.Equals(FVector2D(2,3)));
    TestFalse(TEXT("Undo/redo cannot revive stale derived data"),Doc->bDerivedCurrent);
    Doc->RemoveFromRoot(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGDocumentSeams,"LaceGarment.Document.SeamReferencesAndCycles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGDocumentSeams::RunTest(const FString&)
{
    auto* Doc=DocumentFixture(); FString Error;
    FLGSeam Seam; Seam.Id=FGuid::NewGuid();
    Seam.A.Piece=Doc->Pieces[0].Id; Seam.A.Edge=Doc->Pieces[0].Edges[0].Id;
    Seam.B.Piece=Doc->Pieces[0].Id; Seam.B.Edge=Doc->Pieces[0].Edges[2].Id;
    Doc->Seams.Add(Seam);
    TestTrue(TEXT("Explicit seam intervals structurally valid"),Doc->ValidateStructure(Error));
    Doc->Seams[0].A.From=-0.1;
    TestFalse(TEXT("Invalid curve interval blocked"),Doc->ValidateStructure(Error));
    Doc->Seams[0].A.From=0;
    FLGSeam Other=Seam; Other.Id=FGuid::NewGuid(); Other.Prerequisites={Seam.Id};
    Doc->Seams[0].Prerequisites={Other.Id}; Doc->Seams.Add(Other);
    TestFalse(TEXT("Cyclic sewing order blocked"),Doc->ValidateStructure(Error));
    TestTrue(TEXT("Cycle identified explicitly"),Error.Contains(TEXT("cycle")));
    Doc->Seams.SetNum(1); Doc->Seams[0].Prerequisites.Reset();
    Doc->Seams[0].Type=ELGSeamType::Hem; Doc->Seams[0].B=FLGEdgeInterval(); Doc->Seams[0].FoldWidthMm=2;
    TestTrue(TEXT("Hem requires one edge rather than a fabricated pair"),Doc->ValidateStructure(Error));
    return true;
}
