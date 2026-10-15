#include "LaceGarmentCorrections.h"
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"
using namespace LaceGarment;
namespace
{
ULaceGarmentDocument* CorrectionFixture()
{
    auto* D=NewObject<ULaceGarmentDocument>();
    for(int32 I=0;I<2;++I)
    {
        FLGPiece P;P.Id=FGuid::NewGuid();P.Name=FString::Printf(TEXT("Panel%d"),I);
        P.InitialPlacementCm.SetTranslation(FVector(I*2,0,8));
        for(FVector2D V : {FVector2D(0,0),FVector2D(20,0),FVector2D(20,30),FVector2D(0,30)})
        {FLGControlPoint C;C.Id=FGuid::NewGuid();C.PositionMm=V;P.Points.Add(C);}
        for(int32 K=0;K<4;++K)
        {FLGEdge E;E.Id=FGuid::NewGuid();E.Start=P.Points[K].Id;E.End=P.Points[(K+1)%4].Id;P.Edges.Add(E);P.Outer.Edges.Add(E.Id);}
        D->Pieces.Add(P);
    }
    FLGSeam S;S.Id=FGuid::NewGuid();S.A.Piece=D->Pieces[0].Id;S.A.Edge=D->Pieces[0].Edges[1].Id;
    S.B.Piece=D->Pieces[1].Id;S.B.Edge=D->Pieces[1].Edges[3].Id;D->Seams.Add(S);
    for(int32 Point : {0,3}) {FLGAnchor A;A.Id=FGuid::NewGuid();A.Piece=D->Pieces[0].Id;A.Point=D->Pieces[0].Points[Point].Id;D->Anchors.Add(A);}
    return D;
}
bool BuildFixture(ULaceGarmentDocument& D,FBuildResult& Build,FString& Error)
{
    FSampledDocument S;if(!SampleDocument(D,S,Error)) return false;
    S.Input.GravityCm=0;
    if(!BuildGarment(S.Input,Build,Error)) return false;
    return TransferBodySkin(Build,LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11")),Error,EBodySkinTransfer::SurfaceVertex);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCorrectionReapply,"LaceGarment.Document.ReapplyGeneratedCorrections",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGCorrectionReapply::RunTest(const FString&)
{
    auto* D=CorrectionFixture();FBuildResult Build;FString Error;
    if(!TestTrue(TEXT("Generate actual skinned joined panels"),BuildFixture(*D,Build,Error))) {AddInfo(Error);return false;}
    FGeneratedCorrespondence Map;
    if(!TestTrue(TEXT("Generate stable correspondence"),MakeGeneratedCorrespondence(*D,Build,Map,Error))) {AddInfo(Error);return false;}
    const auto Seam=Build.Stitches[Build.Stitches.Num()/2];
    TestTrue(TEXT("Welded vertices share one simulation ID"),Map.SimulationVertexIds[Seam.X]==Map.SimulationVertexIds[Seam.Y]);
    FLGCorrection C;C.Id=FGuid::NewGuid();C.Target=Map.SimulationVertexIds[Seam.X];C.TopologyHash=Map.TopologyHash;C.Property=TEXT("maxDistance");C.Value=.42;
    D->Corrections.Add(C);
    FLGCorrection W;W.Id=FGuid::NewGuid();W.Target=C.Target;W.TopologyHash=C.TopologyHash;FLGJointWeight J;J.Joint=Map.JointIds[0];J.Weight=1;W.Skin.Add(J);D->Corrections.Add(W);
    if(!TestTrue(TEXT("Apply manual map and skin over generated defaults"),ApplyDocumentCorrections(*D,Build,Error))) {AddInfo(Error);return false;}
    TestFalse(TEXT("Applied source corrections resolved"),D->HasUnresolvedCorrections());
    TestEqual(TEXT("Manual source remains stored"),D->Corrections.Num(),2);
    TestTrue(TEXT("Skin correction propagates through the weld"),Build.SkinJoints[Seam.X]==TArray<int32>{0} && Build.SkinJoints[Seam.Y]==TArray<int32>{0});
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/LaceGarment/Development/M_NativeCloth.M_NativeCloth"));
    auto Collection=MakeClothCollection(Build,Material,Error);
    if(!TestTrue(TEXT("Corrected build converts to real Cloth collection"),Collection.IsValid())) {AddInfo(Error);return false;}
    UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Collection.ToSharedRef());
    TestTrue(TEXT("Manual maxDistance reaches actual simulation map"),FMath::IsNearlyEqual(Cloth.GetWeightMap(TEXT("MaxDistance"))[Cloth.GetSimVertex3DLookup()[Seam.X]],.42f));
    FBuildResult Again;if(!TestTrue(TEXT("Regenerate automatic result"),BuildFixture(*D,Again,Error))) return false;
    FGeneratedCorrespondence AgainMap;
    TestTrue(TEXT("Recreate mapping"),MakeGeneratedCorrespondence(*D,Again,AgainMap,Error));
    TestTrue(TEXT("Same source has deterministic IDs and topology"),AgainMap.TopologyHash==Map.TopologyHash && AgainMap.SimulationVertexIds==Map.SimulationVertexIds && AgainMap.JointIds==Map.JointIds);
    TestTrue(TEXT("Manual source reapplies after regeneration"),ApplyDocumentCorrections(*D,Again,Error));
    D->MeshMaxEdgeMm=1.5; D->InvalidateDerived();
    FBuildResult Changed;if(!TestTrue(TEXT("Generate changed topology"),BuildFixture(*D,Changed,Error))) return false;
    const auto SavedSkin=Changed.SkinJoints;
    TestFalse(TEXT("Changed topology cannot reuse old correction"),ApplyDocumentCorrections(*D,Changed,Error));
    TestTrue(TEXT("Unresolved manual source is preserved"),D->HasUnresolvedCorrections() && D->Corrections.Num()==2);
    TestTrue(TEXT("Failed reapplication leaves generated skin untouched"),Changed.SkinJoints==SavedSkin);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCorrectionFailures,"LaceGarment.Document.InvalidGeneratedCorrections",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGCorrectionFailures::RunTest(const FString&)
{
    auto* D=CorrectionFixture();FBuildResult Original;FGeneratedCorrespondence Map;FString Error;
    if(!TestTrue(TEXT("Build correction failure fixture"),BuildFixture(*D,Original,Error) && MakeGeneratedCorrespondence(*D,Original,Map,Error))) return false;
    const int32 Vertex=Original.Stitches[Original.Stitches.Num()/2].X;
    FLGCorrection Base;Base.Id=FGuid::NewGuid();Base.Target=Map.SimulationVertexIds[Vertex];Base.TopologyHash=Map.TopologyHash;Base.Property=TEXT("maxDistance");Base.Value=.42;
    auto Reject=[&](const TCHAR* Label,TArray<FLGCorrection> Source)
    {
        D->Corrections=Source;FBuildResult Attempt=Original;
        TestFalse(Label,ApplyDocumentCorrections(*D,Attempt,Error));
        TestTrue(TEXT("Failure preserves generated maps and source corrections"),Attempt.SkinJoints==Original.SkinJoints && Attempt.SkinWeights==Original.SkinWeights && Attempt.MaxDistancesCm==Original.MaxDistancesCm && D->Corrections.Num()==Source.Num());
        TestTrue(TEXT("Failure is explicitly unresolved"),D->HasUnresolvedCorrections() && !D->bDerivedCurrent);
    };
    auto Bad=Base;Bad.Target=FGuid::NewGuid();Reject(TEXT("Missing generated target rejected"),{Bad});
    Bad=Base;Bad.Property=TEXT("unsupportedMap");Reject(TEXT("Unsupported map rejected"),{Bad});
    Bad=Base;Bad.Target=Map.SimulationVertexIds[*Original.PinnedVertices.CreateConstIterator()];Reject(TEXT("Authored anchor cannot be released"),{Bad});
    Bad=Base;Bad.Id=FGuid::NewGuid();Reject(TEXT("Duplicate map correction is not last-writer-wins"),{Base,Bad});
    Bad=Base;Bad.Property.Reset();FLGJointWeight W;W.Joint=FGuid::NewGuid();W.Weight=1;Bad.Skin={W};Reject(TEXT("Missing joint rejected"),{Bad});
    W.Joint=Map.JointIds[0];W.Weight=1.e-300;Bad.Skin={W};W.Joint=Map.JointIds[1];W.Weight=1;Bad.Skin.Add(W);
    Reject(TEXT("Positive source weight that becomes float zero rejected"),{Bad});
    D->Corrections={Base};FBuildResult Stale=Original;Stale.Stitches.Pop();
    TestFalse(TEXT("Stale generated sewing cannot accept new correspondence"),ApplyDocumentCorrections(*D,Stale,Error));
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/LaceGarment/Development/M_NativeCloth.M_NativeCloth"));
    FBuildResult Malformed=Original;Malformed.MaxDistancesCm={.1f};
    TestFalse(TEXT("Incomplete map rejected at Cloth boundary"),MakeClothCollection(Malformed,Material,Error).IsValid());
    Malformed.MaxDistancesCm.Init(1,Original.SettledCm.Num());
    TestFalse(TEXT("Map cannot silently overwrite pins"),MakeClothCollection(Malformed,Material,Error).IsValid());
    return true;
}
