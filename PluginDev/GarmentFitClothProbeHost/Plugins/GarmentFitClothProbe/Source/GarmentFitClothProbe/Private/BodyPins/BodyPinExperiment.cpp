#include "BodyPins/BodyPinProbe.h"
#include "ProbeSupport.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "ChaosClothAsset/CollectionClothSeamFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Components/StaticMeshComponent.h"
#include "PreviewScene.h"
#include "DrawDebugHelpers.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"

namespace GarmentFitClothProbe::BodyPins
{
using namespace UE::Chaos::ClothAsset;
static const FString Root(TEXT("/GarmentFitClothProbe/BodyPins/"));
static constexpr float Dt = 1.f / 60.f;

template<typename T> static T* LoadSource(const FString& Name)
{
    if (!FPackageName::DoesPackageExist(Root + Name)) return nullptr;
    return LoadObject<T>(nullptr, *(Root + Name + TEXT(".") + Name));
}

template<typename T> static T* SourceObject(const FString& Name)
{
    UPackage* Package = CreatePackage(*(Root + Name));
    Package->FullyLoad();
    T* Result = FindObject<T>(Package, *Name);
    return Result ? Result : NewObject<T>(Package, *Name, RF_Public | RF_Standalone);
}

static UMaterial* MakeMaterial(const FString& Name, FLinearColor Color, bool bCloth, FString& Error)
{
    UMaterial* Material = SourceObject<UMaterial>(Name);
    Material->GetExpressionCollection().Empty();
    UMaterialExpressionConstant3Vector* Base = NewObject<UMaterialExpressionConstant3Vector>(Material);
    Base->Constant = Color;
    Material->GetExpressionCollection().AddExpression(Base);
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Base);
    UMaterialExpressionConstant* Roughness = NewObject<UMaterialExpressionConstant>(Material);
    Roughness->R = 0.85f;
    Material->GetExpressionCollection().AddExpression(Roughness);
    Material->GetEditorOnlyData()->Roughness.Connect(0, Roughness);
    Material->TwoSided = bCloth;
    if (bCloth)
    {
        Material->SetMaterialUsage(MATUSAGE_Clothing);
        Material->SetMaterialUsage(MATUSAGE_SkeletalMesh);
    }
    Material->PostEditChange();
    return Support::SaveSourceAsset(Material, Error) ? Material : nullptr;
}

static UStaticMesh* PrepareBody(bool bReopen, FString& Error)
{
    UStaticMesh* Mesh = LoadSource<UStaticMesh>(TEXT("SM_LunaBindPose"));
    if (bReopen)
    {
        if (!Mesh || !LoadSource<UMaterial>(TEXT("M_PinBody")) || !LoadSource<UMaterial>(TEXT("M_PinCloth")))
            Error = TEXT("Saved independent body/materials missing");
        return Error.IsEmpty() ? Mesh : nullptr;
    }
    UMaterial* BodyMaterial = MakeMaterial(TEXT("M_PinBody"), FLinearColor(0.36f, 0.39f, 0.43f), false, Error);
    if (!BodyMaterial || !MakeMaterial(TEXT("M_PinCloth"), FLinearColor(0.07f, 0.48f, 0.63f), true, Error)) return nullptr;
    if (!Mesh)
    {
        const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() /
            TEXT("GarmentFitClothProbe/Content/BodyPins/Source/SKM_LunaMk2_Mannequin_69583eb.fbx"));
        if (IFileManager::Get().FileSize(*File) != 2837596)
        { Error = TEXT("Fixed mannequin FBX snapshot missing or wrong size"); return nullptr; }
        UFbxFactory* Factory = NewObject<UFbxFactory>();
        Factory->SetDetectImportTypeOnImport(false);
        UFbxImportUI* Options = Factory->ImportUI;
        Options->bAutomatedImportShouldDetectType = false;
        Options->MeshTypeToImport = FBXIT_StaticMesh;
        Options->bImportAsSkeletal = false;
        Options->bImportMesh = true;
        Options->bImportMaterials = false;
        Options->bImportTextures = false;
        Options->StaticMeshImportData->bCombineMeshes = true;
        Options->StaticMeshImportData->bAutoGenerateCollision = false;
        Options->StaticMeshImportData->bBuildNanite = false;
        Options->StaticMeshImportData->bGenerateLightmapUVs = false;
        // The untextured mannequin keeps imported normals; UV-singular tangent
        // bases do not need MikkTSpace. BuiltIn avoids its UV-dependent warning.
        Options->StaticMeshImportData->NormalImportMethod = FBXNIM_ImportNormals;
        Options->StaticMeshImportData->NormalGenerationMethod = EFBXNormalGenerationMethod::BuiltIn;
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        Task->Filename = File; Task->DestinationPath = Root.LeftChop(1);
        Task->DestinationName = TEXT("SM_LunaBindPose");
        Task->bAutomated = true; Task->bSave = false; Task->bAsync = false;
        Task->Factory = Factory; Task->Options = Options;
        FAssetToolsModule::GetModule().Get().ImportAssetTasks({Task});
        for (UObject* Object : Task->GetObjects()) if (UStaticMesh* Imported = Cast<UStaticMesh>(Object)) Mesh = Imported;
        if (!Mesh || Mesh->GetOutermost()->GetName() != Root + TEXT("SM_LunaBindPose"))
        { Error = TEXT("Independent static bind-pose mannequin import failed or produced unexpected path"); return nullptr; }
    }
    Mesh->bAllowCPUAccess = true;
    FAssetCompilingManager::Get().FinishAllCompilation();
    for (int32 ModelIndex = 0; ModelIndex < Mesh->GetNumSourceModels(); ++ModelIndex)
    {
        FStaticMeshSourceModel& Model = Mesh->GetSourceModel(ModelIndex);
        Model.BuildSettings.bUseMikkTSpace = false;
        Model.BuildSettings.bRecomputeTangents = true;
        Model.BuildSettings.bRecomputeNormals = false;
    }
    for (FStaticMaterial& Slot : Mesh->GetStaticMaterials()) Slot.MaterialInterface = BodyMaterial;
    Mesh->PostEditChange();
    FAssetCompilingManager::Get().FinishAllCompilation();
    return Support::SaveSourceAsset(Mesh, Error) ? Mesh : nullptr;
}

static bool ExtractSurface(UStaticMesh* Mesh, FBodySurface& Surface, FString& Error)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty())
    { Error = TEXT("Body has no render geometry"); return false; }
    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
    Surface = {}; Surface.Mesh = FSoftObjectPath(Mesh);
    for (uint32 I = 0; I < LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices(); ++I)
        Surface.Positions.Add(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I));
    const FIndexArrayView Indices = LOD.IndexBuffer.GetArrayView();
    for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
    {
        const FIntVector3 T(Indices[I], Indices[I + 1], Indices[I + 2]);
        Surface.Triangles.Add(T);
        Surface.TriangleNormals.Add((LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(T.X) +
            LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(T.Y) +
            LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(T.Z)).GetSafeNormal());
    }
    if (Surface.Positions.Num() < 100 || Surface.Triangles.Num() < 100)
    { Error = TEXT("Imported mannequin unexpectedly lacks body geometry"); return false; }
    return true;
}

static int32 ChooseTriangle(const FBodySurface& Body, FVector Desired)
{
    int32 Best = INDEX_NONE;
    double BestScore = TNumericLimits<double>::Max();
    for (int32 I = 0; I < Body.Triangles.Num(); ++I)
    {
        const FIntVector3 T = Body.Triangles[I];
        const FVector3f N = Body.TriangleNormals[I];
        if (N.Y < 0.55f) continue; // Explicit +Y-facing surface of the bind-pose body.
        if (FVector3f::CrossProduct(Body.Positions[T.Y] - Body.Positions[T.X], Body.Positions[T.Z] - Body.Positions[T.X]).SizeSquared() < 1.e-10f) continue;
        const FVector Center((Body.Positions[T.X] + Body.Positions[T.Y] + Body.Positions[T.Z]) / 3.f);
        const double Score = FMath::Square(Center.X - Desired.X) + FMath::Square(Center.Z - Desired.Z) * 2. +
            FMath::Square(Center.Y - Desired.Y) * 0.15;
        if (Score < BestScore) { Best = I; BestScore = Score; }
    }
    return Best;
}

static UGarmentProbeBodyPinSet* MakeSet(const FString& Name, UStaticMesh* Body)
{
    UGarmentProbeBodyPinSet* Set = SourceObject<UGarmentProbeBodyPinSet>(Name);
    Set->BodyMesh = Body; Set->BodyTransform = FTransform::Identity;
    Set->Pins.Reset(); Set->AuthoredPins.Reset();
    Set->SourceCommit = TEXT("69583eb"); Set->SourceBlob = TEXT("8d7916124c01666172a92bbb4b424dfbaa9d0237");
    Set->SnapshotUtc = TEXT("2026-10-16T17:42:06.1148485Z");
    return Set;
}

static void CopySetState(const UGarmentProbeBodyPinSet& Source, UGarmentProbeBodyPinSet& Target)
{
    Target.BodyMesh = Source.BodyMesh; Target.BodyTransform = Source.BodyTransform;
    Target.Pins = Source.Pins; Target.AuthoredPins = Source.AuthoredPins;
    Target.SourceCommit = Source.SourceCommit; Target.SourceBlob = Source.SourceBlob; Target.SnapshotUtc = Source.SnapshotUtc;
}

static void SetProxySpheres(FBodySurface& Body, TConstArrayView<FResolvedPin> Pins)
{
    Body.CollisionSpheres.Reset();
    // Local 4 cm tangent spheres only: sparse contact proxies, not a torso fit.
    for (const FResolvedPin& Pin : Pins)
        Body.CollisionSpheres.Emplace(FVector(Pin.BodyLocalTarget - Pin.BodyLocalNormal * 5.5f), 4.0);
}

static UPhysicsAsset* MakePhysics(const FString& Name, const FBodySurface& Body, FString& Error)
{
    UPhysicsAsset* Physics = SourceObject<UPhysicsAsset>(Name);
    Physics->SkeletalBodySetups.Reset();
    USkeletalBodySetup* Setup = NewObject<USkeletalBodySetup>(Physics);
    Setup->BoneName = TEXT("Root");
    for (const FSphere& Sphere : Body.CollisionSpheres)
    {
        FKSphereElem Element(Sphere.W); Element.Center = Sphere.Center;
        Setup->AggGeom.SphereElems.Add(Element);
    }
    Physics->SkeletalBodySetups.Add(Setup);
    Physics->UpdateBodySetupIndexMap(); Physics->UpdateBoundsBodiesArray();
    return Support::SaveSourceAsset(Physics, Error) ? Physics : nullptr;
}

static FInput MakePanels(FVector Center)
{
    FInput Input; Input.Panels.SetNum(2);
    for (int32 P = 0; P < 2; ++P)
    {
        FPanel& Panel = Input.Panels[P];
        for (int32 Y = 0; Y <= 12; ++Y)
            for (int32 X = 0; X <= 5; ++X)
            {
                Panel.Source2D.Emplace(X * 2.5f, Y * 2.5f);
                Panel.Initial3D.Add(FVector3f(Center + FVector((P * 5 + X) * 2.5 - 12.5, 0, -Y * 2.5)));
            }
        for (int32 Y = 0; Y < 12; ++Y)
            for (int32 X = 0; X < 5; ++X)
            {
                const int32 I = Y * 6 + X;
                Panel.Triangles.Emplace(I, I + 1, I + 7); Panel.Triangles.Emplace(I, I + 7, I + 6);
            }
    }
    for (int32 Y = 0; Y <= 12; ++Y) Input.Seam.Emplace(Y * 6 + 5, Y * 6);
    return Input;
}

struct FAssets
{
    UStaticMesh* Body = nullptr;
    UGarmentProbeBodyPinSet* Set = nullptr;
    UGarmentProbeBodyPinSet* Disabled = nullptr;
    UGarmentProbeBodyPinSet* Released = nullptr;
    UChaosClothAsset* Pinned = nullptr;
    UChaosClothAsset* Free = nullptr;
    FBodySurface Surface;
    TArray<FResolvedPin> Pins;
};

static bool ValidateNativeBinding(UChaosClothAsset* Asset, const UGarmentProbeBodyPinSet& Set,
    const FBodySurface& Body, FString& Error)
{
    const TSharedRef<const FManagedArrayCollection> Collection = static_cast<const UChaosClothAsset*>(Asset)->GetClothCollections()[0];
    const FCollectionClothConstFacade Cloth(Collection);
    TArray<FResolvedPin> Resolved;
    if (!ResolvePins(Set, Body, Collection, Resolved, Error)) return false;
    if (!Cloth.HasWeightMap(TEXT("MaxDistance"))) { Error = TEXT("Native pin weight map missing"); return false; }
    TSet<int32> Enabled;
    for (const FResolvedPin& Pin : Resolved)
        if (Pin.bEnabled)
        {
            Enabled.Add(Pin.WeldedVertex);
            if (!Cloth.GetSimPosition3D()[Pin.WeldedVertex].Equals(Pin.BodyLocalTarget, 1.e-4f))
            { Error = TEXT("Saved native pin placement does not match resolved document target"); return false; }
        }
    const auto Weights = Cloth.GetWeightMap(TEXT("MaxDistance"));
    if (Weights.Num() != Cloth.GetNumSimVertices3D()) { Error = TEXT("Native pin weight map count mismatch"); return false; }
    for (int32 I = 0; I < Weights.Num(); ++I)
        if (!FMath::IsNearlyEqual(Weights[I], Enabled.Contains(I) ? 0.f : 2000.f))
        { Error = TEXT("Saved native zero-mass selection differs from enabled document pin identities"); return false; }
    return true;
}

static bool Prepare(const FString& Prefix, UChaosClothAsset* Integration, const FRunContext& Context, FAssets& Assets, FString& Error)
{
    Error.Reset();
    Assets.Body = PrepareBody(Context.bReopen, Error);
    if (!Assets.Body || !ExtractSurface(Assets.Body, Assets.Surface, Error)) return false;
    if (Context.bReopen)
    {
        Assets.Set = LoadSource<UGarmentProbeBodyPinSet>(Prefix + TEXT("DA_Pins"));
        Assets.Disabled = LoadSource<UGarmentProbeBodyPinSet>(Prefix + TEXT("DA_Disabled"));
        Assets.Released = LoadSource<UGarmentProbeBodyPinSet>(Prefix + TEXT("DA_Released"));
        Assets.Pinned = LoadSource<UChaosClothAsset>(Prefix + TEXT("CA_Pinned"));
        Assets.Free = LoadSource<UChaosClothAsset>(Prefix + TEXT("CA_Free"));
        if (!Assets.Set || !Assets.Disabled || !Assets.Released || !Assets.Pinned || !Assets.Free)
        { Error = TEXT("Saved BodyPins document/lifecycle/native assets missing in fresh process"); return false; }
    }
    else
    {
        const FBox Bounds = Assets.Body->GetBoundingBox();
        const FVector Desired(Bounds.GetCenter().X, Bounds.Max.Y, Bounds.Min.Z + Bounds.GetSize().Z * 0.72);
        const int32 MidTriangle = ChooseTriangle(Assets.Surface, Desired);
        if (MidTriangle == INDEX_NONE) { Error = TEXT("No actual +Y-facing mannequin surface triangle"); return false; }
        const FIntVector3 T = Assets.Surface.Triangles[MidTriangle];
        const FVector Center((Assets.Surface.Positions[T.X] + Assets.Surface.Positions[T.Y] + Assets.Surface.Positions[T.Z]) / 3.f +
            Assets.Surface.TriangleNormals[MidTriangle] * 1.5f);
        TSharedPtr<FManagedArrayCollection> Collection;
        FVertexRef IntegrationAnchor{0,0};
        if (Integration)
        {
            const TSharedRef<const FManagedArrayCollection> ReadOnly = static_cast<const UChaosClothAsset*>(Integration)->GetClothCollections()[0];
            Collection = MakeShared<FManagedArrayCollection>(*ReadOnly);
            FCollectionClothFacade Cloth(Collection.ToSharedRef());
            if (Cloth.GetNumSeams() < 1 || Cloth.GetSeam(0).GetNumSeamStitches() < 1)
            { Error = TEXT("Combined input requires an existing native sewn seam"); return false; }
            const int32 Anchor2D = static_cast<const FCollectionClothConstFacade&>(Cloth).GetSeam(0).GetSeamStitch2DEndIndices()[0].X;
            for (int32 P = 0; P < Cloth.GetNumSimPatterns(); ++P)
            {
                const auto Pattern = Cloth.GetSimPattern(P);
                if (Anchor2D >= Pattern.GetSimVertices2DOffset() && Anchor2D < Pattern.GetSimVertices2DOffset() + Pattern.GetNumSimVertices2D())
                    IntegrationAnchor = {P, Anchor2D - Pattern.GetSimVertices2DOffset()};
            }
            const int32 Anchor3D = Cloth.GetSimVertex3DLookup()[Anchor2D];
            const FVector Anchor(Cloth.GetSimPosition3D()[Anchor3D]);
            const FIntVector3 Face = Cloth.GetSimIndices3D()[0];
            const FVector Normal = FVector::CrossProduct(FVector(Cloth.GetSimPosition3D()[Face.Y] - Cloth.GetSimPosition3D()[Face.X]),
                FVector(Cloth.GetSimPosition3D()[Face.Z] - Cloth.GetSimPosition3D()[Face.X])).GetSafeNormal();
            const FQuat Rotation = FQuat::FindBetweenNormals(Normal, FVector(0,1,0));
            for (FVector3f& Position : Cloth.GetSimPosition3D()) Position = FVector3f(Rotation.RotateVector(FVector(Position) - Anchor) + Center);
            // Explicit rigid placement only. Original per-panel 2D rest geometry is unchanged.
        }
        else
        {
            Collection = Support::BuildPanelCollection(MakePanels(Center), Error);
            if (!Collection) return false;
            Support::ApplyDefaultConfig(Collection.ToSharedRef());
        }
        Assets.Set = MakeSet(Prefix + TEXT("DA_Pins"), Assets.Body);
        const TArray<FVertexRef> References = Integration ? TArray<FVertexRef>{IntegrationAnchor} : TArray<FVertexRef>{{0,0},{0,5},{1,5}};
        for (int32 I = 0; I < References.Num(); ++I)
        {
            FGarmentProbeBodyPin Pin;
            Pin.Id = FName(*FString::Printf(TEXT("SurfacePin%d"), I));
            Pin.PanelIndex = References[I].PanelIndex; Pin.VertexIndex = References[I].VertexIndex;
            Pin.BodyTriangle = Integration ? MidTriangle : ChooseTriangle(Assets.Surface, Desired + FVector((I - 1) * 12.5,0,0));
            if (!CreatePin(*Assets.Set, Pin, Assets.Surface, Collection.ToSharedRef(), Error)) return false;
        }
        if (!ResolvePins(*Assets.Set, Assets.Surface, Collection.ToSharedRef(), Assets.Pins, Error)) return false;
        SetProxySpheres(Assets.Surface, Assets.Pins);
        if (!ResolvePins(*Assets.Set, Assets.Surface, Collection.ToSharedRef(), Assets.Pins, Error)) return false;
        UPhysicsAsset* Physics = MakePhysics(Prefix + TEXT("PA_ContactProxies"), Assets.Surface, Error);
        if (!Physics) return false;
        FCollectionClothFacade Cloth(Collection.ToSharedRef());
        Cloth.SetPhysicsAssetSoftObjectPathName(FSoftObjectPath(Physics));
        Cloth.AddWeightMap(TEXT("MaxDistance"));
        for (float& Weight : Cloth.GetWeightMap(TEXT("MaxDistance"))) Weight = 2000.f;
        FClothGeometryTools::CopySimMeshToRenderMesh(Collection.ToSharedRef(), FSoftObjectPath(Root + TEXT("M_PinCloth.M_PinCloth")), false);
        UChaosClothAsset* Source = Support::BuildNativeAsset(Collection.ToSharedRef(), Root + Prefix + TEXT("CA_Placement"), Error);
        if (!Source) return false;
        Assets.Pinned = BuildPinAsset(Source, *Assets.Set, Assets.Surface, Root + Prefix + TEXT("CA_Pinned"), Error);
        if (!Assets.Pinned) return false;
        Assets.Disabled = MakeSet(Prefix + TEXT("DA_Disabled"), Assets.Body);
        CopySetState(*Assets.Set, *Assets.Disabled);
        for (const FGarmentProbeBodyPin& Pin : Assets.Set->Pins)
            if (!SetPinEnabled(*Assets.Disabled, Pin.Id, false, Error)) return false;
        // Free control uses exactly the already-targeted pinned placement.
        Assets.Free = BuildPinAsset(Assets.Pinned, *Assets.Disabled, Assets.Surface, Root + Prefix + TEXT("CA_Free"), Error);
        if (!Assets.Free) return false;
        Assets.Released = MakeSet(Prefix + TEXT("DA_Released"), Assets.Body);
        CopySetState(*Assets.Set, *Assets.Released);
        for (const FGarmentProbeBodyPin& Pin : Assets.Set->Pins)
            if (!ReleasePin(*Assets.Released, Pin.Id, Error)) return false;
        if (!Support::SaveSourceAsset(Assets.Set, Error) || !Support::SaveSourceAsset(Assets.Disabled, Error) ||
            !Support::SaveSourceAsset(Assets.Released, Error)) return false;
    }
    if (Assets.Set->SourceBlob != TEXT("8d7916124c01666172a92bbb4b424dfbaa9d0237") ||
        Assets.Set->SourceCommit != TEXT("69583eb") || Assets.Set->SnapshotUtc != TEXT("2026-10-16T17:42:06.1148485Z") ||
        !Assets.Set->BodyTransform.Equals(FTransform::Identity, 1.e-6) || Assets.Set->Pins.Num() != (Integration ? 1 : 3))
    { Error = TEXT("Saved pin lifecycle or fixed-source provenance mismatch"); return false; }
    if (!ValidateSavedLifecycle(*Assets.Set, *Assets.Disabled, *Assets.Released, Assets.Surface,
        static_cast<const UChaosClothAsset*>(Assets.Pinned)->GetClothCollections()[0], Error)) return false;
    if (!ResolvePins(*Assets.Set, Assets.Surface, static_cast<const UChaosClothAsset*>(Assets.Pinned)->GetClothCollections()[0], Assets.Pins, Error)) return false;
    SetProxySpheres(Assets.Surface, Assets.Pins);
    if (!ResolvePins(*Assets.Set, Assets.Surface, static_cast<const UChaosClothAsset*>(Assets.Pinned)->GetClothCollections()[0], Assets.Pins, Error)) return false;
    if (!ValidateNativeBinding(Assets.Pinned, *Assets.Set, Assets.Surface, Error) ||
        !ValidateNativeBinding(Assets.Free, *Assets.Disabled, Assets.Surface, Error) ||
        !ValidateNativeBinding(Assets.Free, *Assets.Released, Assets.Surface, Error)) return false;
    const UPhysicsAsset* Physics = Assets.Pinned->GetPhysicsAsset();
    if (!Physics || Physics->SkeletalBodySetups.Num() != 1 || Physics->SkeletalBodySetups[0]->AggGeom.SphereElems.Num() != Assets.Pins.Num())
    { Error = TEXT("Native body contact proxies missing"); return false; }
    for (int32 I = 0; I < Assets.Pins.Num(); ++I)
    {
        const FKSphereElem& S = Physics->SkeletalBodySetups[0]->AggGeom.SphereElems[I];
        if (!S.Center.Equals(Assets.Surface.CollisionSpheres[I].Center, 1.e-4) || !FMath::IsNearlyEqual(S.Radius, 4.f))
        { Error = TEXT("Saved collision sphere differs from measured body-local proxy"); return false; }
    }
    return true;
}

static TArray<TSharedPtr<FJsonValue>> JsonPositions(TConstArrayView<FVector3f> Positions)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FVector3f& P : Positions)
        Result.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y), MakeShared<FJsonValueNumber>(P.Z)}));
    return Result;
}

static bool WriteJson(const TSharedRef<FJsonObject>& Object, const FString& File, FString& Error)
{
    FString Text;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
    if (!FFileHelper::SaveStringToFile(Text, *File)) { Error = TEXT("BodyPins metrics write failed: ") + File; return false; }
    return true;
}

static bool CaptureState(FPreviewScene& Scene, UChaosClothComponent* Component, const FAssets& Assets,
    const FTransform& BodyTransform, TConstArrayView<FVector3f> Positions, bool bEnabled, const FString& File, FString& Error)
{
    if (!Support::EnsureClothRenderReady(Component, Error)) return false;
    FlushPersistentDebugLines(Scene.GetWorld());
    const FCollectionClothConstFacade Cloth(static_cast<const UChaosClothAsset*>(Assets.Pinned)->GetClothCollections()[0]);
    for (const FIntVector3& T : Cloth.GetSimIndices3D())
        for (int32 K = 0; K < 3; ++K)
            DrawDebugLine(Scene.GetWorld(), FVector(Positions[T[K]]), FVector(Positions[T[(K+1)%3]]),
                FColor(35,130,180), true, -1, 0, 0.12f);
    for (const FResolvedPin& Pin : Assets.Pins)
    {
        FVector Target;
        if (!WorldTarget(Pin, BodyTransform, Target, Error)) return false;
        DrawDebugSphere(Scene.GetWorld(), Target, 1.0, 12, bEnabled ? FColor::Green : FColor::Red, true, -1, 0, 0.4f);
        DrawDebugPoint(Scene.GetWorld(), FVector(Positions[Pin.WeldedVertex]), 11.f, bEnabled ? FColor::Green : FColor::Orange, true);
        DrawDebugLine(Scene.GetWorld(), Target, Target + BodyTransform.TransformVectorNoScale(FVector(Pin.BodyLocalNormal)) * 5., FColor::Yellow, true, -1, 0, 0.5f);
    }
    FBox Bounds = Assets.Body->GetBoundingBox().TransformBy(BodyTransform);
    for (const FVector3f& P : Positions) Bounds += FVector(P);
    const double Size = FMath::Max(90., Bounds.GetSize().GetMax());
    const FVector LookAt = Bounds.GetCenter();
    Component->MarkRenderDynamicDataDirty();
    Scene.GetWorld()->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
    return Support::CapturePreview(Scene, File, LookAt + FVector(Size * 0.85, Size * 1.65, Size * 0.55), LookAt, Error);
}

static bool AssertNativeCount(UChaosClothComponent* Component, int32 ExpectedPins, int32 ExpectedVertices, FString& Error)
{
    const FClothSimulationProxy* Proxy = Component->GetClothSimulationProxy();
    if (!Proxy || Proxy->GetNumKinematicParticles() != ExpectedPins || Proxy->GetNumDynamicParticles() != ExpectedVertices - ExpectedPins)
    {
        Error = FString::Printf(TEXT("Native body pin mass count mismatch: expected %d/%d, actual %d/%d"),
            ExpectedPins, ExpectedVertices - ExpectedPins, Proxy ? Proxy->GetNumKinematicParticles() : -1, Proxy ? Proxy->GetNumDynamicParticles() : -1);
        return false;
    }
    return true;
}

static bool RunNative(const FString& Prefix, const FRunContext& Context, FAssets& Assets, FString& Error)
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    GShaderCompilingManager->FinishAllCompilation();
    const FCollectionClothConstFacade Pinned(static_cast<const UChaosClothAsset*>(Assets.Pinned)->GetClothCollections()[0]);
    const FCollectionClothConstFacade Free(static_cast<const UChaosClothAsset*>(Assets.Free)->GetClothCollections()[0]);
    if (Pinned.GetNumSimVertices3D() != Free.GetNumSimVertices3D()) { Error = TEXT("Pin/free topology differs"); return false; }
    for (int32 I = 0; I < Pinned.GetNumSimVertices3D(); ++I)
        if (!Pinned.GetSimPosition3D()[I].Equals(Free.GetSimPosition3D()[I], 1.e-5f))
        { Error = TEXT("No-pin control does not use identical initial placement"); return false; }
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetTransactional(false));
    UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>();
    Body->SetStaticMesh(Assets.Body); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Scene.AddComponent(Body, Assets.Set->BodyTransform);
    UChaosClothComponent* Component = NewObject<UChaosClothComponent>();
    Component->SetAsset(Assets.Pinned); Component->SetSimulateInEditor(true);
    Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->SetForcedLOD(1);
    Component->SetTeleportDistanceThreshold(0.f); Component->SetTeleportRotationThreshold(0.f);
    Scene.AddComponent(Component, Assets.Set->BodyTransform);
    if (!static_cast<USceneComponent*>(Component)->IsVisible() || !Body->IsVisible())
    { Error = TEXT("Native cloth or mannequin is hidden"); return false; }
    Component->MarkRenderStateDirty(); Body->MarkRenderStateDirty();
    Scene.GetWorld()->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
    Support::TickCloth(Component, 0.f);
    TArray<FVector3f> Current;
    if (!Support::ReadNativePositions(Component, Current, Error) ||
        !AssertNativeCount(Component, Assets.Pins.Num(), Pinned.GetNumSimVertices3D(), Error)) return false;
    const FString Output = Context.OutputDirectory;
    IFileManager::Get().MakeDirectory(*Output, true);
    if (!CaptureState(Scene, Component, Assets, Assets.Set->BodyTransform, Current, true, Output / (Prefix + TEXT("initial.png")), Error)) return false;
    double MaxPinError = 0., MaxProxyPenetration = 0., TargetWorldMotion = 0.;
    FTransform BodyTransform = Assets.Set->BodyTransform;
    for (int32 Step = 0; Step < 240; ++Step)
    {
        const double Alpha = FMath::Clamp((Step - 119.) / 120., 0., 1.);
        BodyTransform = FTransform(FRotator(0, Alpha * 45., 0), FVector(Alpha * 30., Alpha * -10., Alpha * 5.));
        // This is the body binding: native root-skinning receives the same rigid
        // component transform as the visible body. No simulated position is edited.
        Body->SetWorldTransform(BodyTransform);
        Component->SetWorldTransform(BodyTransform);
        Support::TickCloth(Component, Dt);
        if (!Support::ReadNativePositions(Component, Current, Error)) return false;
        for (const FResolvedPin& Pin : Assets.Pins)
        {
            FVector Target;
            if (!WorldTarget(Pin, BodyTransform, Target, Error)) return false;
            MaxPinError = FMath::Max(MaxPinError, FVector::Distance(FVector(Current[Pin.WeldedVertex]), Target));
            TargetWorldMotion = FMath::Max(TargetWorldMotion, FVector::Distance(Target, FVector(Pin.BodyLocalTarget)));
        }
        for (const FVector3f& Position : Current)
        {
            if (Position.Size() > 5000.f) { Error = TEXT("Runaway native body pin simulation"); return false; }
            const FVector Local = BodyTransform.InverseTransformPosition(FVector(Position));
            for (const FSphere& Sphere : Assets.Surface.CollisionSpheres)
                MaxProxyPenetration = FMath::Max(MaxProxyPenetration, Sphere.W - FVector::Distance(Local, Sphere.Center));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BodyPins %s follow: pin_error=%.6f cm, world_motion=%.6f cm, proxy_penetration=%.6f cm"), *Prefix, MaxPinError, TargetWorldMotion, MaxProxyPenetration);
    if (MaxPinError > 0.05 || TargetWorldMotion < 15. || MaxProxyPenetration > 0.5)
    { Error = TEXT("Native body-following pin/collision thresholds failed; see BodyPins metrics log"); return false; }
    TSharedRef<FJsonObject> Follow = MakeShared<FJsonObject>();
    Follow->SetNumberField(TEXT("max_pin_target_error_cm"), MaxPinError);
    Follow->SetNumberField(TEXT("max_target_world_motion_cm"), TargetWorldMotion);
    Follow->SetNumberField(TEXT("max_vertex_proxy_penetration_cm"), MaxProxyPenetration);
    Follow->SetNumberField(TEXT("kinematic_particles"), Assets.Pins.Num());
    Follow->SetNumberField(TEXT("body_triangle_count"), Assets.Surface.Triangles.Num());
    Follow->SetArrayField(TEXT("final_positions"), JsonPositions(Current));
    Follow->SetStringField(TEXT("collision_scope"), TEXT("4 cm tangent sphere per pin; frame vertex samples only, not full mannequin, triangle intersections, or substep CCD"));
    if (!WriteJson(Follow, Output / (Prefix + TEXT("follow-metrics.json")), Error) ||
        !CaptureState(Scene, Component, Assets, BodyTransform, Current, true, Output / (Prefix + TEXT("follow.png")), Error)) return false;
    UGarmentProbeBodyPinSet* Session = NewObject<UGarmentProbeBodyPinSet>();
    CopySetState(*Assets.Set, *Session);
    auto MeasureFreeState = [&](const FString& State, bool bRelease) -> bool
    {
        const TArray<FVector3f> BeforeSwitch = Current;
        if (bRelease)
        {
            for (const FGarmentProbeBodyPin& Pin : Assets.Set->Pins)
                if (!ReleasePin(*Session, Pin.Id, Error)) return false;
        }
        else
        {
            for (const FGarmentProbeBodyPin& Pin : Assets.Set->Pins)
                if (!SetPinEnabled(*Session, Pin.Id, false, Error)) return false;
        }
        if (!ValidateNativeBinding(Assets.Free, *Session, Assets.Surface, Error)) return false;
        Component->SetAsset(Assets.Free); // Native mass reinitialization, explicitly discontinuous.
        Support::TickCloth(Component, 0.f);
        if (!Support::ReadNativePositions(Component, Current, Error) ||
            !AssertNativeCount(Component, 0, Pinned.GetNumSimVertices3D(), Error)) return false;
        double ResetJump = 0.;
        for (int32 I = 0; I < Current.Num(); ++I)
            ResetJump = FMath::Max(ResetJump, double((Current[I] - BeforeSwitch[I]).Size()));
        const TArray<FVector3f> FreeStart = Current;
        for (int32 Step = 0; Step < 45; ++Step) Support::TickCloth(Component, Dt);
        if (!Support::ReadNativePositions(Component, Current, Error)) return false;
        double MinFormerPinMovement = TNumericLimits<double>::Max();
        double MinFormerPinDrop = TNumericLimits<double>::Max();
        for (const FResolvedPin& Pin : Assets.Pins)
        {
            MinFormerPinMovement = FMath::Min(MinFormerPinMovement, double((Current[Pin.WeldedVertex] - FreeStart[Pin.WeldedVertex]).Size()));
            MinFormerPinDrop = FMath::Min(MinFormerPinDrop, double(FreeStart[Pin.WeldedVertex].Z - Current[Pin.WeldedVertex].Z));
        }
        if (MinFormerPinMovement < 5. || MinFormerPinDrop < 5.)
        { Error = State + TEXT(": previously pinned particles did not resume native gravity motion"); return false; }
        TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
        Metrics->SetNumberField(TEXT("kinematic_particles"), 0);
        Metrics->SetNumberField(TEXT("min_former_pin_movement_cm"), MinFormerPinMovement);
        Metrics->SetNumberField(TEXT("min_former_pin_drop_cm"), MinFormerPinDrop);
        Metrics->SetNumberField(TEXT("reset_discontinuity_max_cm"), ResetJump);
        Metrics->SetBoolField(TEXT("velocity_preserved_on_release"), false);
        Metrics->SetArrayField(TEXT("final_positions"), JsonPositions(Current));
        return WriteJson(Metrics, Output / (Prefix + State + TEXT("-metrics.json")), Error) &&
            CaptureState(Scene, Component, Assets, BodyTransform, Current, false, Output / (Prefix + State + TEXT(".png")), Error);
    };
    if (!MeasureFreeState(TEXT("disabled"), false)) return false;
    for (const FGarmentProbeBodyPin& Pin : Assets.Set->Pins)
        if (!SetPinEnabled(*Session, Pin.Id, true, Error)) return false;
    if (!ValidateNativeBinding(Assets.Pinned, *Session, Assets.Surface, Error)) return false;
    Component->SetAsset(Assets.Pinned); Support::TickCloth(Component, 0.f);
    if (!Support::ReadNativePositions(Component, Current, Error) || !AssertNativeCount(Component, Assets.Pins.Num(), Current.Num(), Error)) return false;
    for (int32 Step = 0; Step < 60; ++Step) Support::TickCloth(Component, Dt);
    if (!Support::ReadNativePositions(Component, Current, Error)) return false;
    if (!MeasureFreeState(TEXT("released"), true)) return false;
    ResetPins(*Session);
    if (!ValidateNativeBinding(Assets.Pinned, *Session, Assets.Surface, Error)) return false;
    Component->SetAsset(Assets.Pinned); Support::TickCloth(Component, 0.f);
    if (!Support::ReadNativePositions(Component, Current, Error) || !AssertNativeCount(Component, Session->Pins.Num(), Current.Num(), Error)) return false;
    double ResetError = 0.;
    for (int32 I = 0; I < Current.Num(); ++I)
        ResetError = FMath::Max(ResetError, FVector::Distance(FVector(Current[I]), BodyTransform.TransformPosition(FVector(Pinned.GetSimPosition3D()[I]))));
    if (ResetError > 0.01) { Error = TEXT("Native reset failed to restore authored body-local placement"); return false; }
    TSharedRef<FJsonObject> Reset = MakeShared<FJsonObject>();
    Reset->SetNumberField(TEXT("reset_error_cm"), ResetError);
    Reset->SetNumberField(TEXT("restored_pin_count"), Session->Pins.Num());
    Reset->SetArrayField(TEXT("final_positions"), JsonPositions(Current));
    if (!WriteJson(Reset, Output / (Prefix + TEXT("reset-metrics.json")), Error) ||
        !CaptureState(Scene, Component, Assets, BodyTransform, Current, true, Output / (Prefix + TEXT("reset.png")), Error)) return false;
    // Separate no-pin comparison starts from the same original placement and body.
    BodyTransform = FTransform::Identity;
    Body->SetWorldTransform(BodyTransform); Component->SetWorldTransform(BodyTransform);
    CopySetState(*Assets.Set, *Session);
    Component->SetAsset(Assets.Pinned); Component->RecreateClothSimulationProxy(); Support::TickCloth(Component, 0.f);
    if (!Support::ReadNativePositions(Component, Current, Error) || !MeasureFreeState(TEXT("no-pin"), false)) return false;
    TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
    Summary->SetBoolField(TEXT("reopened_existing_assets"), Context.bReopen);
    Summary->SetStringField(TEXT("mechanism"), TEXT("Root-bound native Chaos MaxDistance=0 zero-inverse-mass particles; cloth component rigid transform equals body transform"));
    Summary->SetStringField(TEXT("release_policy"), TEXT("Native free asset/proxy recreation resets placement and velocities; discontinuity measured, no continuity claim"));
    Summary->SetBoolField(TEXT("skeletal_pose_tracking_supported"), false);
    Summary->SetBoolField(TEXT("natural_fitting_claimed"), false);
    Summary->SetStringField(TEXT("body_mesh"), Assets.Body->GetPathName());
    Summary->SetStringField(TEXT("snapshot_git_blob"), Assets.Set->SourceBlob);
    TArray<TSharedPtr<FJsonValue>> Targets;
    for (int32 I = 0; I < Assets.Pins.Num(); ++I)
    {
        TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
        Target->SetStringField(TEXT("id"), Assets.Pins[I].Id.ToString());
        Target->SetNumberField(TEXT("panel"), Assets.Set->Pins[I].PanelIndex);
        Target->SetNumberField(TEXT("original_vertex"), Assets.Set->Pins[I].VertexIndex);
        Target->SetNumberField(TEXT("welded_vertex"), Assets.Pins[I].WeldedVertex);
        Target->SetNumberField(TEXT("body_triangle"), Assets.Set->Pins[I].BodyTriangle);
        Target->SetNumberField(TEXT("surface_offset_cm"), Assets.Set->Pins[I].SurfaceOffsetCm);
        Target->SetBoolField(TEXT("authored_enabled"), Assets.Set->Pins[I].bEnabled);
        Target->SetArrayField(TEXT("body_local_target"), JsonPositions(MakeArrayView(&Assets.Pins[I].BodyLocalTarget, 1)));
        Targets.Add(MakeShared<FJsonValueObject>(Target));
    }
    Summary->SetArrayField(TEXT("pins"), Targets);
    return WriteJson(Summary, Output / (Prefix + TEXT("summary.json")), Error);
}

bool RunExperiment(const FRunContext& Context, FString& Error)
{
    FAssets Assets;
    return Prepare(TEXT(""), nullptr, Context, Assets, Error) && RunNative(TEXT("body-"), Context, Assets, Error);
}

bool RunCombined(UChaosClothAsset* ReadOnlyInput, const FRunContext& Context, FString& Error)
{
    if (!ReadOnlyInput || static_cast<const UChaosClothAsset*>(ReadOnlyInput)->GetClothCollections().IsEmpty())
    { Error = TEXT("Missing read-only sewn input"); return false; }
    const FCollectionClothConstFacade Before(static_cast<const UChaosClothAsset*>(ReadOnlyInput)->GetClothCollections()[0]);
    const TArray<FVector3f> Source3D(Before.GetSimPosition3D());
    const TArray<FVector2f> Source2D(Before.GetSimPosition2D());
    FAssets Assets;
    if (!Prepare(TEXT("Combined_"), ReadOnlyInput, Context, Assets, Error) || !RunNative(TEXT("combined-"), Context, Assets, Error)) return false;
    const FCollectionClothConstFacade After(static_cast<const UChaosClothAsset*>(ReadOnlyInput)->GetClothCollections()[0]);
    if (Source3D != TArray<FVector3f>(After.GetSimPosition3D()) || Source2D != TArray<FVector2f>(After.GetSimPosition2D()))
    { Error = TEXT("Combined experiment modified read-only sewing source"); return false; }
    const FCollectionClothConstFacade Combined(static_cast<const UChaosClothAsset*>(Assets.Pinned)->GetClothCollections()[0]);
    const FIntVector2 Seam = Combined.GetSeam(0).GetSeamStitch2DEndIndices()[0];
    const auto Lookup = Combined.GetSimVertex3DLookup();
    if (Lookup[Seam.X] != Lookup[Seam.Y] || Assets.Pins.Num() != 1 || Assets.Pins[0].WeldedVertex != Lookup[Seam.X])
    { Error = TEXT("Combined pin does not resolve the original welded seam aliases"); return false; }
    return true;
}
}
