#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Texture2D.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Catches unit/axis mistakes, missing rig data, rigid/unweighted imports and
// absent collision in the actual saved UE asset, independent of Blender output.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftDummyImportTest,
    "LaceLoft.Dummy.ImportContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaceLoftDummyImportTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if (!TestNotNull(TEXT("Authored dummy skeletal mesh exists"), Mesh)) return false;
    TestNotNull(TEXT("Shared skeleton exists"), Mesh->GetSkeleton());
    UPhysicsAsset* Physics = Mesh->GetPhysicsAsset();
    if (TestNotNull(TEXT("Cloth collision asset exists"), Physics))
        TestTrue(TEXT("Body and limb collision bodies exist"), Physics->SkeletalBodySetups.Num() >= 12);

    const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
    TestTrue(TEXT("Dummy is 15 cm tall, not 15 m or 0.15 cm"),
        FMath::IsNearlyEqual(Bounds.BoxExtent.Z * 2.0, 15.0, 0.2));
    TestTrue(TEXT("Soles are on z=0"), FMath::Abs(Bounds.Origin.Z - Bounds.BoxExtent.Z) < 0.1);
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    TestTrue(TEXT("A-pose lateral span is Y; forward is X"), Bounds.BoxExtent.Y > Bounds.BoxExtent.X * 1.5);
    const TCHAR* Required[] = { TEXT("root"), TEXT("pelvis"), TEXT("spine_01"), TEXT("chest"),
        TEXT("neck"), TEXT("head"), TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("forearm_l"),
        TEXT("hand_l"), TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("forearm_r"), TEXT("hand_r"),
        TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r") };
    for (const TCHAR* Bone : Required)
        TestTrue(FString::Printf(TEXT("Bone %s is imported"), Bone), Ref.FindBoneIndex(Bone) != INDEX_NONE);
    TestTrue(TEXT("Root translation is zero"), Ref.GetRefBonePose()[0].GetTranslation().IsNearlyZero(0.01));
    for (const FTransform& Transform : Ref.GetRefBonePose())
        TestTrue(TEXT("Bone scale is one"), Transform.GetScale3D().Equals(FVector::OneVector, 0.001));

    const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
    if (!TestNotNull(TEXT("Imported model is retained"), Model) || Model->LODModels.IsEmpty()) return false;
    const FSkeletalMeshLODModel& LOD = Model->LODModels[0];
    TestTrue(TEXT("UV0 is imported"), LOD.NumTexCoords >= 1);
    int32 Unweighted = 0, TooMany = 0, NotNormalized = 0, InvalidUV = 0, Blended = 0;
    for (const FSkelMeshSection& Section : LOD.Sections)
    {
        for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
        {
            uint32 Sum = 0;
            int32 Influences = 0;
            for (uint16 Weight : Vertex.InfluenceWeights) { Sum += Weight; Influences += Weight > 0; }
            Unweighted += Sum == 0;
            NotNormalized += FMath::Abs(int32(Sum) - 65535) > 2;
            TooMany += Influences > 4;
            Blended += Influences > 1;
            const FVector2f UV = Vertex.UVs[0];
            InvalidUV += !FMath::IsFinite(UV.X) || !FMath::IsFinite(UV.Y)
                || UV.X < -0.0001f || UV.Y < -0.0001f || UV.X > 1.0001f || UV.Y > 1.0001f;
        }
    }
    TestEqual(TEXT("No unweighted vertices"), Unweighted, 0);
    TestEqual(TEXT("Normalized imported weights"), NotNormalized, 0);
    TestEqual(TEXT("At most four influences per vertex"), TooMany, 0);
    TestEqual(TEXT("Finite UV0 within atlas"), InvalidUV, 0);
    TestTrue(TEXT("Joint vertices blend between bones"), Blended > 100);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftDummySurfaceTest,
    "LaceLoft.Dummy.SurfaceContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaceLoftDummySurfaceTest::RunTest(const FString& Parameters)
{
    const FString Folder=TEXT("/Game/Characters/Dummy/Materials/");
    auto* Material=LoadObject<UMaterial>(nullptr,*(Folder+TEXT("M_Dummy.M_Dummy")));
    auto* White=LoadObject<UMaterialInstanceConstant>(nullptr,*(Folder+TEXT("MI_DummyWhite.MI_DummyWhite")));
    if(!TestNotNull(TEXT("Saved paintable master material"),Material)
        || !TestNotNull(TEXT("Saved white material instance"),White))return false;
    TestTrue(TEXT("Master supports skeletal meshes without fallback"),Material->GetUsageByFlag(MATUSAGE_SkeletalMesh));
    for(const FString Suffix:{TEXT("White"),TEXT("Checker_2048"),TEXT("Checker_1024"),TEXT("Checker_512"),
                              TEXT("Asymmetric_2048"),TEXT("Asymmetric_1024"),TEXT("Asymmetric_512")})
    {
        const FString Name=TEXT("T_Dummy")+Suffix;
        auto* Texture=LoadObject<UTexture2D>(nullptr,*(Folder+Name+TEXT(".")+Name));
        if(TestNotNull(*Name,Texture))
        {
            const int32 Expected=Suffix.EndsWith(TEXT("512"))?512:Suffix.EndsWith(TEXT("1024"))?1024:2048;
            TestEqual(TEXT("Authored texture resolution survives import"),Texture->Source.GetSizeX(),int64(Expected));
        }
    }
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if(!Mesh)return false;
    TestTrue(TEXT("White instance is the actual mesh default"),Mesh->GetMaterials()[0].MaterialInterface==White);
    const auto& LOD=Mesh->GetImportedModel()->LODModels[0];
    TArray<FSoftSkinVertex> Vertices;LOD.GetVertices(Vertices);
    // Import splits vertices at normals/UV seams. Weld only positions for topology;
    // the external checker still keeps distinct UV coordinates on seam sides.
    TMap<FString,int32> Welded;TArray<int32> IDs;
    for(const auto& V:Vertices)
    {
        const FString Key=FString::Printf(TEXT("%.6f,%.6f,%.6f"),V.Position.X,V.Position.Y,V.Position.Z);
        if(!Welded.Contains(Key))Welded.Add(Key,Welded.Num());
        IDs.Add(Welded[Key]);
    }
    FString Json;auto Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    Writer->WriteArrayStart();
    for(int32 I=0;I<LOD.IndexBuffer.Num();I+=3)
    {
        Writer->WriteObjectStart();Writer->WriteArrayStart(TEXT("vertices"));
        for(int32 J=0;J<3;++J)Writer->WriteValue(IDs[LOD.IndexBuffer[I+J]]);
        Writer->WriteArrayEnd();Writer->WriteArrayStart(TEXT("xyz"));
        for(int32 J=0;J<3;++J)
        {
            const auto& V=Vertices[LOD.IndexBuffer[I+J]].Position;
            Writer->WriteArrayStart();Writer->WriteValue(V.X);Writer->WriteValue(V.Y);Writer->WriteValue(V.Z);Writer->WriteArrayEnd();
        }
        Writer->WriteArrayEnd();Writer->WriteArrayStart(TEXT("uv"));
        for(int32 J=0;J<3;++J)
        {
            const auto& UV=Vertices[LOD.IndexBuffer[I+J]].UVs[0];
            Writer->WriteArrayStart();Writer->WriteValue(UV.X);Writer->WriteValue(UV.Y);Writer->WriteArrayEnd();
        }
        Writer->WriteArrayEnd();Writer->WriteObjectEnd();
    }
    Writer->WriteArrayEnd();Writer->Close();
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/Implementation/S3-ImportedUV");
    IFileManager::Get().MakeDirectory(*Directory,true);
    TestTrue(TEXT("Imported UV triangles exported for independent numerical validation"),
        FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("UVTriangles.json"))));
    return !HasAnyErrors();
}

#endif
