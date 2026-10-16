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
                TestTrue(TEXT("Saved input remains unassigned until user provides it"), SavedPreview->InputMesh.IsNull());
                TestTrue(TEXT("Saved level references the real WBP"), SavedPreview->InputWidgetClass == Blueprint->GeneratedClass);
                TestNull(TEXT("No substitute character is saved"), SavedPreview->GetStaticPreview()->GetStaticMesh());
                TestNull(TEXT("No substitute skeleton is saved"), SavedPreview->GetSkeletalPreview()->GetSkeletalMeshAsset());
            }
        }
        TestEqual(TEXT("Exactly one preview actor is saved"), PreviewCount, 1);
        TestTrue(TEXT("Level uses isolated game mode"), SavedWorld->GetWorldSettings()->DefaultGameMode == AGarmentFitLabGameMode::StaticClass());
    }
    return true;
}

#endif
