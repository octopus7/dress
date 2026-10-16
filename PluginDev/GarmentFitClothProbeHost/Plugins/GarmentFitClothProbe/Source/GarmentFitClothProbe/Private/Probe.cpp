#include "Probe.h"
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
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "PreviewScene.h"
#include "DrawDebugHelpers.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/SavePackage.h"
#include "Serialization/JsonSerializer.h"

namespace GarmentFitClothProbe
{
using namespace UE::Chaos::ClothAsset;
static constexpr float Dt = 1.f / 60.f;
static constexpr int32 Steps = 600;
static const FVector SphereCenter(0, -40, 40);
static constexpr float SphereRadius = 20.f;
static const TCHAR* AssetRoot = TEXT("/GarmentFitClothProbe/Experiment/");

static FInput MakeInput()
{
    FInput Input;
    Input.Panels.SetNum(2);
    // Two separately authored 50 x 100 cm flat panels, 5 cm grid, already cut.
    for (int32 P = 0; P < 2; ++P)
    {
        FPanel& Panel = Input.Panels[P];
        for (int32 Y = 0; Y <= 20; ++Y)
            for (int32 X = 0; X <= 10; ++X)
            {
                Panel.Source2D.Emplace(X * 5.f, Y * 5.f);
                Panel.Initial3D.Emplace(X * 5.f + (P == 0 ? -50.f : 0.f), Y * 5.f - 50.f, 80.f);
            }
        for (int32 Y = 0; Y < 20; ++Y)
            for (int32 X = 0; X < 10; ++X)
            {
                const int32 I = Y * 11 + X;
                Panel.Triangles.Emplace(I, I + 1, I + 12);
                Panel.Triangles.Emplace(I, I + 12, I + 11);
            }
    }
    for (int32 Y = 0; Y <= 20; ++Y) Input.Seam.Emplace(Y * 11 + 10, Y * 11);
    return Input;
}

static bool SaveAsset(UObject* Asset, FString& Error)
{
    const FString File = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *File, Args))
    {
        Error = TEXT("Failed to save ") + File;
        return false;
    }
    return true;
}

static UPhysicsAsset* CreateSphere(FString& Error)
{
    UPackage* Package = CreatePackage(*(FString(AssetRoot) + TEXT("PA_Sphere")));
    Package->FullyLoad(); // Required when regenerating an existing source asset in a new process.
    UPhysicsAsset* Physics = FindObject<UPhysicsAsset>(Package, TEXT("PA_Sphere"));
    if (!Physics) Physics = NewObject<UPhysicsAsset>(Package, TEXT("PA_Sphere"), RF_Public | RF_Standalone);
    Physics->SkeletalBodySetups.Reset();
    USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(Physics);
    Body->BoneName = TEXT("Root"); // The native cloth asset's default reference skeleton.
    FKSphereElem Sphere(SphereRadius);
    Sphere.Center = SphereCenter;
    Body->AggGeom.SphereElems.Add(Sphere);
    Physics->SkeletalBodySetups.Add(Body);
    Physics->UpdateBodySetupIndexMap();
    Physics->UpdateBoundsBodiesArray();
    return SaveAsset(Physics, Error) ? Physics : nullptr;
}

static UChaosClothAsset* CreateCloth(const FInput& Input, UPhysicsAsset* Physics, const TCHAR* Name, FString& Error)
{
    TSharedRef<FManagedArrayCollection> Collection = MakeShared<FManagedArrayCollection>();
    FCollectionClothFacade Cloth(Collection);
    Cloth.DefineSchema();
    for (const FPanel& Panel : Input.Panels)
        Cloth.AddGetSimPattern().Initialize(Panel.Source2D, Panel.Initial3D, Panel.Triangles);
    TArray<FIntVector2> Stitches;
    for (FIntVector2 Pair : Input.Seam)
        Stitches.Emplace(Pair.X, Input.Panels[0].Source2D.Num() + Pair.Y);
    Cloth.AddGetSeam().Initialize(Stitches);
    FClothGeometryTools::BindMeshToRootBone(Collection, true, false);

    Cloth.AddWeightMap(TEXT("MaxDistance"));
    TArrayView<float> MaxDistance = Cloth.GetWeightMap(TEXT("MaxDistance"));
    for (int32 I = 0; I < MaxDistance.Num(); ++I)
        MaxDistance[I] = Cloth.GetSimPosition3D()[I].Y < -49.9f ? 0.f : 200.f;

    // Native default-config node's UObject -> property-collection path, no custom solver.
    UChaosClothConfig* Config = NewObject<UChaosClothConfig>();
    Config->BendingStiffnessWeighted = { 0.05f, 0.05f };
    Config->DampingCoefficient = 0.05f;
    Config->CollisionThickness = 1.f;
    Config->FrictionCoefficient = 0.3f;
    Config->bUseGravityOverride = true;
    Config->Gravity = FVector(0, 0, -980.665);
    Config->AnimDriveStiffness = { 0.f, 0.f };
    Config->AnimDriveDamping = { 0.f, 0.f };
    UChaosClothSharedSimConfig* Shared = NewObject<UChaosClothSharedSimConfig>();
    Shared->IterationCount = 8;
    Shared->MaxIterationCount = 8;
    Shared->SubdivisionCount = 4;
    ::Chaos::FClothingSimulationConfig NativeConfig;
    NativeConfig.Initialize(Config, Shared);
    NativeConfig.GetPropertyCollection(0)->CopyTo(&Collection.Get());
    FClothEngineTools::GenerateTethers(Collection, TEXT("MaxDistance"), true);
    if (Physics) Cloth.SetPhysicsAssetSoftObjectPathName(FSoftObjectPath(Physics));
    FClothGeometryTools::CopySimMeshToRenderMesh(Collection, FSoftObjectPath(FClothEngineTools::GetSimPreviewMaterial()), false);

    UPackage* Package = CreatePackage(*(FString(AssetRoot) + Name));
    Package->FullyLoad();
    FAssetCompilingManager::Get().FinishAllCompilation(); // Finish async PostLoad before Build resets materials/collections.
    UChaosClothAsset* Asset = FindObject<UChaosClothAsset>(Package, Name);
    if (!Asset) Asset = NewObject<UChaosClothAsset>(Package, Name, RF_Public | RF_Standalone);
    FText BuildError, Verbose;
    Asset->Build({Collection}, nullptr, &BuildError, &Verbose);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!BuildError.IsEmpty() || !Asset->HasValidClothSimulationModels())
    {
        Error = TEXT("Cloth Build: ") + BuildError.ToString() + TEXT(" ") + Verbose.ToString();
        return nullptr;
    }
    return SaveAsset(Asset, Error) ? Asset : nullptr;
}

static bool ReadPositions(UChaosClothComponent* Component, TArray<FVector3f>& Out, FString& Error)
{
    Component->WaitForExistingParallelClothSimulation_GameThread();
    const FClothSimulationProxy* Proxy = Component->GetClothSimulationProxy();
    const FClothSimulData* Data = Proxy ? Proxy->GetCurrentSimulationData_AnyThread().Find(0) : nullptr;
    if (!Data || Data->Positions.Num() != 441)
    {
        Error = FString::Printf(TEXT("Missing native simulation data or unexpected vertex count (%d)"), Data ? Data->Positions.Num() : -1);
        return false;
    }
    Out.Reset(Data->Positions.Num());
    for (const FVector3f& P : Data->Positions)
        Out.Add(FVector3f(Data->Transform.TransformPosition(FVector(P))));
    return true;
}

static void Tick(UChaosClothComponent* Component, float Delta)
{
    static_cast<UActorComponent*>(Component)->TickComponent(Delta, LEVELTICK_All, nullptr);
    Component->WaitForExistingParallelClothSimulation_GameThread();
}

static TArray<TSharedPtr<FJsonValue>> JsonPositions(TConstArrayView<FVector3f> Positions)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for (const FVector3f& P : Positions)
        Values.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y), MakeShared<FJsonValueNumber>(P.Z)}));
    return Values;
}

static bool Capture(FPreviewScene& Scene, UChaosClothComponent* Component, const FCollectionClothConstFacade& Cloth,
    TConstArrayView<FVector3f> Positions, bool bSphere, const FString& File, FString& Error, bool bBackView = false)
{
    // The cloth surface is rendered by the actual UChaosClothComponent; thin debug lines
    // overlay the measured native sim topology. No substitute mesh or UI widget tree.
    FlushPersistentDebugLines(Scene.GetWorld());
    const auto Triangles = Cloth.GetSimIndices3D();
    for (int32 I = 0; I < Triangles.Num(); ++I)
    {
        const FIntVector3 T = Triangles[I];
        const FColor Color = I < 400 ? FColor(30, 190, 230) : FColor(245, 155, 45);
        for (int32 K = 0; K < 3; ++K)
            DrawDebugLine(Scene.GetWorld(), FVector(Positions[T[K]]), FVector(Positions[T[(K+1)%3]]), Color, true, -1, 0, 0.35f);
    }
    const auto Seam = Cloth.GetSeam(0).GetSeamStitch3DIndex();
    for (int32 I = 1; I < Seam.Num(); ++I)
        DrawDebugLine(Scene.GetWorld(), FVector(Positions[Seam[I-1]]), FVector(Positions[Seam[I]]), FColor::Yellow, true, -1, 0, 1.1f);
    for (int32 I = 0; I < Positions.Num(); ++I)
        if (Cloth.GetWeightMap(TEXT("MaxDistance"))[I] == 0.f)
            DrawDebugPoint(Scene.GetWorld(), FVector(Positions[I]), 7.f, FColor::Green, true);
    if (bSphere) DrawDebugSphere(Scene.GetWorld(), SphereCenter, SphereRadius, 32, FColor(240, 90, 130), true, -1, 0, 0.35f);

    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
    Target->InitCustomFormat(1200, 900, PF_B8G8R8A8, false);
    Target->ClearColor = FLinearColor(0.015f, 0.02f, 0.035f, 1.f);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D* Camera = NewObject<USceneCaptureComponent2D>();
    Camera->TextureTarget = Target;
    Camera->CaptureSource = SCS_FinalColorLDR;
    Camera->bCaptureEveryFrame = false;
    Camera->bCaptureOnMovement = false;
    Camera->FOVAngle = 42.f;
    Camera->ShowFlags.SetTemporalAA(false);
    Camera->ShowFlags.SetMotionBlur(false);
    Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
    const FVector Location(190, bBackView ? -210 : 210, 165), LookAt(0, -10, 35);
    Scene.AddComponent(Camera, FTransform((LookAt - Location).Rotation(), Location));
    Component->MarkRenderDynamicDataDirty();
    Scene.GetWorld()->SendAllEndOfFrameUpdates();
    FlushRenderingCommands();
    Camera->CaptureScene();
    FlushRenderingCommands();
    TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(*File));
    const bool bOk = Archive && FImageUtils::ExportRenderTarget2DAsPNG(Target, *Archive);
    if (Archive) Archive->Close();
    Scene.RemoveComponent(Camera);
    if (!bOk) Error = TEXT("Capture failed: ") + File;
    return bOk;
}

struct FRunMetrics
{
    float MaxPinError = 0, MaxFrameMotion = 0, MaxEdgeRatio = 1, MaxPenetration = 0;
    float MeanDrop = 0, LastSecondMaxMotion = 0, ResetError = 0, RepeatError = 0;
    int32 ContactSamples = 0;
    TArray<FVector3f> Final;
};

static bool Simulate(UChaosClothAsset* Asset, bool bSphere, const FString& Output, FRunMetrics& M, FString& Error)
{
    const FCollectionClothConstFacade Cloth(static_cast<const UChaosClothAsset*>(Asset)->GetClothCollections()[0]);
    const auto Rest = Cloth.GetSimPosition3D();
    const auto Weights = Cloth.GetWeightMap(TEXT("MaxDistance"));
    const auto Lookup = Cloth.GetSimVertex3DLookup();
    if (Cloth.GetNumSimPatterns() != 2 || Cloth.GetNumSimVertices2D() != 462 || Rest.Num() != 441 ||
        Cloth.GetNumSimFaces() != 800 || Cloth.GetNumSeams() != 1 || Cloth.GetSeam(0).GetNumSeamStitches() != 21)
    { Error = TEXT("Saved cloth topology is not the two-panel fixture"); return false; }
    for (const FIntVector2& Pair : Cloth.GetSeam(0).GetSeamStitch2DEndIndices())
        if (Lookup[Pair.X] != Lookup[Pair.Y]) { Error = TEXT("Seam is not welded in native collection"); return false; }
    const FInput Input = MakeInput();
    for (int32 P = 0; P < 2; ++P)
    {
        const auto Original2D = Cloth.GetSimPattern(P).GetSimPosition2D();
        for (int32 I = 0; I < Original2D.Num(); ++I)
        {
            if (!Original2D[I].Equals(Input.Panels[P].Source2D[I], 1.e-5f))
            { Error = TEXT("2D source changed"); return false; }
            const int32 Global2D = P * 231 + I;
            if (!Rest[Lookup[Global2D]].Equals(Input.Panels[P].Initial3D[I], 1.e-5f))
            { Error = TEXT("Saved initial 3D placement changed"); return false; }
        }
    }
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetTransactional(false));
    UChaosClothComponent* Component = NewObject<UChaosClothComponent>();
    Component->SetAsset(Asset);
    Component->SetSimulateInEditor(true);
    Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->SetForcedLOD(1);
    Scene.AddComponent(Component, FTransform::Identity);
    if (bSphere)
    {
        UStaticMeshComponent* Sphere = NewObject<UStaticMeshComponent>();
        Sphere->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Visible witness; solver uses PA_Sphere only.
        Scene.AddComponent(Sphere, FTransform(FQuat::Identity, SphereCenter, FVector(SphereRadius / 50.f)));
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    GShaderCompilingManager->FinishAllCompilation();
    if (!static_cast<USceneComponent*>(Component)->IsVisible()) { Error = TEXT("Native cloth surface is hidden after component registration"); return false; }
    Component->MarkRenderStateDirty();
    Scene.GetWorld()->SendAllEndOfFrameUpdates();
    FlushRenderingCommands();
    Tick(Component, 0.f);
    TArray<FVector3f> Initial(Rest), Previous, Current;
    if (!ReadPositions(Component, Current, Error)) return false;
    for (int32 I = 0; I < Current.Num(); ++I)
        if (Current[I].ContainsNaN() || !Current[I].Equals(Initial[I], 1.e-3f))
        { Error = TEXT("Native initial state differs from source placement"); return false; }
    const FString Prefix = Output / (bSphere ? TEXT("sphere") : TEXT("free"));
    if (!Capture(Scene, Component, Cloth, Initial, bSphere, Prefix + TEXT("-initial.png"), Error)) return false;
    Previous = Initial;
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        Tick(Component, Dt);
        if (!ReadPositions(Component, Current, Error)) return false;
        for (int32 I = 0; I < Current.Num(); ++I)
        {
            const FVector3f P = Current[I];
            if (P.ContainsNaN() || P.Size() > 1000.f) { Error = TEXT("Non-finite or runaway simulation"); return false; }
            const float Motion = (P - Previous[I]).Size();
            M.MaxFrameMotion = FMath::Max(M.MaxFrameMotion, Motion);
            if (Step >= 540) M.LastSecondMaxMotion = FMath::Max(M.LastSecondMaxMotion, Motion);
            if (Weights[I] == 0.f) M.MaxPinError = FMath::Max(M.MaxPinError, (P - Rest[I]).Size());
            const float Clearance = (FVector(P) - SphereCenter).Size() - SphereRadius;
            if (bSphere)
            {
                M.MaxPenetration = FMath::Max(M.MaxPenetration, -Clearance);
                if (Clearance < 1.5f) ++M.ContactSamples;
            }
        }
        for (const FIntVector3& T : Cloth.GetSimIndices3D())
            for (int32 K = 0; K < 3; ++K)
            {
                const int32 A = T[K], B = T[(K + 1) % 3];
                M.MaxEdgeRatio = FMath::Max(M.MaxEdgeRatio, (Current[A] - Current[B]).Size() / (Rest[A] - Rest[B]).Size());
            }
        Previous = Current;
    }
    M.Final = Current;
    int32 FreeCount = 0;
    for (int32 I = 0; I < Current.Num(); ++I)
        if (Weights[I] > 0.f) { M.MeanDrop += Rest[I].Z - Current[I].Z; ++FreeCount; }
    M.MeanDrop /= FMath::Max(1, FreeCount);
    const FClothSimulationProxy* Proxy = Component->GetClothSimulationProxy();
    UE_LOG(LogTemp, Display, TEXT("Probe %s: pins=%d dynamic=%d substeps=%d iterations=%d drop=%.6f pin=%.6f stretch=%.6f motion=%.6f last=%.6f penetration=%.6f contacts=%d"),
        bSphere ? TEXT("sphere") : TEXT("free"), Proxy->GetNumKinematicParticles(), Proxy->GetNumDynamicParticles(), Proxy->GetNumSubsteps(), Proxy->GetNumIterations(),
        M.MeanDrop, M.MaxPinError, M.MaxEdgeRatio, M.MaxFrameMotion, M.LastSecondMaxMotion, M.MaxPenetration, M.ContactSamples);
    if (Proxy->GetNumKinematicParticles() != 21 || Proxy->GetNumDynamicParticles() != 420 ||
        M.MeanDrop < 10.f || M.MaxPinError > 0.01f || M.MaxEdgeRatio > 1.25f || M.MaxFrameMotion > 20.f ||
        M.LastSecondMaxMotion > 1.f || (bSphere && (M.MaxPenetration > 0.25f || M.ContactSamples == 0)))
    { Error = TEXT("Native simulation failed deformation/pin/stability/collision thresholds (see Probe metrics)"); return false; }
    if (!Capture(Scene, Component, Cloth, Current, bSphere, Prefix + TEXT("-10s.png"), Error)) return false;
    if (bSphere && !Capture(Scene, Component, Cloth, Current, true, Prefix + TEXT("-10s-back.png"), Error, true)) return false;

    Component->RecreateClothSimulationProxy();
    Tick(Component, 0.f);
    if (!ReadPositions(Component, Current, Error)) return false;
    for (int32 I = 0; I < Current.Num(); ++I)
    {
        if (Current[I].ContainsNaN()) { Error = TEXT("Non-finite reset"); return false; }
        M.ResetError = FMath::Max(M.ResetError, (Current[I] - Rest[I]).Size());
    }
    if (M.ResetError > 1.e-3f) { Error = TEXT("Hard reset did not restore initial 3D positions"); return false; }
    if (!Capture(Scene, Component, Cloth, Current, bSphere, Prefix + TEXT("-reset.png"), Error)) return false;
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        Tick(Component, Dt);
        if (!ReadPositions(Component, Current, Error)) return false;
        for (const FVector3f& P : Current)
            if (P.ContainsNaN() || P.Size() > 1000.f) { Error = TEXT("Non-finite or runaway replay"); return false; }
    }
    for (int32 I = 0; I < Current.Num(); ++I) M.RepeatError = FMath::Max(M.RepeatError, (Current[I] - M.Final[I]).Size());
    if (M.RepeatError > 0.05f) { Error = FString::Printf(TEXT("Reset replay mismatch %.6f cm"), M.RepeatError); return false; }
    for (int32 I = 0; I < Rest.Num(); ++I)
        if (!Rest[I].Equals(Initial[I], 1.e-5f)) { Error = TEXT("Simulation overwrote source positions"); return false; }

    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetBoolField(TEXT("sphere_enabled"), bSphere);
    Result->SetNumberField(TEXT("dt_seconds"), Dt);
    Result->SetNumberField(TEXT("mean_free_drop_cm"), M.MeanDrop);
    Result->SetNumberField(TEXT("max_pin_error_cm"), M.MaxPinError);
    Result->SetNumberField(TEXT("max_edge_ratio"), M.MaxEdgeRatio);
    Result->SetNumberField(TEXT("max_step_motion_cm"), M.MaxFrameMotion);
    Result->SetNumberField(TEXT("last_second_max_step_motion_cm"), M.LastSecondMaxMotion);
    Result->SetNumberField(TEXT("max_vertex_sphere_penetration_cm"), M.MaxPenetration);
    Result->SetNumberField(TEXT("contact_vertex_samples"), M.ContactSamples);
    Result->SetNumberField(TEXT("hard_reset_error_cm"), M.ResetError);
    Result->SetNumberField(TEXT("replay_max_difference_cm"), M.RepeatError);
    Result->SetArrayField(TEXT("initial_positions"), JsonPositions(Initial));
    Result->SetArrayField(TEXT("final_positions"), JsonPositions(M.Final));
    FString Json;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
    if (!FFileHelper::SaveStringToFile(Json, *(Prefix + TEXT("-metrics.json")))) { Error = TEXT("Metrics write failed"); return false; }
    return true;
}

bool Run(FString& Error)
{
    const bool bReopen = FParse::Param(FCommandLine::Get(), TEXT("ProbeReopen"));
    FString Output;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ProbeOutput="), Output) || FPaths::IsRelative(Output))
    { Error = TEXT("Supply absolute -ProbeOutput= directory under this host Saved/Probe"); return false; }
    const FString Allowed = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Probe"));
    if (!FPaths::IsUnderDirectory(Output, Allowed)) { Error = TEXT("Output must stay under this host Saved/Probe"); return false; }
    IFileManager::Get().MakeDirectory(*Output, true);
    const FInput Input = MakeInput();
    if (!Validate(Input, Error)) return false;
    UChaosClothAsset* Free = nullptr;
    UChaosClothAsset* Sphere = nullptr;
    if (bReopen)
    {
        Free = LoadObject<UChaosClothAsset>(nullptr, *(FString(AssetRoot) + TEXT("CA_Free.CA_Free")));
        Sphere = LoadObject<UChaosClothAsset>(nullptr, *(FString(AssetRoot) + TEXT("CA_Sphere.CA_Sphere")));
        if (!Free || !Sphere || !Sphere->GetPhysicsAsset()) { Error = TEXT("Saved cloth/physics assets missing in new process"); return false; }
    }
    else
    {
        UPhysicsAsset* Physics = CreateSphere(Error);
        if (!Physics) return false;
        Free = CreateCloth(Input, nullptr, TEXT("CA_Free"), Error);
        Sphere = CreateCloth(Input, Physics, TEXT("CA_Sphere"), Error);
        if (!Free || !Sphere) return false;
    }
    const UPhysicsAsset* Physics = Sphere->GetPhysicsAsset();
    if (Free->GetPhysicsAsset() || !Physics || Physics->SkeletalBodySetups.Num() != 1 ||
        Physics->SkeletalBodySetups[0]->BoneName != TEXT("Root") ||
        Physics->SkeletalBodySetups[0]->AggGeom.GetElementCount() != 1 ||
        Physics->SkeletalBodySetups[0]->AggGeom.SphereElems.Num() != 1)
    { Error = TEXT("Expected no collider in CA_Free and exactly one Root sphere in CA_Sphere"); return false; }
    const FKSphereElem& SavedSphere = Physics->SkeletalBodySetups[0]->AggGeom.SphereElems[0];
    if (!SavedSphere.Center.Equals(SphereCenter) || !FMath::IsNearlyEqual(SavedSphere.Radius, SphereRadius))
    { Error = TEXT("Saved collider differs from measured collider"); return false; }
    FRunMetrics FreeMetrics, SphereMetrics;
    if (!Simulate(Free, false, Output, FreeMetrics, Error) || !Simulate(Sphere, true, Output, SphereMetrics, Error)) return false;
    // Positive collision control: the same no-collider run must enter the sphere volume.
    float ControlPenetration = 0, MaxDifference = 0;
    for (int32 I = 0; I < FreeMetrics.Final.Num(); ++I)
    {
        ControlPenetration = FMath::Max(ControlPenetration, SphereRadius - float((FVector(FreeMetrics.Final[I]) - SphereCenter).Size()));
        MaxDifference = FMath::Max(MaxDifference, (FreeMetrics.Final[I] - SphereMetrics.Final[I]).Size());
    }
    UE_LOG(LogTemp, Display, TEXT("Probe collision control: no-collider penetration=%.6f cm, max result difference=%.6f cm, reopen=%d"), ControlPenetration, MaxDifference, bReopen);
    if (ControlPenetration < 2.f || MaxDifference < 2.f) { Error = TEXT("Collision positive control ineffective"); return false; }
    TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
    Summary->SetBoolField(TEXT("reopened_existing_assets"), bReopen);
    Summary->SetNumberField(TEXT("no_collider_control_penetration_cm"), ControlPenetration);
    Summary->SetNumberField(TEXT("collision_max_position_difference_cm"), MaxDifference);
    Summary->SetStringField(TEXT("collision_scope"), TEXT("Vertex samples each 1/60s, not triangle penetration or substep CCD proof"));
    FString Json;
    FJsonSerializer::Serialize(Summary, TJsonWriterFactory<>::Create(&Json));
    if (!FFileHelper::SaveStringToFile(Json, *(Output / TEXT("summary.json")))) { Error = TEXT("Summary write failed"); return false; }
    return true;
}
}
