#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "Dataflow/DataflowInstance.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
ULaceLoftPatternDefinition* Fixture()
{
    auto* P=NewObject<ULaceLoftPatternDefinition>();
    P->PatternId=TEXT("Fixture");P->DummyMeasurementsVersion=TEXT("dummy-1");P->SourceHash=TEXT("fixture");
    for(const FName Id:{FName(TEXT("front")),FName(TEXT("back"))})
    {
        FLaceLoftPatternPiece Piece;Piece.PieceId=Id;
        Piece.CutOutline.PathId=FName(Id.ToString()+TEXT(".cut"));Piece.CutOutline.bClosed=true;
        Piece.CutOutline.PointsMm={FVector2D(0,0),FVector2D(10,0),FVector2D(0,10),FVector2D(0,0)};
        FLaceLoftPatternEdge Edge;Edge.EdgeId=TEXT("side");Edge.Path.PathId=FName(Id.ToString()+TEXT(".side"));
        Edge.Path.PointsMm={FVector2D(0,0),FVector2D(10,0)};Edge.StitchVertices={0,1};Piece.Edges.Add(Edge);
        Piece.SimPositionsMm={FVector2D(0,0),FVector2D(10,0),FVector2D(0,10)};
        Piece.AssembledPositionsMm={FVector(0,0,0),FVector(10,0,0),FVector(0,10,0)};
        Piece.TriangleIndices={0,1,2};Piece.MaxDistanceMm={0,10,10};Piece.AnchorBones={NAME_None,NAME_None,NAME_None};
        P->Pieces.Add(Piece);
    }
    FLaceLoftSeamOperation Seam;Seam.SeamId=TEXT("join");
    FLaceLoftEdgeReference A;A.PieceId=TEXT("front");A.EdgeId=TEXT("side");
    FLaceLoftEdgeReference B=A;B.PieceId=TEXT("back");Seam.SideA={A};Seam.SideB={B};P->Seams={Seam};
    return P;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftPatternRulesTest,"LaceLoft.Patterns.InvalidDefinitions",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftPatternRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Valid sewn pattern accepted"),ValidatePattern(*Fixture()).Num(),0);
    auto* Duplicate=Fixture();const auto DuplicatePiece=Duplicate->Pieces[0];Duplicate->Pieces.Add(DuplicatePiece);
    TestTrue(TEXT("Duplicate PieceId rejected"),!ValidatePattern(*Duplicate).IsEmpty());
    auto* Missing=Fixture();Missing->Seams[0].SideB[0].EdgeId=TEXT("missing");
    TestTrue(TEXT("Missing EdgeId rejected"),!ValidatePattern(*Missing).IsEmpty());
    auto* Direction=Fixture();Direction->Seams[0].SideB[0].Direction=-1;
    TestTrue(TEXT("Wrong guided seam direction rejected"),!ValidatePattern(*Direction).IsEmpty());
    auto* Ratio=Fixture();Ratio->Pieces[1].Edges[0].Path.PointsMm[1].X=16;
    TestTrue(TEXT("Undeclared length mismatch rejected"),!ValidatePattern(*Ratio).IsEmpty());
    auto* Cycle=Fixture();auto Second=Cycle->Seams[0];Second.SeamId=TEXT("next");Second.Prerequisites={TEXT("join")};
    Cycle->Seams[0].Prerequisites={TEXT("next")};Cycle->Seams.Add(Second);
    TestTrue(TEXT("Dependency cycle rejected"),!ValidatePattern(*Cycle).IsEmpty());
    auto* Opening=Fixture();Opening->Pieces[0].Edges[0].bOpening=true;
    TestTrue(TEXT("Join cannot close a garment opening"),!ValidatePattern(*Opening).IsEmpty());
    auto* Elastic=Fixture();Elastic->Pieces[1].Edges[0].Path.PointsMm[1].X=9;
    Elastic->Pieces[1].SimPositionsMm[1].X=9;
    Elastic->Seams[0].Kind=ELaceLoftSeamKind::ElasticBand;Elastic->Seams[0].EaseRatio=.9;
    TestEqual(TEXT("Declared elastic length ratio accepted"),ValidatePattern(*Elastic).Num(),0);
    auto* BadIndex=Fixture();BadIndex->Pieces[0].TriangleIndices[0]=999;
    TestTrue(TEXT("3D panel index outside common source rejected"),!ValidatePattern(*BadIndex).IsEmpty());
    auto* WrongMap=Fixture();WrongMap->Pieces[0].Edges[0].StitchVertices={0,2};
    TestTrue(TEXT("In-range mapping must follow the actual UI edge"),!ValidatePattern(*WrongMap).IsEmpty());
    auto* SelfJoin=Fixture();SelfJoin->Seams[0].SideB=SelfJoin->Seams[0].SideA;
    TestTrue(TEXT("Cannot sew an edge to itself"),!ValidatePattern(*SelfJoin).IsEmpty());
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftPatternAssetsTest,"LaceLoft.Patterns.AuthoredGarments",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaceLoftPatternAssetsTest::RunTest(const FString& Parameters)
{
    FString MeasurementSource;TSharedPtr<FJsonObject> Measurements;
    if(!FFileHelper::LoadFileToString(MeasurementSource,*(FPaths::ProjectDir()/TEXT("ArtSource/Dummy/Measurements.json")))
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MeasurementSource),Measurements))return false;
    for(const FString Id:{TEXT("TShirt"),TEXT("DolphinShorts")})
    {
        const FString AssetPath=TEXT("/Game/Garments/Patterns/DA_")+Id+TEXT(".DA_")+Id;
        const auto* Pattern=LoadObject<ULaceLoftPatternDefinition>(nullptr,*AssetPath);
        if(!TestNotNull(Id+TEXT(" actual pattern asset"),Pattern))continue;
        TestEqual(TEXT("Pattern version"),Pattern->PatternVersion,1);
        TestEqual(TEXT("Final measured body version"),Pattern->DummyMeasurementsVersion,Measurements->GetStringField(TEXT("version")));
        const FString Filename=FPaths::ProjectDir()/TEXT("ArtSource/Patterns/")+Id+TEXT(".json");
        TestEqual(TEXT("Source hash is current"),Pattern->SourceHash,LexToString(FMD5Hash::HashFile(*Filename)));
        for(const auto& Error:ValidatePattern(*Pattern))AddError(Error);
        TestEqual(TEXT("Required piece count"),Pattern->Pieces.Num(),Id==TEXT("TShirt")?5:7);
        auto* Asset=Pattern->ClothAsset.LoadSynchronous();
        if(!TestNotNull(TEXT("Saved final cloth reference"),Asset))continue;
        TestNotNull(TEXT("Editor Dataflow retained"),Asset->GetDataflowInstance().GetDataflowAsset());
        TestNotNull(TEXT("Collision asset retained"),Asset->GetPhysicsAsset());
        TestTrue(TEXT("Shared full skeleton"),Asset->GetRefSkeleton().GetNum()>=20);
        if(!TestEqual(TEXT("One collection"),Asset->GetClothCollections().Num(),1))continue;
        UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Asset->GetClothCollections()[0]);
        TestEqual(TEXT("Piece to simulation panel mapping"),Cloth.GetNumSimPatterns(),Pattern->Pieces.Num());
        int32 Vertices=0,Faces=0;
        for(int32 I=0;I<Pattern->Pieces.Num();++I)
        {
            const auto& Piece=Pattern->Pieces[I];Vertices+=Piece.SimPositionsMm.Num();Faces+=Piece.TriangleIndices.Num()/3;
            TestEqual(TEXT("Piece-local vertex mapping"),Cloth.GetSimPattern(I).GetNumSimVertices2D(),Piece.SimPositionsMm.Num());
            const auto Positions=Cloth.GetSimPattern(I).GetSimPosition2D();
            for(int32 J=0;J<Positions.Num();++J)
                if(!FVector2D(Positions[J]).Equals(LaceLoftUnits::ToCm(Piece.SimPositionsMm[J]),.0001))
                {AddError(TEXT("Source mm to actual cloth cm changed"));break;}
        }
        TestEqual(TEXT("Render vertices retain source panel correspondence"),Cloth.GetNumRenderVertices(),Vertices);
        TestEqual(TEXT("Source triangles retained"),Cloth.GetNumSimFaces(),Faces);
        int32 Moving=0,Fixed=0,InvalidWeights=0,InvalidUV=0;
        for(float Distance:Cloth.GetWeightMap(TEXT("MaxDistance"))){Moving+=Distance>=.1f;Fixed+=Distance==0;}
        for(const auto& Weights:Cloth.GetRenderBoneWeights())
        {float Sum=0;for(float W:Weights)Sum+=W;InvalidWeights+=!FMath::IsNearlyEqual(Sum,1.f,.0001f);}
        for(const auto& Channels:Cloth.GetRenderUVs())
            InvalidUV+=Channels.IsEmpty() || Channels[0].ContainsNaN() || Channels[0].X<0 || Channels[0].X>1 || Channels[0].Y<0 || Channels[0].Y>1;
        TestTrue(TEXT("Anchored and dynamic fabric"),Fixed>0 && Moving>100);
        TestEqual(TEXT("Normalized render weights"),InvalidWeights,0);TestEqual(TEXT("Valid UV0"),InvalidUV,0);
    }
    return !HasAnyErrors();
}
#endif
