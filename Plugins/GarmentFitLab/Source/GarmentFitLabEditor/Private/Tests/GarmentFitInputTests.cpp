#include "Misc/AutomationTest.h"
#include "GarmentFitPreviewActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
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
                    FString(TEXT("/Game/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2_MannequinFit.SKM_LunaMk2_MannequinFit")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGarmentFitProportionsTest, "GarmentFitLab.Input.FittedMannequin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGarmentFitProportionsTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* Baseline = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2_Mannequin.SKM_LunaMk2_Mannequin"));
    USkeletalMesh* Fitted = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2_MannequinFit.SKM_LunaMk2_MannequinFit"));
    if (!TestNotNull(TEXT("Baseline reopens"), Baseline) || !TestNotNull(TEXT("Fitted input reopens"), Fitted)) return false;
    if (!TestNotNull(TEXT("Fitted skeleton reopens"), Fitted->GetSkeleton())) return false;
    TestTrue(TEXT("Changed rest pose has an independent skeleton"), Fitted->GetSkeleton() != Baseline->GetSkeleton());
    const FReferenceSkeleton& Before = Baseline->GetRefSkeleton();
    const FReferenceSkeleton& After = Fitted->GetRefSkeleton();
    const FReferenceSkeleton& SavedSkeleton = Fitted->GetSkeleton()->GetReferenceSkeleton();
    if (!TestEqual(TEXT("All 131 bones survive fitting"), After.GetNum(), Before.GetNum()) ||
        !TestEqual(TEXT("Skeleton and mesh have the same bone count"), SavedSkeleton.GetNum(), After.GetNum())) return false;
    TArray<FTransform> OldComponent, NewComponent;
    for (int32 Index = 0; Index < After.GetNum(); ++Index)
    {
        TestEqual(TEXT("Fitted bone names are retained"), After.GetBoneName(Index), Before.GetBoneName(Index));
        TestEqual(TEXT("Fitted parents are retained"), After.GetParentIndex(Index), Before.GetParentIndex(Index));
        TestEqual(TEXT("Saved skeleton names match mesh"), SavedSkeleton.GetBoneName(Index), After.GetBoneName(Index));
        TestEqual(TEXT("Saved skeleton parents match mesh"), SavedSkeleton.GetParentIndex(Index), After.GetParentIndex(Index));
        TestTrue(TEXT("Saved skeleton rest pose matches mesh bind pose"),
            SavedSkeleton.GetRefBonePose()[Index].Equals(After.GetRefBonePose()[Index], 0.001));
        const int32 Parent = After.GetParentIndex(Index);
        OldComponent.Add(Parent == INDEX_NONE ? Before.GetRefBonePose()[Index] : Before.GetRefBonePose()[Index] * OldComponent[Parent]);
        NewComponent.Add(Parent == INDEX_NONE ? After.GetRefBonePose()[Index] : After.GetRefBonePose()[Index] * NewComponent[Parent]);
        TestTrue(TEXT("Fitting preserves joint axes"), NewComponent[Index].GetRotation().AngularDistance(OldComponent[Index].GetRotation()) < 0.001);
        // FBX retains the armature's meter-to-centimeter conversion scale.
        // Limb lengths must change joint translations, not add bone scaling.
        TestTrue(TEXT("Fitting adds no bone scale"), NewComponent[Index].GetScale3D().Equals(OldComponent[Index].GetScale3D(), 0.001));
    }
    const auto Point = [&After, &NewComponent](const FString& Name)
    {
        return NewComponent[After.FindBoneIndex(*Name)].GetLocation();
    };
    for (const FString Side : {FString(TEXT("l")), FString(TEXT("r"))})
    {
        for (const FString Joint : {FString(TEXT("upperarm_")), FString(TEXT("lowerarm_")), FString(TEXT("hand_")),
            FString(TEXT("thigh_")), FString(TEXT("calf_")), FString(TEXT("foot_")), FString(TEXT("ik_hand_")), FString(TEXT("ik_foot_"))})
        {
            if (!TestTrue(TEXT("Required fitting joint exists"), After.FindBoneIndex(*(Joint + Side)) != INDEX_NONE)) return false;
        }
        const double Upper = FVector::Distance(Point(TEXT("upperarm_")+Side), Point(TEXT("lowerarm_")+Side));
        const double Fore = FVector::Distance(Point(TEXT("lowerarm_")+Side), Point(TEXT("hand_")+Side));
        const double Thigh = FVector::Distance(Point(TEXT("thigh_")+Side), Point(TEXT("calf_")+Side));
        const double Calf = FVector::Distance(Point(TEXT("calf_")+Side), Point(TEXT("foot_")+Side));
        TestTrue(TEXT("Upper arm is lengthened with its joints"), FMath::IsNearlyEqual(Upper, 18.0185, 0.01));
        TestTrue(TEXT("Forearm is lengthened with its joints"), FMath::IsNearlyEqual(Fore, 16.1007, 0.01));
        TestTrue(TEXT("Thigh retains its length"), FMath::IsNearlyEqual(Thigh, 32.2423, 0.01));
        TestTrue(TEXT("Calf is shortened with its joints"), FMath::IsNearlyEqual(Calf, 33.834, 0.01));
        TestTrue(TEXT("Hand IK follows the new wrist"), Point(TEXT("ik_hand_")+Side).Equals(Point(TEXT("hand_")+Side), 0.001));
        TestTrue(TEXT("Foot IK stays at the ankle"), Point(TEXT("ik_foot_")+Side).Equals(Point(TEXT("foot_")+Side), 0.001));
        const int32 FootIndex = After.FindBoneIndex(*(TEXT("foot_")+Side));
        TestTrue(TEXT("Feet retain their floor placement"), NewComponent[FootIndex].GetLocation().Equals(OldComponent[FootIndex].GetLocation(), 0.001));
        AddInfo(FString::Printf(TEXT("Fitted %s lengths cm: arm %.4f + %.4f, leg %.4f + %.4f"), *Side, Upper, Fore, Thigh, Calf));
    }
    const FSkeletalMeshRenderData* Render = Fitted->GetResourceForRendering();
    if (!TestTrue(TEXT("Fitted mesh has render data"), Render && !Render->LODRenderData.IsEmpty())) return false;
    const FSkeletalMeshLODRenderData& LOD = Render->LODRenderData[0];
    FBox Bounds(ForceInit);
    for (uint32 Index = 0; Index < LOD.GetNumVertices(); ++Index)
        Bounds += FVector(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index));
    TestTrue(TEXT("Shorter legs are reflected in surface height"), FMath::IsNearlyEqual(Bounds.GetSize().Z, 127.226, 0.02));
    TestTrue(TEXT("Longer arms are reflected in surface span"), FMath::IsNearlyEqual(Bounds.GetSize().Y, 65.9857, 0.02));
    int32 Triangles = 0;
    for (const FSkelMeshRenderSection& Section : LOD.RenderSections) Triangles += Section.NumTriangles;
    TestEqual(TEXT("Limb fitting preserves the closed topology"), Triangles, 75010);
    UMaterial* Matte = LoadObject<UMaterial>(nullptr, TEXT("/Game/GarmentFitLab/Materials/M_LunaMk2_Matte.M_LunaMk2_Matte"));
    for (const FSkeletalMaterial& Slot : Fitted->GetMaterials())
        TestTrue(TEXT("Fitted body retains the requested matte"), Slot.MaterialInterface == Matte);
    AddInfo(FString::Printf(TEXT("Fitted surface cm: %s"), *Bounds.GetSize().ToString()));

    // Exercise the imported skin and inverse bind matrices, not just bone names.
    UWorld* PoseWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    AActor* PoseActor = PoseWorld->SpawnActor<AActor>();
    UPoseableMeshComponent* Pose = NewObject<UPoseableMeshComponent>(PoseActor);
    PoseActor->SetRootComponent(Pose);
    Pose->SetSkinnedAssetAndUpdate(Fitted);
    Pose->RegisterComponentWithWorld(PoseWorld);
    Pose->RefreshBoneTransforms();
    TArray<FMatrix44f> RefToLocal;
    TArray<FVector3f> RestPositions, BentPositions, ResetPositions;
    const auto Skin = [&]()
    {
        TArray<FVector3f> Positions;
        Pose->GetCurrentRefToLocalMatrices(RefToLocal, 0);
        USkinnedMeshComponent::ComputeSkinnedPositions(Pose, Positions, RefToLocal, LOD, LOD.SkinWeightVertexBuffer);
        return Positions;
    };
    RestPositions = Skin();
    double RestError = 0;
    for (int32 Index = 0; Index < RestPositions.Num(); ++Index)
        RestError = FMath::Max(RestError, double(FVector3f::Distance(RestPositions[Index], LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Index))));
    TestEqual(TEXT("Every imported vertex has a rest skin result"), RestPositions.Num(), int32(LOD.GetNumVertices()));
    TestTrue(TEXT("Imported inverse binds reproduce the rest surface"), RestError < 0.02);
    for (const FName Joint : {FName(TEXT("lowerarm_l")), FName(TEXT("lowerarm_r")), FName(TEXT("calf_l")), FName(TEXT("calf_r"))})
    {
        FTransform Transform = Pose->GetBoneTransformByName(Joint, EBoneSpaces::ComponentSpace);
        Transform.SetRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(60.0)) * Transform.GetRotation());
        Pose->SetBoneTransformByName(Joint, Transform, EBoneSpaces::ComponentSpace);
    }
    Pose->RefreshBoneTransforms();
    BentPositions = Skin();
    double MaxMovement = 0;
    bool bFinite = BentPositions.Num() == RestPositions.Num();
    for (int32 Index = 0; Index < BentPositions.Num(); ++Index)
    {
        bFinite &= !BentPositions[Index].ContainsNaN() && BentPositions[Index].Size() < 250.0f;
        MaxMovement = FMath::Max(MaxMovement, double(FVector3f::Distance(BentPositions[Index], RestPositions[Index])));
    }
    TestTrue(TEXT("Elbow and knee bends keep finite skin positions"), bFinite);
    TestTrue(TEXT("Imported skin follows the bent limbs"), MaxMovement > 10.0);
    for (const FName Joint : {FName(TEXT("lowerarm_l")), FName(TEXT("lowerarm_r")), FName(TEXT("calf_l")), FName(TEXT("calf_r"))})
        Pose->ResetBoneTransformByName(Joint);
    Pose->RefreshBoneTransforms();
    ResetPositions = Skin();
    double ResetError = 0;
    for (int32 Index = 0; Index < ResetPositions.Num(); ++Index)
        ResetError = FMath::Max(ResetError, double(FVector3f::Distance(ResetPositions[Index], RestPositions[Index])));
    TestTrue(TEXT("Reference surface restores after bending"), ResetError < 0.02);
    AddInfo(FString::Printf(TEXT("Fitted skin cm: rest error %.6f, bend movement %.3f, reset error %.6f"), RestError, MaxMovement, ResetError));
    PoseWorld->DestroyWorld(false);
    return true;
}

#endif
