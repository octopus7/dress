#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "EditorFramework/AssetImportData.h"
#include "AssetCompilingManager.h"
#include "LaceGarmentPipeline.h"
#include "LaceGarmentCollisionProjection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGReferenceBody, "LaceGarment.Body.IndependentReference", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGReferenceBody::RunTest(const FString&)
{
    USkeletalMesh* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    if(!TestNotNull(TEXT("Independent body is a saved plugin asset"),Mesh)) return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(!TestNotNull(TEXT("Independent skeleton"),Mesh->GetSkeleton())) return false;
    TestTrue(TEXT("Skeleton belongs to plugin"),Mesh->GetSkeleton()->GetPathName().StartsWith(TEXT("/LaceGarment/")));
    TestTrue(TEXT("Reference skeleton has articulated bones"),Mesh->GetRefSkeleton().GetNum()>10);
    TestTrue(TEXT("Body is 15 cm tall"),FMath::IsNearlyEqual(Mesh->GetImportedBounds().BoxExtent.Z*2,15.0,0.05));
    TestNotNull(TEXT("Cloth collision physics asset"),Mesh->GetPhysicsAsset());
    if(Mesh->GetPhysicsAsset()) TestTrue(TEXT("Physics belongs to plugin"),Mesh->GetPhysicsAsset()->GetPathName().StartsWith(TEXT("/LaceGarment/")));
    TestTrue(TEXT("Body has material"),!Mesh->GetMaterials().IsEmpty());
    for(const auto& Material : Mesh->GetMaterials())
        TestTrue(TEXT("Material belongs to plugin"),Material.MaterialInterface && Material.MaterialInterface->GetPathName().StartsWith(TEXT("/LaceGarment/")));
    if(const auto* Import=Mesh->GetAssetImportData())
        for(const auto& Source : Import->GetSourceData().SourceFiles)
            TestTrue(TEXT("No author machine absolute import path"),FPaths::IsRelative(Source.RelativeFilename));
    const auto* Model=Mesh->GetImportedModel();
    if(!TestTrue(TEXT("Imported geometry available"),Model && Model->LODModels.Num()>0)) return false;
    int32 InvalidWeights=0,InvalidUV=0,InvalidPositions=0,VertexCount=0;
    for(const auto& Section : Model->LODModels[0].Sections)
        for(const auto& Vertex : Section.SoftVertices)
        {
            ++VertexCount;
            uint32 Sum=0; int32 Influences=0;
            for(int32 I=0; I<MAX_TOTAL_INFLUENCES; ++I)
                if(Vertex.InfluenceWeights[I])
                {
                    Sum+=Vertex.InfluenceWeights[I]; ++Influences;
                    if(!Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[I])) ++InvalidWeights;
                    else if(!Mesh->GetRefSkeleton().IsValidIndex(Section.BoneMap[Vertex.InfluenceBones[I]])) ++InvalidWeights;
                }
            if(Sum!=65535 || Influences<1 || Influences>4) ++InvalidWeights;
            if(Vertex.Position.ContainsNaN()) ++InvalidPositions;
            const auto UV=Vertex.UVs[0];
            if(UV.ContainsNaN() || UV.X<16.f/2048 || UV.Y<16.f/2048 || UV.X>1-16.f/2048 || UV.Y>1-16.f/2048) ++InvalidUV;
        }
    TestTrue(TEXT("Substantial body mesh"),VertexCount>1000);
    TestEqual(TEXT("Finite body positions"),InvalidPositions,0);
    TestEqual(TEXT("Normalized 1-4 valid bone influences"),InvalidWeights,0);
    TestEqual(TEXT("UV0 within padded 2K atlas"),InvalidUV,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGBodyCollision, "LaceGarment.Body.ReferenceCollisionSpace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLGBodyCollision::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    TArray<LaceGarment::FCapsule> Capsules; TArray<LaceGarment::FConvexCollider> Convexes; FString Error;
    if(!TestTrue(TEXT("Extract the saved reference body collision"),LaceGarment::ReadBodyCollision(Mesh,Capsules,Convexes,Error))) { AddInfo(Error); return false; }
    // 19 body bones minus nine convex-refitted bones retain ten primitives:
    // head, neck, two forearms, two hands, two calves and two feet.
    TestEqual(TEXT("Capsule and sphere shapes retained outside refitted regions"),Capsules.Num(),10);
    TestTrue(TEXT("Cooked convexes stay within the reference decomposition budget"),Convexes.Num()>=19 && Convexes.Num()<=54);
    int32 High=0,Left=0,Right=0,Low=0;
    for(const auto& C : Capsules)
    {
        const auto Center=(C.A+C.B)*.5;
        TestTrue(TEXT("Centimetre component coordinates are finite and doll-sized"),!Center.ContainsNaN() && Center.GetAbsMax()<17);
        TestTrue(TEXT("Positive reasonable radius"),C.RadiusCm>0 && C.RadiusCm<5);
        if(Center.Z>11)
        {
            ++High;
            TestTrue(TEXT("Head proxy does not extend down through the shirt neckline"),FMath::Min(C.A.Z,C.B.Z)-C.RadiusCm>9.8);
            TestTrue(TEXT("Head proxy fits the actual doll height"),FMath::Max(C.A.Z,C.B.Z)+C.RadiusCm<15.5);
        }
        if(Center.Z<3) ++Low;
        if(Center.Y>2) ++Left;
        if(Center.Y<-2) ++Right;
    }
    TestTrue(TEXT("Head and feet stay at their reference heights"),High>0 && Low>0);
    TestTrue(TEXT("Arm proxies are on both sides in UE Y, not collapsed at bone-local origins"),Left>0 && Right>0);
    for(double Sign : {-1.0,1.0}) for(FVector2d Clearance : {FVector2d(1.45,7.8),FVector2d(1.65,7.8),FVector2d(1.5,8.0)})
    {
        const FVector3d Gap(0,Sign*Clearance.X,Clearance.Y);
        double Penetration=0;
        for(const auto& C : Capsules)
        {
            const auto D=C.B-C.A;
            const auto Q=C.A+D*FMath::Clamp(FVector3d::DotProduct(Gap-C.A,D)/FMath::Max(D.SquaredLength(),1.e-12),0.0,1.0);
            Penetration=FMath::Max(Penetration,C.RadiusCm-(Gap-Q).Length());
        }
        for(const auto& C : Convexes) {FVector3d N;Penetration=FMath::Max(Penetration,-C.SignedDistance(Gap,N));}
        // Independent triangle section at X=0,Z=7.8: torso ends at |Y|=1.249,
        // arm begins at |Y|=2.010. This point lies in the actual air gap.
        TestTrue(TEXT("Arm collision does not seal the measured torso-arm gap"),Penetration<.005);
    }
    const auto* HeadProxy=Capsules.FindByPredicate([](const auto& C){return (C.A.Z+C.B.Z)*.5>11;});
    for(const FVector3d Gap : {FVector3d(0,0,4.6),FVector3d(0,0,5.0),FVector3d(0,0,5.2),FVector3d(1.15,0,6.4),FVector3d(-1.15,0,6.4),FVector3d(0,.15,4.2),FVector3d(0,-.15,4.2)})
    {
        double Clearance=DBL_MAX;
        for(const auto& C : Capsules)
        {
            const auto D=C.B-C.A;
            const auto Q=C.A+D*FMath::Clamp(FVector3d::DotProduct(Gap-C.A,D)/FMath::Max(D.SquaredLength(),1.e-12),0.0,1.0);
            Clearance=FMath::Min(Clearance,(Gap-Q).Length()-C.RadiusCm);
        }
        for(const auto& C : Convexes) {FVector3d N;Clearance=FMath::Min(Clearance,C.SignedDistance(Gap,N));}
        // Measured original X=0 cross-sections have inner thigh |Y|=.170/.109/.085cm.
        TestTrue(TEXT("Refitted proxies leave measured thigh and waist air points clear"),Clearance>=.03);
    }
    int32 ArmVertices=0; double ArmMaxOutside=0;
    for(const auto& Section : Mesh->GetImportedModel()->LODModels[0].Sections) for(const auto& Vertex : Section.SoftVertices)
    {
        int32 Strongest=0;
        for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(Vertex.InfluenceWeights[K]>Vertex.InfluenceWeights[Strongest]) Strongest=K;
        const FString BoneName=Mesh->GetRefSkeleton().GetBoneName(Section.BoneMap[Vertex.InfluenceBones[Strongest]]).ToString();
        if(!BoneName.StartsWith(TEXT("upperarm")) && !BoneName.StartsWith(TEXT("forearm")) && !BoneName.StartsWith(TEXT("clavicle")) && !BoneName.StartsWith(TEXT("thigh")) && BoneName!=TEXT("spine_01") && BoneName!=TEXT("pelvis")) continue;
        ++ArmVertices; double Outside=DBL_MAX;
        const FVector3d P(Vertex.Position);
        for(const auto& C : Capsules)
        {
            const auto D=C.B-C.A;
            const auto Q=C.A+D*FMath::Clamp(FVector3d::DotProduct(P-C.A,D)/FMath::Max(D.SquaredLength(),1.e-12),0.0,1.0);
            Outside=FMath::Min(Outside,(P-Q).Length()-C.RadiusCm);
        }
        for(const auto& C : Convexes) {FVector3d N;Outside=FMath::Min(Outside,C.SignedDistance(P,N));}
        ArmMaxOutside=FMath::Max(ArmMaxOutside,Outside);
    }
    TestTrue(TEXT("Measured arm and spine surfaces are represented"),ArmVertices>6000);
    TestTrue(TEXT("Collision contains all refitted surface vertices"),ArmMaxOutside<1.e-5);
    const int32 HeadBone=Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head"));
    if(HeadProxy && Mesh->GetImportedModel() && !Mesh->GetImportedModel()->LODModels.IsEmpty())
    {
        int32 HeadVertices=0; double MaxOutside=0;
        for(const auto& Section : Mesh->GetImportedModel()->LODModels[0].Sections) for(const auto& V : Section.SoftVertices)
        {
            int32 Strongest=0;
            for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(V.InfluenceWeights[K]>V.InfluenceWeights[Strongest]) Strongest=K;
            if(Section.BoneMap.IsValidIndex(V.InfluenceBones[Strongest]) && Section.BoneMap[V.InfluenceBones[Strongest]]==HeadBone)
            { ++HeadVertices; MaxOutside=FMath::Max(MaxOutside,(FVector3d(V.Position)-HeadProxy->A).Length()-HeadProxy->RadiusCm); }
        }
        TestTrue(TEXT("Measured head surface is represented"),HeadVertices>100);
        TestTrue(TEXT("Head proxy conservatively contains the actual head vertices"),MaxOutside<1.e-5);
    }
    const auto Before=Capsules[0].A;
    TestFalse(TEXT("Missing body rejected"),LaceGarment::ReadBodyCollision(nullptr,Capsules,Error));
    TestEqual(TEXT("Failed extraction preserves previous collider set"),Capsules.Num(),10);
    TestTrue(TEXT("Previous collider coordinates preserved"),Capsules[0].A==Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGBodyArmSpace,"LaceGarment.Body.ArmNegativeSpace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGBodyArmSpace::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    TArray<LaceGarment::FCapsule> Capsules;TArray<LaceGarment::FConvexCollider> Convexes;FString Error;
    if(!TestTrue(TEXT("Read actual cooked reference body"),LaceGarment::ReadBodyCollision(Mesh,Capsules,Convexes,Error))) {AddInfo(Error);return false;}
    const auto& LOD=Mesh->GetImportedModel()->LODModels[0];TArray<FSoftSkinVertex> Vertices;LOD.GetVertices(Vertices);
    for(double Sign : {-1.0,1.0}) for(auto Sample : {FVector3d(1.5,8.0,.1),FVector3d(1.45,8.2,.03)})
    {
        const FVector3d P(0,Sign*Sample.X,Sample.Y);double SurfaceDistance=DBL_MAX,CollisionDistance=DBL_MAX;
        for(int32 I=0;I<LOD.IndexBuffer.Num();I+=3)
        {
            const auto Q=FMath::ClosestPointOnTriangleToPoint(P,FVector3d(Vertices[LOD.IndexBuffer[I]].Position),
                FVector3d(Vertices[LOD.IndexBuffer[I+1]].Position),FVector3d(Vertices[LOD.IndexBuffer[I+2]].Position));
            SurfaceDistance=FMath::Min(SurfaceDistance,(P-Q).Length());
        }
        for(const auto& C : Capsules)
        {
            const auto D=C.B-C.A;const auto Q=C.A+D*FMath::Clamp(FVector3d::DotProduct(P-C.A,D)/FMath::Max(D.SquaredLength(),1.e-12),0.0,1.0);
            CollisionDistance=FMath::Min(CollisionDistance,(P-Q).Length()-C.RadiusCm);
        }
        for(const auto& C : Convexes) {FVector3d N;CollisionDistance=FMath::Min(CollisionDistance,C.SignedDistance(P,N));}
        AddInfo(FString::Printf(TEXT("Arm gap %s: source surface %.6fcm, cooked collision %.6fcm, required %.3fcm"),*P.ToString(),SurfaceDistance,CollisionDistance,Sample.Z));
        TestTrue(TEXT("Protected air sphere does not intersect actual source triangles"),SurfaceDistance>Sample.Z+.005);
        TestTrue(TEXT("Cooked collision preserves the measured arm air sphere"),CollisionDistance>=Sample.Z);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGBodyThighTaper,"LaceGarment.Body.ThighTaperSpace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGBodyThighTaper::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    TArray<LaceGarment::FCapsule> Capsules;TArray<LaceGarment::FConvexCollider> Convexes;FString Error;
    if(!TestTrue(TEXT("Read actual body for hem clearance"),LaceGarment::ReadBodyCollision(Mesh,Capsules,Convexes,Error))) return false;
    const auto& LOD=Mesh->GetImportedModel()->LODModels[0];TArray<FSoftSkinVertex> Vertices;LOD.GetVertices(Vertices);
    // Measured source sections: Z3.8 front X=.608, outer Y=1.504,
    // inner Y=.393. Coarse axial supports had a 4.776cm perimeter where
    // the actual leg section is only 3.573cm, forcing a fitting hem downward.
    for(double Sign : {-1.0,1.0}) for(auto Sample : {FVector3d(.67,.97,3.8),FVector3d(-.67,.97,3.8),FVector3d(.04,1.57,3.8),FVector3d(.04,.33,3.8)})
    {
        const FVector3d P(Sample.X,Sign*Sample.Y,Sample.Z);double SurfaceDistance=DBL_MAX,CollisionDistance=DBL_MAX;
        for(int32 I=0;I<LOD.IndexBuffer.Num();I+=3)
        {
            const auto Q=FMath::ClosestPointOnTriangleToPoint(P,FVector3d(Vertices[LOD.IndexBuffer[I]].Position),
                FVector3d(Vertices[LOD.IndexBuffer[I+1]].Position),FVector3d(Vertices[LOD.IndexBuffer[I+2]].Position));
            SurfaceDistance=FMath::Min(SurfaceDistance,(P-Q).Length());
        }
        for(const auto& C : Capsules)
        {
            const auto D=C.B-C.A;const auto Q=C.A+D*FMath::Clamp(FVector3d::DotProduct(P-C.A,D)/FMath::Max(D.SquaredLength(),1.e-12),0.0,1.0);
            CollisionDistance=FMath::Min(CollisionDistance,(P-Q).Length()-C.RadiusCm);
        }
        for(const auto& C : Convexes) {FVector3d N;CollisionDistance=FMath::Min(CollisionDistance,C.SignedDistance(P,N));}
        AddInfo(FString::Printf(TEXT("Thigh taper %s: source %.6fcm, collision %.6fcm"),*P.ToString(),SurfaceDistance,CollisionDistance));
        TestTrue(TEXT("Hem air sphere avoids every actual source triangle"),SurfaceDistance>.035);
        TestTrue(TEXT("Collision preserves cloth-thickness clearance around the tapered thigh"),CollisionDistance>=.03);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGBodyClearMotion,"LaceGarment.Body.ClearFitMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGBodyClearMotion::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    TArray<LaceGarment::FCapsule> Capsules;TArray<LaceGarment::FConvexCollider> Convexes;FString Error;
    if(!TestTrue(TEXT("Read actual collision for clear movement"),LaceGarment::ReadBodyCollision(Mesh,Capsules,Convexes,Error))) return false;
    // Actual failed fits: moving each stretched edge toward its rest length is
    // provably clear of every body shape. Separate contact blocking from the
    // competing cloth constraints before changing either solver or pattern.
    const TArray<TPair<FVector3d,FVector3d>> Motions={
        {FVector3d(.31841589,1.44470816,8.20920908),FVector3d(.09632106,1.47110164,8.17939777)},
        {FVector3d(-.01174748,1.73835087,6.11110813),FVector3d(-.01455028,1.74240311,6.20395517)}};
    for(const auto& Motion : Motions)
    {
        for(const auto& C : Convexes)
        {
            double Separation=-DBL_MAX;
            for(const auto& Plane : C.GetPlanes())
            {
                const FVector3d N(Plane.X,Plane.Y,Plane.Z);
                Separation=FMath::Max(Separation,FMath::Min(FVector3d::DotProduct(Motion.Key,N)-Plane.W,FVector3d::DotProduct(Motion.Value,N)-Plane.W));
            }
            TestTrue(TEXT("One plane separates the entire motion from each thickened convex"),Separation>.0300001);
        }
        for(const auto& C : Capsules)
        {
            FVector3d A,B;FMath::SegmentDistToSegment(Motion.Key,Motion.Value,C.A,C.B,A,B);
            TestTrue(TEXT("Entire motion clears each thickened capsule"),(A-B).Length()>C.RadiusCm+.03);
        }
        const auto Actual=LaceGarment::SweepBodyCollision(Motion.Key,Motion.Value,Capsules,.03,Convexes);
        AddInfo(FString::Printf(TEXT("Clear movement endpoint error %.9fcm"),(Actual-Motion.Value).Length()));
        TestTrue(TEXT("Contact solver permits the analytically clear fit correction"),Actual.Equals(Motion.Value,1.e-6));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLGBodySurfaceCoverage,"LaceGarment.Body.ContactSurfaceCoverage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLGBodySurfaceCoverage::RunTest(const FString&)
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    TArray<LaceGarment::FCapsule> Capsules;TArray<LaceGarment::FConvexCollider> Convexes;FString Error;
    if(!TestTrue(TEXT("Read cooked collision for full surface proof"),LaceGarment::ReadBodyCollision(Mesh,Capsules,Convexes,Error))) return false;
    if(!TestTrue(TEXT("Reference fixture fits the coverage bitset"),Capsules.Num()+Convexes.Num()<=64)) return false;
    const auto& LOD=Mesh->GetImportedModel()->LODModels[0];TArray<FSoftSkinVertex> Vertices;LOD.GetVertices(Vertices);
    TArray<uint64> Inside;Inside.Init(0,Vertices.Num());TArray<bool> Relevant;Relevant.Init(false,Vertices.Num());
    for(const auto& Section : LOD.Sections) for(int32 I=0;I<Section.SoftVertices.Num();++I)
    {
        const auto& V=Section.SoftVertices[I];int32 Strongest=0;
        for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(V.InfluenceWeights[K]>V.InfluenceWeights[Strongest]) Strongest=K;
        const FString Name=Mesh->GetRefSkeleton().GetBoneName(Section.BoneMap[V.InfluenceBones[Strongest]]).ToString();
        Relevant[Section.BaseVertexIndex+I]=Name==TEXT("pelvis") || Name==TEXT("spine_01") || Name==TEXT("chest") || Name==TEXT("neck") || Name.StartsWith(TEXT("clavicle")) || Name.StartsWith(TEXT("upperarm")) || Name.StartsWith(TEXT("forearm")) || Name.StartsWith(TEXT("thigh"));
    }
    for(int32 I=0;I<Vertices.Num();++I)
    {
        const FVector3d P(Vertices[I].Position);
        for(int32 K=0;K<Capsules.Num();++K)
        {
            const auto& C=Capsules[K];const auto Axis=C.B-C.A;
            const auto Q=C.A+Axis*FMath::Clamp(FVector3d::DotProduct(P-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
            if((P-Q).Length()-C.RadiusCm<=2.e-4) Inside[I]|=uint64(1)<<K;
        }
        for(int32 K=0;K<Convexes.Num();++K)
        {FVector3d N;if(Convexes[K].SignedDistance(P,N)<=2.e-4) Inside[I]|=uint64(1)<<(Capsules.Num()+K);}
    }
    int32 Faces=0,Unproven=0;
    for(int32 I=0;I<LOD.IndexBuffer.Num();I+=3)
    {
        const int32 A=LOD.IndexBuffer[I],B=LOD.IndexBuffer[I+1],C=LOD.IndexBuffer[I+2];
        if(!Relevant[A] || !Relevant[B] || !Relevant[C]) continue;
        ++Faces;if((Inside[A]&Inside[B]&Inside[C])==0) ++Unproven;
    }
    TestTrue(TEXT("Actual garment-contact body faces tested"),Faces>25000);
    // Convexity makes three contained vertices a proof for the whole triangle.
    // Merely containing each vertex in a different collider is insufficient.
    TestEqual(TEXT("Every contact face is wholly contained in one collision shape"),Unproven,0);
    return true;
}
