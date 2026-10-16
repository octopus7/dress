#include "ProbeSupport.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "AssetCompilingManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PreviewScene.h"
#include "DrawDebugHelpers.h"
#include "Misc/AutomationTest.h"

using namespace GarmentFitClothProbe;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProbeSurfaceCoverage,
    "GarmentFitClothProbe.Rendering.SurfacesExcludeDebugAndBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProbeSurfaceCoverage::RunTest(const FString&)
{
    // A visible body or debug overlay must not let an absent cloth surface pass.
    FRunContext Context; FString Error;
    if (!Support::GetRunContext(TEXT("Rendering"), Context, Error)) { AddError(Error); return false; }
    UChaosClothAsset* Asset = LoadObject<UChaosClothAsset>(nullptr,
        TEXT("/GarmentFitClothProbe/Sewing/CA_Equal100.CA_Equal100"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!TestNotNull(TEXT("Saved native cloth fixture"), Asset) || !TestNotNull(TEXT("Occluding body control"), Cube)) return false;
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetTransactional(false));
    UChaosClothComponent* Cloth = NewObject<UChaosClothComponent>();
    Cloth->SetAsset(Asset); Cloth->SetSimulateInEditor(true); Cloth->SetForcedLOD(1);
    Cloth->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Scene.AddComponent(Cloth, FTransform::Identity);
    Support::TickCloth(Cloth, 0.f);
    if (!Support::EnsureClothRenderReady(Cloth, Error)) { AddError(Error); return false; }
    Cloth->MarkRenderDynamicDataDirty();
    const FVector Camera(200, 230, 200), LookAt(0, 40, 65);
    if (!TestTrue(TEXT("Real cloth surface passes without overlays"), Support::CapturePreview(Scene,
        Context.OutputDirectory / TEXT("positive-surface.png"), Camera, LookAt, Error, Cloth)))
    { AddError(Error); return false; }

    UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>();
    Body->SetStaticMesh(Cube);
    Scene.AddComponent(Body, FTransform(LookAt));
    for (UMaterialInterface* Material : Body->GetMaterials()) if (Material) Material->EnsureIsComplete();
    Cloth->SetHiddenInSceneCapture(true);
    Error.Reset();
    TestFalse(TEXT("Body cannot satisfy native cloth coverage"), Support::CapturePreview(Scene,
        Context.OutputDirectory / TEXT("body-without-cloth.png"), Camera, LookAt, Error, Cloth));
    TestTrue(TEXT("Missing surface is the failure reason"), Error.StartsWith(TEXT("Native cloth surface missing:")));

    Body->SetVisibility(false);
    DrawDebugPoint(Scene.GetWorld(), LookAt, 200.f, FColor::Green, true);
    Error.Reset();
    TestFalse(TEXT("Debug pixels cannot satisfy native cloth coverage"), Support::CapturePreview(Scene,
        Context.OutputDirectory / TEXT("debug-without-cloth.png"), Camera, LookAt, Error, Cloth));
    TestTrue(TEXT("Debug-only capture reports missing surface"), Error.StartsWith(TEXT("Native cloth surface missing:")));
    return true;
}
