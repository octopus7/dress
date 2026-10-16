#include "Misc/AutomationTest.h"
#include "GarmentFitPreviewActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Misc/PackageName.h"
#include "GarmentFitInputWidget.h"
#include "GarmentFitLabGameMode.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "ReferenceSkeleton.h"
#include "Animation/Skeleton.h"
#include "Materials/Material.h"
#include "Rendering/SkeletalMeshRenderData.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarmentFitInputTest, "GarmentFitLab.Input.ValidationAndReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGarmentFitInputTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    AGarmentFitPreviewActor* Actor = World->SpawnActor<AGarmentFitPreviewActor>();
    Actor->RefreshInput();
    TestEqual(TEXT("Unassigned input is missing"), Actor->GetInputState(), EGarmentFitInputState::Missing);
    TestFalse(TEXT("Missing input has a visible explanation"), Actor->GetInputMessage().IsEmpty());

    UObject* WrongType = NewObject<UTexture2D>(GetTransientPackage());
    Actor->InputMesh = WrongType;
    Actor->RefreshInput();
    TestEqual(TEXT("Non-mesh object is rejected"), Actor->GetInputState(), EGarmentFitInputState::UnsupportedType);

    Actor->InputMesh = NewObject<UStaticMesh>(GetTransientPackage());
    Actor->RefreshInput();
    TestEqual(TEXT("Empty static mesh is rejected"), Actor->GetInputState(), EGarmentFitInputState::EmptyMesh);
    Actor->InputMesh = NewObject<USkeletalMesh>(GetTransientPackage());
    Actor->RefreshInput();
    TestEqual(TEXT("Empty skeletal mesh is rejected"), Actor->GetInputState(), EGarmentFitInputState::EmptyMesh);

    USkeletalMesh* BonesWithoutSurface = NewObject<USkeletalMesh>(GetTransientPackage());
    BonesWithoutSurface->AddSourceModel();
    {
        FReferenceSkeletonModifier SkeletonModifier(BonesWithoutSurface->GetRefSkeleton(), nullptr);
        SkeletonModifier.Add(FMeshBoneInfo(TEXT("root"), TEXT("root"), INDEX_NONE), FTransform::Identity);
    }
    Actor->InputMesh = BonesWithoutSurface;
    Actor->RefreshInput();
    TestEqual(TEXT("Bones and a source LOD without drawable geometry are rejected"), Actor->GetInputState(), EGarmentFitInputState::EmptyMesh);
    TestTrue(TEXT("Reference-pose preview disables mesh post-process animation"), Actor->GetSkeletalPreview()->GetDisablePostProcessBlueprint());

    // Engine primitive is only a transient test input, never the character in the saved level.
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    TestNotNull(TEXT("Engine test fixture loads"), Cube);
    Actor->InputMesh = Cube;
    Actor->RefreshInput();
    TestEqual(TEXT("Renderable static input is ready for preview only"), Actor->GetInputState(), EGarmentFitInputState::Ready);
    TestTrue(TEXT("Component uses the assigned input"), Actor->GetStaticPreview()->GetStaticMesh() == Cube);
    TestNull(TEXT("Static input does not create a skeletal preview"), Actor->GetSkeletalPreview()->GetSkeletalMeshAsset());

    Actor->InputMesh = WrongType;
    Actor->RefreshInput();
    TestNull(TEXT("Rejected replacement clears the old visible mesh"), Actor->GetStaticPreview()->GetStaticMesh());
    Actor->InputMesh.Reset();
    Actor->RefreshInput();
    TestEqual(TEXT("Clearing input returns to missing"), Actor->GetInputState(), EGarmentFitInputState::Missing);

    Actor->InputMesh = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/GarmentFitLab/UnavailableInput.UnavailableInput")));
    Actor->RefreshInput();
    TestEqual(TEXT("Broken asset reference is distinct from unassigned input"), Actor->GetInputState(), EGarmentFitInputState::LoadFailed);
    TestNull(TEXT("Broken reference leaves no old static mesh"), Actor->GetStaticPreview()->GetStaticMesh());

    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarmentFitSavedAssetsTest, "GarmentFitLab.Input.SavedAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGarmentFitSavedAssetsTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Test level is saved"), FPackageName::DoesPackageExist(TEXT("/Game/Maps/L_GarmentFitLab")));
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr,
        TEXT("/GarmentFitLab/UI/WBP_GarmentFitLab.WBP_GarmentFitLab"));
    if (!TestNotNull(TEXT("Actual editable Widget Blueprint exists"), Blueprint))
    {
        return false;
    }
    TestTrue(TEXT("Widget Blueprint compiled"), Blueprint->Status == BS_UpToDate);
    TestNotNull(TEXT("Designer root is saved"), Blueprint->WidgetTree->RootWidget.Get());
    TestNotNull(TEXT("Status text is in the asset hierarchy"), Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("InputStatusText"))));
    TestNotNull(TEXT("Asset text is in the asset hierarchy"), Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("InputAssetText"))));

    UWorld* PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    AGarmentFitPreviewActor* Preview = PreviewWorld->SpawnActor<AGarmentFitPreviewActor>();
    UGarmentFitInputWidget* Widget = CreateWidget<UGarmentFitInputWidget>(PreviewWorld, Blueprint->GeneratedClass.Get());
    if (TestNotNull(TEXT("Saved WBP can be instantiated"), Widget))
    {
        Widget->SetPreviewActor(Preview);
        UTextBlock* Status = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("InputStatusText")));
        UTextBlock* Asset = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("InputAssetText")));
        if (TestNotNull(TEXT("Runtime status binding exists"), Status) && TestNotNull(TEXT("Runtime asset binding exists"), Asset))
        {
            TestTrue(TEXT("Saved WBP shows the missing-input message"), Status->GetText().EqualTo(Preview->GetInputMessage()));
            Preview->InputMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            Preview->RefreshInput();
            Widget->RefreshDisplay();
            TestTrue(TEXT("Bound status follows changed input"), Status->GetText().EqualTo(Preview->GetInputMessage()));
            TestEqual(TEXT("Bound path identifies the actual input"), Asset->GetText().ToString(), FString(TEXT("/Engine/BasicShapes/Cube.Cube")));
        }
    }
    PreviewWorld->DestroyWorld(false);

    UPackage* LevelPackage = LoadPackage(nullptr, TEXT("/Game/Maps/L_GarmentFitLab"), LOAD_None);
    UWorld* SavedWorld = UWorld::FindWorldInPackage(LevelPackage);
    if (TestNotNull(TEXT("Test level reopens"), SavedWorld))
    {
        int32 PreviewCount = 0;
        for (AActor* Candidate : SavedWorld->PersistentLevel->Actors)
        {
            if (AGarmentFitPreviewActor* SavedPreview = Cast<AGarmentFitPreviewActor>(Candidate))
            {
                ++PreviewCount;
                USkeletalMesh* Character = Cast<USkeletalMesh>(SavedPreview->InputMesh.LoadSynchronous());
                TestNotNull(TEXT("Supplied Mk2 input reopens as a skeletal mesh"), Character);
                TestEqual(TEXT("The requested mannequin derivative is assigned"), SavedPreview->InputMesh.ToSoftObjectPath().ToString(),
                    FString(TEXT("/Game/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2_Mannequin.SKM_LunaMk2_Mannequin")));
                SavedPreview->RefreshInput();
                TestEqual(TEXT("Supplied character is ready after reopening"), SavedPreview->GetInputState(), EGarmentFitInputState::Ready);
                TestTrue(TEXT("Saved level references the real WBP"), SavedPreview->InputWidgetClass == Blueprint->GeneratedClass);
                TestNull(TEXT("No second static character is displayed"), SavedPreview->GetStaticPreview()->GetStaticMesh());
                TestTrue(TEXT("Skeletal preview uses supplied character"), SavedPreview->GetSkeletalPreview()->GetSkeletalMeshAsset() == Character);
                TestTrue(TEXT("Supplied character stays in reference pose"), SavedPreview->GetSkeletalPreview()->GetDisablePostProcessBlueprint());
            }
        }
        TestEqual(TEXT("Exactly one preview actor is saved"), PreviewCount, 1);
        TestTrue(TEXT("Level uses isolated game mode"), SavedWorld->GetWorldSettings()->DefaultGameMode == AGarmentFitLabGameMode::StaticClass());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarmentFitSuppliedInputTest, "GarmentFitLab.Input.SuppliedLunaMk2",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGarmentFitSuppliedInputTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2.SKM_LunaMk2"));
    if (!TestNotNull(TEXT("Supplied mesh loads from disk"), Mesh)) return false;
    USkeleton* Skeleton = Mesh->GetSkeleton();
    if (TestNotNull(TEXT("New independent skeleton is saved"), Skeleton))
    {
        TestTrue(TEXT("Skeleton belongs to this input"), Skeleton->GetPathName().StartsWith(TEXT("/Game/GarmentFitLab/")));
    }
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    TestTrue(TEXT("Source skeleton bones are retained"), Ref.GetNum() >= 130);
    for (const FName Bone : {FName(TEXT("root")), FName(TEXT("pelvis")), FName(TEXT("head")), FName(TEXT("hand_l")), FName(TEXT("ball_l"))})
    {
        TestTrue(*FString::Printf(TEXT("Reference bone %s exists"), *Bone.ToString()), Ref.FindBoneIndex(Bone) != INDEX_NONE);
    }
    const FSkeletalMeshRenderData* Render = Mesh->GetResourceForRendering();
    if (!TestTrue(TEXT("Imported LOD has render data"), Render && !Render->LODRenderData.IsEmpty())) return false;
    const FSkeletalMeshLODRenderData& LOD = Render->LODRenderData[0];
    FBox SurfaceBounds(ForceInit);
    for (uint32 Index = 0; Index < LOD.GetNumVertices(); ++Index)
    {
        SurfaceBounds += FVector(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index));
    }
    const FVector Size = SurfaceBounds.GetSize();
    AddInfo(FString::Printf(TEXT("Mk2 surface cm: %s; bounds %s; bones %d; sections %d"),
        *Size.ToString(), *SurfaceBounds.ToString(), Ref.GetNum(), LOD.RenderSections.Num()));
    // Source is meters, Z-up, front -Y. Import converts to cm, Z-up, front +X.
    TestTrue(TEXT("Source height is preserved in centimeters"), FMath::IsNearlyEqual(Size.Z, 135.696, 0.2));
    TestTrue(TEXT("Source width lies on UE Y"), FMath::IsNearlyEqual(Size.Y, 57.315, 0.2));
    TestTrue(TEXT("Source depth lies on UE X"), FMath::IsNearlyEqual(Size.X, 22.893, 0.2));
    UMaterial* Matte = LoadObject<UMaterial>(nullptr, TEXT("/Game/GarmentFitLab/Materials/M_LunaMk2_Matte.M_LunaMk2_Matte"));
    if (TestNotNull(TEXT("New matte material is saved"), Matte))
    {
        TArray<UTexture*> Textures;
        Matte->GetUsedTextures(Textures);
        TestEqual(TEXT("New body material has no textures"), Textures.Num(), 0);
        for (const FSkeletalMaterial& Slot : Mesh->GetMaterials())
        {
            TestTrue(TEXT("Every body slot uses the requested new material"), Slot.MaterialInterface == Matte);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarmentFitMannequinTest, "GarmentFitLab.Input.MannequinDerivative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGarmentFitMannequinTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* Original = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2.SKM_LunaMk2"));
    USkeletalMesh* Mannequin = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2_Mannequin.SKM_LunaMk2_Mannequin"));
    if (!TestNotNull(TEXT("First import is preserved"), Original) ||
        !TestNotNull(TEXT("Mannequin derivative reopens"), Mannequin)) return false;
    TestTrue(TEXT("Derivative retains the existing skeleton"), Mannequin->GetSkeleton() == Original->GetSkeleton());
    const FReferenceSkeleton& Before = Original->GetRefSkeleton();
    const FReferenceSkeleton& After = Mannequin->GetRefSkeleton();
    if (TestEqual(TEXT("Bone hierarchy size is unchanged"), After.GetNum(), Before.GetNum()))
    {
        for (int32 Index = 0; Index < Before.GetNum(); ++Index)
        {
            TestEqual(TEXT("Bone names stay aligned"), After.GetBoneName(Index), Before.GetBoneName(Index));
            TestEqual(TEXT("Bone parent stays aligned"), After.GetParentIndex(Index), Before.GetParentIndex(Index));
            TestTrue(TEXT("Reference pose is preserved"), After.GetRefBonePose()[Index].Equals(Before.GetRefBonePose()[Index], 0.0001));
        }
    }
    UMaterial* Matte = LoadObject<UMaterial>(nullptr, TEXT("/Game/GarmentFitLab/Materials/M_LunaMk2_Matte.M_LunaMk2_Matte"));
    for (const FSkeletalMaterial& Slot : Mannequin->GetMaterials())
    {
        TestTrue(TEXT("Derivative uses the same texture-free matte material"), Slot.MaterialInterface == Matte);
    }
    const FSkeletalMeshRenderData* Render = Mannequin->GetResourceForRendering();
    if (!TestTrue(TEXT("Rebuilt surface renders"), Render && !Render->LODRenderData.IsEmpty())) return false;
    const FSkeletalMeshLODRenderData& LOD = Render->LODRenderData[0];
    FBox Bounds(ForceInit);
    for (uint32 Index = 0; Index < LOD.GetNumVertices(); ++Index)
    {
        Bounds += FVector(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index));
    }
    TestTrue(TEXT("Mannequin keeps source height"), FMath::IsNearlyEqual(Bounds.GetSize().Z, 135.696, 0.2));
    TestTrue(TEXT("Mannequin keeps hand span"), FMath::IsNearlyEqual(Bounds.GetSize().Y, 57.315, 0.2));
    TestTrue(TEXT("All rebuilt sections have geometry"), !LOD.RenderSections.IsEmpty());
    int32 TriangleCount = 0;
    for (const FSkelMeshRenderSection& Section : LOD.RenderSections) TriangleCount += Section.NumTriangles;
    TestEqual(TEXT("Authored closed surface survives FBX import"), TriangleCount, 75010);
    AddInfo(FString::Printf(TEXT("Mannequin bounds cm: %s; triangles %d"), *Bounds.GetSize().ToString(), TriangleCount));
    return true;
}

#endif
