#include "LaceGarmentPipeline.h"
#include "LaceGarmentCollisionProjection.h"
#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ReferenceSkeleton.h"
using namespace LaceGarment;
namespace
{
TArray<FVector3d> BoxPoints(FVector3d Center,FVector3d Extent)
{
    TArray<FVector3d> P;
    for(double X : {-1.,1.}) for(double Y : {-1.,1.}) for(double Z : {-1.,1.}) P.Add(Center+Extent*FVector3d(X,Y,Z));
    return P;
}
double ConvexDepth(FVector3d P,const FConvexCollider& C)
{
    double Depth=DBL_MAX;
    for(const auto& Plane : C.GetPlanes()) Depth=FMath::Min(Depth,-Plane.PlaneDot(P));
    return Depth;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGConvexHull,"LaceGarment.Collision.ConvexHull",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGConvexHull::RunTest(const FString&)
{
    const auto Points=BoxPoints({0,0,0},{.8,.3,.2}); FConvexCollider Hull; FString Error;
    if(!TestTrue(TEXT("Build native convex hull"),BuildConvexCollider(Points,Hull,Error))) {AddInfo(Error);return false;}
    TestTrue(TEXT("Body centre is inside"),FMath::IsNearlyEqual(ConvexDepth({0,0,0},Hull),.2,1.e-7));
    TestTrue(TEXT("Real side clearance is not inflated to a sphere"),ConvexDepth({0,.5,0},Hull)<-.19);
    for(auto P : Points) TestTrue(TEXT("Every source point is contained"),ConvexDepth(P,Hull)>=-1.e-8);
    const auto Saved=Hull.GetVertices();
    TestFalse(TEXT("Coplanar hull rejected"),BuildConvexCollider({{0,0,0},{1,0,0},{1,1,0},{0,1,0}},Hull,Error));
    TestTrue(TEXT("Failed hull build is atomic"),Saved==Hull.GetVertices());
    TestFalse(TEXT("A hull collapsed by native float precision is rejected"),BuildConvexCollider(BoxPoints({0,0,1000},{1,1,1.e-5}),Hull,Error));
    TestTrue(TEXT("Native precision rejection preserves the previous hull"),Saved==Hull.GetVertices());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGConvexContact,"LaceGarment.Collision.ConvexContinuousContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGConvexContact::RunTest(const FString&)
{
    FConvexCollider Hull; FString Error;
    if(!TestTrue(TEXT("Prepare thin body hull"),BuildConvexCollider(BoxPoints({0,0,0},{.8,.3,.2}),Hull,Error))) return false;
    const TArray<FConvexCollider> Convexes={Hull}; const TArray<FCapsule> Empty;
    auto Result=ProjectBodyCollision({0,0,0},Empty,.03,Convexes);
    TestTrue(TEXT("Interior point exits padded convex"),ConvexDepth(Result,Hull)<=-.03+1.e-7);
    Result=SweepBodyCollision({2,0,0},{-2,0,0},Empty,.03,Convexes);
    TestTrue(TEXT("Outside endpoints cannot tunnel through hull"),Result.X>=.83-1.e-7 && Result.X<.84);
    Result=SweepBodyCollision({2,0,0},{-2,.1,0},Empty,.03,Convexes);
    TestTrue(TEXT("Contact retains tangential motion"),Result.X>=.83-1.e-7 && Result.Y>.09);
    const FVector3d Clear(0,.6,0);
    TestTrue(TEXT("Clear space stays clear"),SweepBodyCollision({2,.6,0},Clear,Empty,.03,Convexes).Equals(Clear,1.e-8));
    const FVector3d Corner(.825,.325,0);
    TestTrue(TEXT("Rounded thickness preserves diagonal corner clearance"),ProjectBodyCollision(Corner,Empty,.03,Convexes).Equals(Corner,1.e-8));
    const FVector3d NearCorner=FVector3d(.8,.3,0)+FVector3d(1,1,0).GetSafeNormal()*.03001;
    Result=SweepBodyCollision(NearCorner,-NearCorner,Empty,.03,Convexes);
    TestTrue(TEXT("A near-surface rounded-corner start cannot tunnel"),Result.X>0 && Result.Y>0);
    const FVector3d GrazingCorner(.8298958028897333,.3026155438398572,0);
    Result=SweepBodyCollision(GrazingCorner,-GrazingCorner*2,Empty,.03,Convexes);
    AddInfo(FString::Printf(TEXT("Grazing-corner inward result: %s"),*Result.ToString()));
    FVector3d ContactNormal;
    // Sliding may cross Y=0 along the positive-X face. The regression is the
    // unchecked jump to the far-side target, not the sign of tangential motion.
    TestTrue(TEXT("Conservative GJK entry behind the start still blocks through-motion"),
        Result.X>=.8 && Hull.SignedDistance(Result,ContactNormal)>=.03-1.e-6);
    Result=SweepBodyCollision(GrazingCorner,GrazingCorner*2,Empty,.03,Convexes);
    TestTrue(TEXT("Near-corner outward motion remains free"),Result.Equals(GrazingCorner*2,1.e-8));
    const TArray<FCapsule> Mixed={{{0,0,.2},{0,0,.2},.3}};
    Result=ProjectBodyCollision({0,0,.1},Mixed,.03,Convexes);
    TestTrue(TEXT("Overlap exits convex and capsule"),ConvexDepth(Result,Hull)<=-.03+1.e-7 && (Result-Mixed[0].A).Length()>=.33-1.e-7);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGConvexFace,"LaceGarment.Collision.RejectConvexFaceCrossing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGConvexFace::RunTest(const FString&)
{
    FConvexCollider Hull; FString Error;
    if(!TestTrue(TEXT("Prepare face-crossing hull"),BuildConvexCollider(BoxPoints({1,1,0},{.3,.3,.5}),Hull,Error))) return false;
    FBuildInput Input; FPanel Panel; Panel.BoundaryMm={{0,0},{20,0},{20,20},{0,20}}; Panel.MaxEdgeMm=100;
    Input.Panels={Panel}; Input.Pins={{0,0}}; Input.GravityCm=0; Input.ConvexColliders={Hull};
    FBuildResult Result;
    TestFalse(TEXT("Clear vertices do not authorize a hull-crossing face"),BuildGarment(Input,Result,Error));
    TestTrue(TEXT("Initial face rejection explains the cause"),Error.Contains(TEXT("face penetrates")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGCookedConvex,"LaceGarment.Collision.CookedConvexSpace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGCookedConvex::RunTest(const FString&)
{
    auto* Body=NewObject<USkeletalMesh>();
    const FTransform Root(FRotator(0,23,0),FVector(0,0,4)),Child(FRotator(0,0,37),FVector(0,2,1));
    { FReferenceSkeletonModifier Modifier(Body->GetRefSkeleton(),nullptr);
      Modifier.Add(FMeshBoneInfo(TEXT("root"),TEXT("root"),INDEX_NONE),Root);
      Modifier.Add(FMeshBoneInfo(TEXT("arm"),TEXT("arm"),0),Child); }
    auto* Physics=NewObject<UPhysicsAsset>(); auto* Setup=NewObject<USkeletalBodySetup>(Physics);
    Setup->BoneName=TEXT("arm"); FKConvexElem Shape;
    for(auto P : BoxPoints({.2,.1,0},{.8,.3,.2})) Shape.VertexData.Add(P);
    Shape.UpdateElemBox(); Setup->AggGeom.ConvexElems.Add(Shape); Setup->CreatePhysicsMeshes();
    Physics->SkeletalBodySetups.Add(Setup); Body->SetPhysicsAsset(Physics);
    TArray<FCapsule> Capsules; TArray<FConvexCollider> Hulls; FString Error;
    if(!TestTrue(TEXT("Extract actual cooked convex"),ReadBodyCollision(Body,Capsules,Hulls,Error))) {AddInfo(Error);return false;}
    if(!TestEqual(TEXT("One native convex"),Hulls.Num(),1)) return false;
    const FTransform Component=Child*Root; FVector3d N;
    TestTrue(TEXT("Cooked convex follows full reference bone hierarchy"),FMath::IsNearlyEqual(Hulls[0].SignedDistance(Component.TransformPosition({.2,.1,0}),N),-.2,1.e-5));
    const auto Corner=Component.TransformPosition({1.025,.425,0});
    TestTrue(TEXT("Cooked polygon faces retain rounded corner clearance"),FMath::IsNearlyEqual(Hulls[0].SignedDistance(Corner,N),FMath::Sqrt(2.)*.025,1.e-5));
    TestTrue(TEXT("Rotated convex blocks traversal"),(SweepBodyCollision(Component.TransformPosition({2,.1,0}),Component.TransformPosition({-2,.1,0}),Capsules,.03,Hulls)-Component.TransformPosition({-2,.1,0})).Length()>1);
    const auto Saved=Hulls[0].GetVertices();
    TestFalse(TEXT("Capsule-only reader never silently drops convex shapes"),ReadBodyCollision(Body,Capsules,Error));
    Setup->AggGeom.ConvexElems[0].SetTransform(FTransform(FVector(1,0,0)));
    TestFalse(TEXT("Nonidentity convex element transform rejected"),ReadBodyCollision(Body,Capsules,Hulls,Error));
    TestTrue(TEXT("Failed extraction is atomic"),Hulls.Num()==1 && Hulls[0].GetVertices()==Saved);
    Setup->AggGeom.ConvexElems[0].SetTransform(FTransform::Identity);
    Setup->AggGeom.ConvexElems[0].ResetChaosConvexMesh();
    TestFalse(TEXT("Missing cooked shape rejected before runtime dereference"),ReadBodyCollision(Body,Capsules,Hulls,Error));
    return true;
}
