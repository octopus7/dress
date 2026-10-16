#include "ProbeSupport.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "ChaosClothAsset/CollectionClothSeamFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "AssetCompilingManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PreviewScene.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "PipelineStateCache.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "Materials/MaterialInterface.h"

namespace GarmentFitClothProbe::Support
{
using namespace UE::Chaos::ClothAsset;
bool GetRunContext(const FString& Feature, FRunContext& Out, FString& Error)
{
    FString Root;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ProbeOutput="), Root) || FPaths::IsRelative(Root) ||
        !FPaths::IsUnderDirectory(Root, FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Probe"))) ||
        Feature.Contains(TEXT("..")) || Feature.Contains(TEXT("/")) || Feature.Contains(TEXT("\\")))
    { Error = TEXT("Feature output must be under this host Saved/Probe with a simple feature name"); return false; }
    Out.OutputDirectory = Root / Feature;
    Out.bReopen = FParse::Param(FCommandLine::Get(), TEXT("ProbeReopen"));
    IFileManager::Get().MakeDirectory(*Out.OutputDirectory, true);
    return true;
}

bool SaveSourceAsset(UObject* Asset, FString& Error)
{
    if (!Asset || !Asset->GetOutermost()->GetName().StartsWith(TEXT("/GarmentFitClothProbe/")))
    { Error = TEXT("Only experiment plugin source assets may be saved"); return false; }
    const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args))
    { Error = TEXT("Failed to save ") + Filename; return false; }
    return true;
}

UChaosClothAsset* BuildNativeAsset(const TSharedRef<FManagedArrayCollection>& Collection,
    const FString& PackageName, FString& Error)
{
    if (!PackageName.StartsWith(TEXT("/GarmentFitClothProbe/")) || !FPackageName::IsValidLongPackageName(PackageName))
    { Error = TEXT("Cloth package must stay under /GarmentFitClothProbe/"); return nullptr; }
    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FString Name = FPackageName::GetShortName(PackageName);
    UChaosClothAsset* Asset = FindObject<UChaosClothAsset>(Package, *Name);
    if (!Asset) Asset = NewObject<UChaosClothAsset>(Package, *Name, RF_Public | RF_Standalone);
    FText BuildError, Verbose;
    Asset->Build({Collection}, nullptr, &BuildError, &Verbose);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!BuildError.IsEmpty() || !Asset->HasValidClothSimulationModels())
    { Error = TEXT("Cloth Build: ") + BuildError.ToString() + TEXT(" ") + Verbose.ToString(); return nullptr; }
    return SaveSourceAsset(Asset, Error) ? Asset : nullptr;
}

TSharedPtr<FManagedArrayCollection> BuildPanelCollection(const FInput& Input, FString& Error)
{
    if (!Validate(Input, Error)) return nullptr;
    const TSharedRef<FManagedArrayCollection> Collection = MakeShared<FManagedArrayCollection>();
    FCollectionClothFacade Cloth(Collection);
    Cloth.DefineSchema();
    for (const FPanel& Panel : Input.Panels)
        Cloth.AddGetSimPattern().Initialize(Panel.Source2D, Panel.Initial3D, Panel.Triangles);
    TArray<FIntVector2> Stitches;
    for (FIntVector2 Pair : Input.Seam)
        Stitches.Emplace(Pair.X, Input.Panels[0].Source2D.Num() + Pair.Y);
    Cloth.AddGetSeam().Initialize(Stitches);
    FClothGeometryTools::BindMeshToRootBone(Collection, true, false);
    return Collection;
}

void ApplyDefaultConfig(const TSharedRef<FManagedArrayCollection>& Collection)
{
    UChaosClothConfig* Config = NewObject<UChaosClothConfig>();
    Config->BendingStiffnessWeighted = {0.05f, 0.05f};
    Config->DampingCoefficient = 0.05f;
    Config->CollisionThickness = 1.f;
    Config->FrictionCoefficient = 0.3f;
    Config->bUseGravityOverride = true;
    Config->Gravity = FVector(0, 0, -980.665);
    Config->AnimDriveStiffness = {0.f, 0.f};
    Config->AnimDriveDamping = {0.f, 0.f};
    UChaosClothSharedSimConfig* Shared = NewObject<UChaosClothSharedSimConfig>();
    Shared->IterationCount = Shared->MaxIterationCount = 8;
    Shared->SubdivisionCount = 4;
    ::Chaos::FClothingSimulationConfig Native;
    Native.Initialize(Config, Shared);
    Native.GetPropertyCollection(0)->CopyTo(&Collection.Get());
}

bool ResolveVertex(const TSharedRef<const FManagedArrayCollection>& Collection,
    FVertexRef Vertex, int32& WeldedVertex, FString& Error)
{
    WeldedVertex = INDEX_NONE;
    const FCollectionClothConstFacade Cloth(Collection);
    if (!Cloth.IsValid() || Vertex.PanelIndex < 0 || Vertex.PanelIndex >= Cloth.GetNumSimPatterns())
    { Error = TEXT("Missing panel in vertex reference"); return false; }
    const auto Pattern = Cloth.GetSimPattern(Vertex.PanelIndex);
    if (Vertex.VertexIndex < 0 || Vertex.VertexIndex >= Pattern.GetNumSimVertices2D())
    { Error = TEXT("Missing panel-local vertex"); return false; }
    const int32 Index = Cloth.GetSimVertex3DLookup()[Pattern.GetSimVertices2DOffset() + Vertex.VertexIndex];
    if (Index < 0 || Index >= Cloth.GetNumSimVertices3D())
    { Error = TEXT("Vertex has no valid welded simulation particle"); return false; }
    WeldedVertex = Index;
    return true;
}

void TickCloth(UChaosClothComponent* Component, float Dt)
{
    static_cast<UActorComponent*>(Component)->TickComponent(Dt, LEVELTICK_All, nullptr);
    Component->WaitForExistingParallelClothSimulation_GameThread();
}

bool ReadNativePositions(UChaosClothComponent* Component, TArray<FVector3f>& Out, FString& Error)
{
    Out.Reset();
    if (!Component) { Error = TEXT("Missing cloth component"); return false; }
    Component->WaitForExistingParallelClothSimulation_GameThread();
    const FClothSimulationProxy* Proxy = Component->GetClothSimulationProxy();
    const FClothSimulData* Data = Proxy ? Proxy->GetCurrentSimulationData_AnyThread().Find(0) : nullptr;
    if (!Data || Data->Positions.IsEmpty()) { Error = TEXT("Missing native cloth simulation output"); return false; }
    for (const FVector3f& P : Data->Positions)
    {
        const FVector3f World = FVector3f(Data->Transform.TransformPosition(FVector(P)));
        if (World.ContainsNaN()) { Error = TEXT("Non-finite native cloth position"); Out.Reset(); return false; }
        Out.Add(World);
    }
    return true;
}

bool EnsureClothRenderReady(UChaosClothComponent* Component, FString& Error)
{
    if (!Component || !Component->GetWorld())
    { Error = TEXT("Native cloth capture requires a registered component"); return false; }
    // An empty global compiler queue does not mean a material's on-demand
    // shader map is complete. Request its missing permutations before the very
    // first draw; otherwise synchronous captures can omit the entire surface.
    for (UMaterialInterface* Material : Component->GetMaterials())
        if (Material) Material->EnsureIsComplete();

    PipelineStateCache::WaitForAllTasks();
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    Component->MarkRenderStateDirty();
    Component->GetWorld()->SendAllEndOfFrameUpdates();
    FlushRenderingCommands();
    if (!Component->GetSceneProxy() || !Component->GetMeshObject())
    {
        Error = FString::Printf(TEXT("Native cloth render proxy absent (PSO pending=%d, mesh object=%d)"),
            Component->IsPSOPrecaching(), Component->GetMeshObject() != nullptr);
        return false;
    }
    return true;
}

bool CapturePreview(FPreviewScene& Scene, const FString& Filename, const FVector& CameraLocation,
    const FVector& LookAt, FString& Error, UChaosClothComponent* SurfaceToVerify)
{
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
    Target->InitCustomFormat(1200, 900, PF_B8G8R8A8, false);
    Target->ClearColor = FLinearColor(0.015f, 0.02f, 0.035f, 1.f);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Camera = NewObject<USceneCaptureComponent2D>();
    Camera->TextureTarget = Target;
    Camera->CaptureSource = SCS_FinalColorLDR;
    Camera->bCaptureEveryFrame = Camera->bCaptureOnMovement = false;
    Camera->FOVAngle = 42.f;
    Camera->ShowFlags.SetTemporalAA(false);
    Camera->ShowFlags.SetMotionBlur(false);
    Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    // Keep the fixed preview exposure independent of physical camera defaults.
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    // FPreviewScene has a one-sided key and no ambient cubemap. Add a rear
    // fill so the native cloth's opposite-facing normals remain visible too.
    UDirectionalLightComponent* Fill = NewObject<UDirectionalLightComponent>();
    Fill->SetMobility(EComponentMobility::Movable);
    Fill->SetIntensity(UE_PI * 0.5f);
    Scene.AddComponent(Fill, FTransform((CameraLocation - LookAt).Rotation()));
    Scene.AddComponent(Camera, FTransform((LookAt - CameraLocation).Rotation(), CameraLocation));
    ON_SCOPE_EXIT { Scene.RemoveComponent(Camera); Scene.RemoveComponent(Fill); };
    Scene.GetWorld()->SendAllEndOfFrameUpdates();
    FlushRenderingCommands();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

    auto Render = [&]()
    {
        Camera->CaptureScene();
        FlushRenderingCommands();
    };
    auto SavePNG = [&](const FString& File)
    {
        TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(*File));
        const bool bOK = Archive && FImageUtils::ExportRenderTarget2DAsPNG(Target, *Archive);
        if (Archive && !Archive->Close()) { Error = TEXT("Capture close failed: ") + File; return false; }
        if (!bOK) Error = TEXT("Capture failed: ") + File;
        return bOK;
    };
    if (SurfaceToVerify)
    {
        // Restrict evidence to the real native component. A mannequin, collider
        // or measured solver wire cannot satisfy the surface assertion.
        Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        Camera->ShowOnlyComponent(SurfaceToVerify);
        Render();
        TArray<FColor> Pixels;
        if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) || Pixels.Num() != 1200 * 900)
        { Error = TEXT("Cannot read native surface pixels: ") + Filename; return false; }
        // The black background's tonemap dither is below 16. A visible material
        // must cover a real area, not a few isolated bright samples.
        constexpr int32 MinimumSurfacePixels = 1000;
        int32 SurfacePixels = 0;
        for (const FColor& Pixel : Pixels)
            SurfacePixels += FMath::Max3(Pixel.R, Pixel.G, Pixel.B) > 16;
        const FString Stem = Filename.LeftChop(4);
        if (!SavePNG(Stem + TEXT("-cloth-only.png"))) return false;
        const TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
        Coverage->SetStringField(TEXT("asset"), SurfaceToVerify->GetAsset()->GetPathName());
        Coverage->SetNumberField(TEXT("native_surface_pixels"), SurfacePixels);
        Coverage->SetNumberField(TEXT("minimum_surface_pixels"), MinimumSurfacePixels);
        Coverage->SetNumberField(TEXT("channel_threshold"), 16);
        Coverage->SetNumberField(TEXT("width"), 1200);
        Coverage->SetNumberField(TEXT("height"), 900);
        Coverage->SetBoolField(TEXT("passed"), SurfacePixels >= MinimumSurfacePixels);
        FString Text;
        FJsonSerializer::Serialize(Coverage, TJsonWriterFactory<>::Create(&Text));
        if (!FFileHelper::SaveStringToFile(Text, *(Stem + TEXT("-coverage.json"))))
        { Error = TEXT("Cannot save native surface coverage: ") + Filename; return false; }
        UE_LOG(LogTemp, Display, TEXT("Native surface pixels %s: %d"), *Filename, SurfacePixels);
        if (SurfacePixels < MinimumSurfacePixels)
        {
            Error = FString::Printf(TEXT("Native cloth surface missing: %s (%d pixels, require %d)"),
                *Filename, SurfacePixels, MinimumSurfacePixels);
            return false;
        }
        Camera->ClearShowOnlyComponents();
        Camera->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
    }
    Render();
    return SavePNG(Filename);
}
}
