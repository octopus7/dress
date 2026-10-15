#include "LaceGarmentPipeline.h"
#include "Misc/AutomationTest.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Materials/Material.h"
#include "Engine/SkeletalMesh.h"
#include "LaceGarmentCollisionProjection.h"
#include <limits>

using namespace LaceGarment;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCollisionSweep, "LaceGarment.Pipeline.PreventBodyTunneling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGCollisionSweep::RunTest(const FString&)
{
    const TArray<FCapsule> Body={{{0,0,-2},{0,0,2},1}};
    const auto Through=SweepBodyCollision({-2,0,0},{2,0,0},Body,.03);
    TestTrue(TEXT("A step with outside endpoints cannot tunnel through the body"),Through.X<-1.0299);
    const auto Inside=SweepBodyCollision({-2,0,0},{.5,0,0},Body,.03);
    TestTrue(TEXT("An interior target remains on the incoming side"),Inside.X<-1.0299);
    const auto Slide=SweepBodyCollision({-1.0301,0,0},{-.8,.3,0},Body,.03);
    TestTrue(TEXT("Collision retains tangential movement"),Slide.Y>.15);
    TestTrue(TEXT("Sliding does not penetrate"),Slide.X*Slide.X+Slide.Y*Slide.Y>=1.03*1.03-1.e-7);
    TestTrue(TEXT("A clear step is unchanged"),SweepBodyCollision({2,0,0},{2,1,0},Body,.03).Equals(FVector3d(2,1,0),1.e-8));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCollisionUnion, "LaceGarment.Pipeline.OverlappingBodyCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGCollisionUnion::RunTest(const FString&)
{
    auto Check=[&](const TArray<FCapsule>& Capsules,FVector3d Start,double Thickness)
    {
        const auto Result=ProjectBodyCollision(Start,Capsules,Thickness);
        for(const auto& C : Capsules)
        {
            const auto D=C.B-C.A;
            const auto Q=C.A+D*FMath::Clamp(FVector3d::DotProduct(Result-C.A,D)/FMath::Max(D.SquaredLength(),1.e-12),0.0,1.0);
            TestTrue(TEXT("Projection leaves every overlapping body shape"),(Result-Q).Length()>=C.RadiusCm+Thickness-1.e-7);
        }
        TestTrue(TEXT("Projection is finite"),!Result.ContainsNaN());
        TestTrue(TEXT("Projecting an already feasible point is idempotent"),ProjectBodyCollision(Result,Capsules,Thickness).Equals(Result,1.e-7));
    };
    Check({{{-1,0,0},{-1,0,0},1.5},{{1,0,0},{1,0,0},1.5}},{0,0,0},0);
    Check({{{-1,0,-2},{-1,0,2},1.5},{{1,0,-2},{1,0,2},1.5}},{0,0,0},0.03);
    // Actual Fit-4 upper-arm/forearm overlap, previously left 0.23195cm penetration.
    Check({{{.017931586,1.466336527,8.782769246},{.037379948,2.197776109,7.700121195},.805640899},
        {{.026424707,3.144882057,7.193937466},{.038541094,3.816776920,6.409721448},.504999935}},
        {.217744709,2.719945514,7.456712125},.03);
    Check({{{0,0,-1},{0,0,1},1}}, {0,0,1},.03);
    return true;
}
namespace
{
FBuildInput FlatJoinedPanels()
{
    FBuildInput Input;
    FPanel A;
    A.BoundaryMm={{0,0},{20,0},{20,30},{0,30}};
    A.MaxEdgeMm=4;
    A.InitialPlacementCm.SetTranslation(FVector(0,0,8));
    FPanel B=A;
    B.InitialPlacementCm.SetTranslation(FVector(2.5,0,8));
    Input.Panels={A,B};
    Input.Seams.Add({0,1,1,3,true});
    Input.Pins={{0,0},{0,3}};
    Input.GravityCm=0;
    return Input;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGBendingControl, "LaceGarment.Pipeline.BendingRegularizationControl", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGBendingControl::RunTest(const FString&)
{
    FBuildResult Result;FString Error;
    for(double Strength : {-0.1,1.1,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {
        auto Input=FlatJoinedPanels();Input.BendingStrength=Strength;
        TestFalse(TEXT("Invalid regularization rejected before fitting"),BuildGarment(Input,Result,Error));
        TestTrue(TEXT("Invalid regularization creates no derived geometry"),Result.InitialCm.IsEmpty());
    }
    auto Input=FlatJoinedPanels();Input.BendingStrength=0;
    TestTrue(TEXT("Zero regularization remains a valid flat seam solve"),BuildGarment(Input,Result,Error));
    // A short partial close distinguishes the regularizer's effect without
    // treating either transient state as an accepted garment.
    Input.MaxSteps=1;FBuildResult Free,Regularized;
    BuildGarment(Input,Free,Error);Input.BendingStrength=.05;BuildGarment(Input,Regularized,Error);
    double Difference=0;
    if(TestEqual(TEXT("Comparison topology unchanged"),Free.SettledCm.Num(),Regularized.SettledCm.Num()))
        for(int32 I=0;I<Free.SettledCm.Num();++I) Difference=FMath::Max(Difference,(Free.SettledCm[I]-Regularized.SettledCm[I]).Length());
    TestTrue(TEXT("Regularization setting affects the solve"),Difference>1.e-7);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGSeamSettle, "LaceGarment.Pipeline.SeamRelaxation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGSeamSettle::RunTest(const FString&)
{
    FBuildInput Input=FlatJoinedPanels();
    FBuildResult Result;
    FString Error;
    if (!TestTrue(TEXT("Separated panels converge through explicit seam"),BuildGarment(Input,Result,Error))) { AddInfo(Error); return false; }
    TestTrue(TEXT("Convergence measured"),Result.bConverged);
    TestTrue(TEXT("Seam under 0.2mm"),Result.MaxSeamGapCm<0.02);
    TestTrue(TEXT("Edge length error under 10 percent"),Result.MaxRelativeEdgeError<0.1);
    TestTrue(TEXT("2D source preserved"),Input.Panels[1].BoundaryMm[0]==FVector2d(0,0));
    int32 B=Result.Offsets[1];
    TestEqual(TEXT("Initial placement retained"),Result.InitialCm[B],FVector3d(2.5,0,8));
    TestTrue(TEXT("Settling moves actual geometry"),(Result.InitialCm[B]-Result.SettledCm[B]).Length()>0.2);
    TestEqual(TEXT("UV per unwelded vertex"),Result.UV.Num(),Result.SettledCm.Num());
    float MaxUFirst=0, MinUSecond=1;
    for(int32 I=0; I<Result.UV.Num(); ++I)
    {
        auto UV=Result.UV[I];
        TestTrue(TEXT("Atlas margins"),UV.X>=16.f/2048-1.e-6 && UV.Y>=16.f/2048-1.e-6 && UV.X<=1-16.f/2048+1.e-6 && UV.Y<=1-16.f/2048+1.e-6);
        if (I<B) MaxUFirst=FMath::Max(MaxUFirst,UV.X); else MinUSecond=FMath::Min(MinUSecond,UV.X);
    }
    TestTrue(TEXT("32px unique island separation"),MinUSecond-MaxUFirst >= 32.f/2048-1.e-6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGClothCollection, "LaceGarment.Pipeline.ClothCollection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGClothCollection::RunTest(const FString&)
{
    FBuildResult Result;
    FString Error;
    if (!TestTrue(TEXT("Build for collection"),BuildGarment(FlatJoinedPanels(),Result,Error))) return false;
    UMaterial* Material=NewObject<UMaterial>();
    auto Collection=MakeClothCollection(Result,Material,Error);
    if (!TestTrue(TEXT("Cloth collection created"),Collection.IsValid())) { AddInfo(Error); return false; }
    UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Collection.ToSharedRef());
    TestEqual(TEXT("Both pattern sources preserved"),Cloth.GetNumSimPatterns(),2);
    TestTrue(TEXT("Actual cloth stitch topology merged"),Cloth.GetNumSimVertices3D()<Cloth.GetNumSimVertices2D());
    TestEqual(TEXT("Render preserves UV seams"),Cloth.GetNumRenderVertices(),Result.UV.Num());
    for(int32 I=0; I<Result.UV.Num(); ++I) TestEqual(TEXT("Explicit atlas replaces default projection"),Cloth.GetRenderUVs()[I][0],Result.UV[I]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGUnconverged, "LaceGarment.Pipeline.RejectUnconverged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGUnconverged::RunTest(const FString&)
{
    FBuildInput Input=FlatJoinedPanels();
    Input.MaxSteps=1;
    FBuildResult Result;
    FString Error;
    TestFalse(TEXT("A single step cannot be labelled converged"),BuildGarment(Input,Result,Error));
    TestFalse(TEXT("No validated flag"),Result.bConverged);
    TestFalse(TEXT("No cloth from incomplete drape"),MakeClothCollection(Result,NewObject<UMaterial>(),Error).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCapsuleDrape, "LaceGarment.Pipeline.DollScaleCapsuleDrape", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGCapsuleDrape::RunTest(const FString&)
{
    FBuildInput Input;
    FPanel Front;
    Front.BoundaryMm={{0,0},{40,0},{40,30},{20,32},{0,30}};
    Front.MaxEdgeMm=5;
    Front.InitialPlacementCm=FTransform(FRotator(0,90,-90),FVector(1.2,-2,7));
    FPanel Back=Front;
    Back.InitialPlacementCm=FTransform(FRotator(0,-90,-90),FVector(-1.2,2,7));
    Input.Panels={Front,Back};
    Input.Seams={{0,1,1,4,true},{0,4,1,1,true}};
    Input.Pins={{0,3},{1,3}};
    Input.Colliders.Add({FVector3d(0,0,6.5),FVector3d(0,0,10.5),1.0});
    FBuildResult Result;
    FString Error;
    if(!TestTrue(TEXT("Drape around a 2cm torso capsule"),BuildGarment(Input,Result,Error))) { AddInfo(Error); return false; }
    AddInfo(FString::Printf(TEXT("steps=%d speed=%.5f seam=%.5f penetration=%.5f strain=%.5f"),Result.Steps,Result.MaxSpeedCmPerSec,Result.MaxSeamGapCm,Result.MaxPenetrationCm,Result.MaxRelativeEdgeError));
    TestTrue(TEXT("Collision clearance"),Result.MaxPenetrationCm<0.005);
    TestTrue(TEXT("Side seams closed"),Result.MaxSeamGapCm<0.02);
    double MaxFoldDegrees=0;
    for(int32 PanelIndex=0; PanelIndex<Result.Panels.Num(); ++PanelIndex)
    {
        const auto& Panel=Result.Panels[PanelIndex];
        TMap<uint64,FVector3d> EdgeNormal;
        for(auto T : Panel.Triangles)
        {
            const int32 Offset=Result.Offsets[PanelIndex];
            const FVector3d A=Result.SettledCm[Offset+T.X],B=Result.SettledCm[Offset+T.Y],C=Result.SettledCm[Offset+T.Z];
            const FVector3d Normal=FVector3d::CrossProduct(B-A,C-A).GetSafeNormal();
            for(int32 K=0; K<3; ++K)
            {
                int32 X=T[K],Y=T[(K+1)%3];
                const uint64 Key=(uint64(FMath::Min(X,Y))<<32)|uint32(FMath::Max(X,Y));
                if(const FVector3d* Other=EdgeNormal.Find(Key))
                    MaxFoldDegrees=FMath::Max(MaxFoldDegrees,FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector3d::DotProduct(Normal,*Other),-1.0,1.0))));
                else EdgeNormal.Add(Key,Normal);
            }
        }
    }
    AddInfo(FString::Printf(TEXT("max internal panel fold %.3f degrees"),MaxFoldDegrees));
    TestTrue(TEXT("Capsule fixture does not collapse into near-reversed adjacent faces"),MaxFoldDegrees<120);
    Input.Panels[0].InitialPlacementCm.SetTranslation(FVector(0,-2,7));
    TestFalse(TEXT("Body-penetrating initial placement blocked"),BuildGarment(Input,Result,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGMalformedBuild, "LaceGarment.Pipeline.MalformedBuildRejected", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGMalformedBuild::RunTest(const FString&)
{
    FBuildResult Result;
    Result.bConverged=true;
    Result.SettledCm={{0,0,0},{1,0,0},{0,1,0}};
    Result.UV={{0,0},{1,0},{0,1}};
    FString Error;
    TestFalse(TEXT("Convergence flag cannot authorize absent panel topology"),MakeClothCollection(Result,NewObject<UMaterial>(),Error).IsValid());
    TestFalse(TEXT("Invalid result reports a reason"),Error.IsEmpty());
    FBuildResult Valid;
    if(!TestTrue(TEXT("Prepare valid input for damaged-result checks"),BuildGarment(FlatJoinedPanels(),Valid,Error))) return false;
    UMaterial* Material=NewObject<UMaterial>();
    Result=Valid; Result.Offsets.Reset();
    TestFalse(TEXT("Missing offsets rejected without engine assertion"),MakeClothCollection(Result,Material,Error).IsValid());
    Result=Valid; Result.PinnedVertices.Add(Result.SettledCm.Num());
    TestFalse(TEXT("Out of range pin rejected"),MakeClothCollection(Result,Material,Error).IsValid());
    Result=Valid; Result.Stitches.Add(FIntVector2(-1,0));
    TestFalse(TEXT("Out of range stitch rejected"),MakeClothCollection(Result,Material,Error).IsValid());
    Result=Valid; Result.Panels[0].Triangles[0].X=-1;
    TestFalse(TEXT("Out of range triangle rejected"),MakeClothCollection(Result,Material,Error).IsValid());
    Result=Valid; Result.SettledCm[0].Z=1.e100;
    TestFalse(TEXT("Float overflow rejected at collection boundary"),MakeClothCollection(Result,Material,Error).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGInitialFaceCollision, "LaceGarment.Pipeline.RejectInitialFaceCollision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGInitialFaceCollision::RunTest(const FString&)
{
    FBuildInput Input;
    FPanel Panel; Panel.BoundaryMm={{0,0},{20,0},{20,20},{0,20}}; Panel.MaxEdgeMm=100;
    Input.Panels={Panel}; Input.Pins={{0,0}}; Input.GravityCm=0;
    // Every corner is outside, but both triangle interiors cross this capsule.
    Input.Colliders.Add({FVector3d(1,1,-1),FVector3d(1,1,1),0.4});
    FBuildResult Result; FString Error;
    TestFalse(TEXT("Vertex-only clearance cannot authorize a body-crossing panel"),BuildGarment(Input,Result,Error));
    TestFalse(TEXT("Body collision explains rejection"),Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGBodySkinTransfer, "LaceGarment.Pipeline.BodySkinTransfer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGBodySkinTransfer::RunTest(const FString&)
{
    FBuildResult Result; FString Error;
    FBuildInput Input=FlatJoinedPanels();
    for(auto& Panel : Input.Panels)
    {
        Panel.BoundaryMm={{0,0},{5,0},{5,5},{0,5}}; Panel.MaxEdgeMm=1;
        Panel.InitialPlacementCm=FTransform(FRotator(0,90,-90),FVector(1,-0.5,8));
    }
    Input.Panels[1].InitialPlacementCm.SetTranslation(FVector(1,0.1,8));
    if(!TestTrue(TEXT("Build chest panels for skin transfer"),BuildGarment(Input,Result,Error))) return false;
    auto* Body=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    if(!TestTrue(TEXT("Explicit surface-vertex transfer from independent body"),TransferBodySkin(Result,Body,Error,EBodySkinTransfer::SurfaceVertex))) { AddInfo(Error); return false; }
    TestTrue(TEXT("Transfer mode is recorded"),Result.SkinTransfer==EBodySkinTransfer::SurfaceVertex);
    TestEqual(TEXT("Weights per unwelded vertex"),Result.SkinWeights.Num(),Result.SettledCm.Num());
    bool bHasArticulatedJoint=false;
    for(int32 I=0; I<Result.SkinWeights.Num(); ++I)
    {
        float Sum=0;
        TestTrue(TEXT("At most four influences"),Result.SkinWeights[I].Num()>=1 && Result.SkinWeights[I].Num()<=4);
        for(float W : Result.SkinWeights[I]) { TestTrue(TEXT("Positive skin influence"),W>0); Sum+=W; }
        TestTrue(TEXT("Skin normalized"),FMath::IsNearlyEqual(Sum,1.f,1.e-5f));
        for(int32 J : Result.SkinJoints[I]) bHasArticulatedJoint |= J>0;
    }
    TestTrue(TEXT("Body transfer is not root binding"),bHasArticulatedJoint);
    auto Collection=MakeClothCollection(Result,NewObject<UMaterial>(),Error);
    if(!TestTrue(TEXT("Skinned cloth collection created"),Collection.IsValid())) { AddInfo(Error); return false; }
    UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Collection.ToSharedRef());
    TestEqual(TEXT("Independent reference skeleton source preserved"),Cloth.GetSkeletalMeshSoftObjectPathName(),FSoftObjectPath(Body));
    for(int32 I=0; I<Result.SkinWeights.Num(); ++I)
    {
        TestTrue(TEXT("Render skin is not overwritten by CopySimMesh"),Cloth.GetRenderBoneWeights()[I]==Result.SkinWeights[I] && Cloth.GetRenderBoneIndices()[I]==Result.SkinJoints[I]);
        const int32 Sim=Cloth.GetSimVertex3DLookup()[I];
        TestTrue(TEXT("Welded sim skin matches source surface"),Cloth.GetSimBoneWeights()[Sim]==Result.SkinWeights[I] && Cloth.GetSimBoneIndices()[Sim]==Result.SkinJoints[I]);
    }
    FBuildResult Broad;
    if(!TestTrue(TEXT("Prepare broad region crossing distinct skin domains"),BuildGarment(FlatJoinedPanels(),Broad,Error))) return false;
    TestFalse(TEXT("Excess influence union requires explicit redistribution"),TransferBodySkin(Broad,Body,Error));
    TestTrue(TEXT("Failed transfer preserves prior empty skin"),Broad.SkinWeights.IsEmpty() && Broad.ReferenceMesh.IsNull());
    return true;
}
