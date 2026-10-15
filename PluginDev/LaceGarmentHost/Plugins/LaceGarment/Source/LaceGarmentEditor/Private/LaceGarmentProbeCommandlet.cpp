#include "LaceGarmentProbeCommandlet.h"
#include "LaceGarmentPipeline.h"
#include "LaceGarmentInspection.h"
#include "Engine/SkeletalMesh.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "ClothingSystemRuntimeTypes.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/IConsoleManager.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "RenderingThread.h"

namespace
{
bool SaveProbeObject(UObject* Object)
{
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
    FString Filename=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    FAssetRegistryModule::AssetCreated(Object);
    return UPackage::SavePackage(Object->GetOutermost(),Object,*Filename,Args);
}

LaceGarment::FBuildInput MakeProbe()
{
    using namespace LaceGarment;
    FBuildInput Input;
    FPanel Front;
    Front.Id=FGuid(1,0,0,1);
    Front.BoundaryMm={{0,0},{40,0},{40,30},{20,32},{0,30}};
    Front.MaxEdgeMm=5;
    Front.InitialPlacementCm=FTransform(FRotator(0,90,-90),FVector(1.2,-2,7));
    FPanel Back=Front;
    Back.Id=FGuid(1,0,0,2);
    Back.InitialPlacementCm=FTransform(FRotator(0,-90,-90),FVector(-1.2,2,7));
    Input.Panels={Front,Back};
    Input.Seams={{0,1,1,4,true},{0,4,1,1,true}};
    Input.Pins={{0,3},{1,3}};
    Input.Colliders.Add({FVector3d(0,0,6.5),FVector3d(0,0,10.5),1.0});
    return Input;
}

TArray<TSharedPtr<FJsonValue>> Vectors(const TArray<FVector3d>& Points)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for(auto P : Points) Values.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}));
    return Values;
}

template<typename T> bool Same(TConstArrayView<T> A,TConstArrayView<T> B)
{
    if(A.Num()!=B.Num()) return false;
    for(int32 I=0; I<A.Num(); ++I) if(A[I]!=B[I]) return false;
    return true;
}
}

ULaceGarmentProbeCommandlet::ULaceGarmentProbeCommandlet()
{
    IsClient=true; IsServer=false; IsEditor=true; LogToConsole=true;
}

int32 ULaceGarmentProbeCommandlet::Main(const FString& Params)
{
    using namespace LaceGarment;
    using namespace UE::Chaos::ClothAsset;
    FString ReportDir;
    if(!FParse::Value(*Params,TEXT("ReportDir="),ReportDir)) return 1;
    IFileManager::Get().MakeDirectory(*ReportDir,true);
    const bool bReopen=FParse::Param(*Params,TEXT("VerifyExisting"));
    const bool bBodySkin=FParse::Param(*Params,TEXT("BodySkin"));
    const FString AssetPackage=bBodySkin?TEXT("/LaceGarment/Development/CA_BodySkinProbe"):TEXT("/LaceGarment/Development/CA_NativePanelProbe");
    USkeletalMesh* ReferenceBody=bBodySkin?LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11")):nullptr;
    if(bBodySkin && !ReferenceBody) return 11;
    TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("scope"),TEXT("Two-panel native drape probe; not completed garment authoring or gate A"));
    Report->SetBoolField(TEXT("reopenedInNewProcess"),bReopen);
    Report->SetStringField(TEXT("units"),TEXT("cm"));
    Report->SetStringField(TEXT("generatorVersion"),GeneratorVersion);
    Report->SetStringField(TEXT("asset"),AssetPackage);
    Report->SetStringField(TEXT("skinTransfer"),bBodySkin?TEXT("surfaceVertex"):TEXT("rootOnlyProbe"));
    FString Error;
    UChaosClothAsset* Asset=nullptr;
    if(!bReopen)
    {
        FBuildResult Result;
        if(!BuildGarment(MakeProbe(),Result,Error)) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 2; }
        if(bBodySkin && !TransferBodySkin(Result,ReferenceBody,Error,EBodySkinTransfer::SurfaceVertex)) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 12; }
        Report->SetNumberField(TEXT("steps"),Result.Steps);
        Report->SetNumberField(TEXT("maxSeamGapCm"),Result.MaxSeamGapCm);
        Report->SetNumberField(TEXT("maxPenetrationCm"),Result.MaxPenetrationCm);
        Report->SetNumberField(TEXT("maxRelativeEdgeError"),Result.MaxRelativeEdgeError);
        Report->SetNumberField(TEXT("maxFacePenetrationCm"),Result.MaxFacePenetrationCm);
        Report->SetNumberField(TEXT("maxInternalFoldDegrees"),Result.MaxInternalFoldDegrees);
        Report->SetNumberField(TEXT("minAreaRatio"),Result.MinAreaRatio);
        Report->SetNumberField(TEXT("maxAreaRatio"),Result.MaxAreaRatio);
        Report->SetNumberField(TEXT("maxSpeedCmPerSec"),Result.MaxSpeedCmPerSec);
        Report->SetNumberField(TEXT("texelsPerCm"),Result.TexelsPerCm);
        Report->SetArrayField(TEXT("initialCm"),Vectors(Result.InitialCm));
        Report->SetArrayField(TEXT("settledCm"),Vectors(Result.SettledCm));
        TArray<TSharedPtr<FJsonValue>> Rest, Faces, UV;
        for(int32 I=0; I<Result.Panels.Num(); ++I)
        {
            for(auto P : Result.Panels[I].RestMm) Rest.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)}));
            for(auto T : Result.Panels[I].Triangles) Faces.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(T.X+Result.Offsets[I]),MakeShared<FJsonValueNumber>(T.Y+Result.Offsets[I]),MakeShared<FJsonValueNumber>(T.Z+Result.Offsets[I])}));
        }
        for(auto P : Result.UV) UV.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)}));
        Report->SetArrayField(TEXT("restMm"),Rest); Report->SetArrayField(TEXT("triangles"),Faces); Report->SetArrayField(TEXT("uv0"),UV);
        FString AuthoringJson; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&AuthoringJson));
        if(!FFileHelper::SaveStringToFile(AuthoringJson,*(ReportDir/TEXT("authoring.json")))) return 3;

        UPackage* MatPackage=CreatePackage(TEXT("/LaceGarment/Development/M_NativeCloth"));
        MatPackage->FullyLoad();
        UMaterial* Material=FindObject<UMaterial>(MatPackage,TEXT("M_NativeCloth"));
        if(!Material)
        {
            Material=NewObject<UMaterial>(MatPackage,TEXT("M_NativeCloth"),RF_Public|RF_Standalone);
            auto* Color=CastChecked<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionVectorParameter::StaticClass()));
            Color->ParameterName=TEXT("Color"); Color->DefaultValue=FLinearColor(0.08f,0.35f,0.65f);
            UMaterialEditingLibrary::ConnectMaterialProperty(Color,TEXT(""),MP_BaseColor);
        }
        Material->TwoSided=true;
        Material->SetMaterialUsage(MATUSAGE_Clothing); Material->SetMaterialUsage(MATUSAGE_SkeletalMesh);
        UMaterialEditingLibrary::RecompileMaterial(Material);
        FAssetCompilingManager::Get().FinishAllCompilation();
        if(!SaveProbeObject(Material)) return 3;
        Asset=SaveClothAsset(Result,Material,AssetPackage,Error);
        if(!Asset) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 4; }

        UPackage* PhysicsPackage=CreatePackage(TEXT("/LaceGarment/Development/PA_NativeProbe"));
        PhysicsPackage->FullyLoad();
        UPhysicsAsset* Physics=FindObject<UPhysicsAsset>(PhysicsPackage,TEXT("PA_NativeProbe"));
        if(!Physics) Physics=NewObject<UPhysicsAsset>(PhysicsPackage,TEXT("PA_NativeProbe"),RF_Public|RF_Standalone);
        Physics->SkeletalBodySetups.Reset();
        USkeletalBodySetup* Body=NewObject<USkeletalBodySetup>(Physics);
        Body->BoneName=Asset->GetRefSkeleton().GetBoneName(0);
        FKSphylElem Capsule; Capsule.Center=FVector(0,0,8.5); Capsule.Radius=1; Capsule.Length=4;
        Body->AggGeom.SphylElems.Add(Capsule);
        Physics->SkeletalBodySetups.Add(Body); Physics->UpdateBodySetupIndexMap(); Physics->UpdateBoundsBodiesArray();
        if(!SaveProbeObject(Physics)) return 5;
        // Persist explicit runtime configuration and collision in the cloth collection itself.
        const UChaosClothAsset* BuiltAsset=Asset;
        TSharedRef<FManagedArrayCollection> Collection=MakeShared<FManagedArrayCollection>(*BuiltAsset->GetClothCollections()[0]);
        FCollectionClothFacade Cloth(Collection);
        Cloth.SetPhysicsAssetSoftObjectPathName(FSoftObjectPath(Physics));
        FText BuildError;
        Asset->Build({Collection},nullptr,&BuildError);
        FAssetCompilingManager::Get().FinishAllCompilation();
        if(!BuildError.IsEmpty() || !SaveProbeObject(Asset)) return 6;
    }
    else Asset=LoadObject<UChaosClothAsset>(nullptr,*(AssetPackage+TEXT(".")+FPackageName::GetLongPackageAssetName(AssetPackage)));

    if(!Asset || !Asset->HasValidClothSimulationModels() || !Asset->GetPhysicsAsset()) return 7;
    const UChaosClothAsset* ReadAsset=Asset;
    FCollectionClothConstFacade Cloth(ReadAsset->GetClothCollections()[0]);
    Report->SetNumberField(TEXT("simVertices"),Cloth.GetNumSimVertices3D());
    Report->SetNumberField(TEXT("renderVertices"),Cloth.GetNumRenderVertices());
    Report->SetNumberField(TEXT("simTriangles"),Cloth.GetNumSimFaces());
    Report->SetNumberField(TEXT("seams"),Cloth.GetNumSeams());
    Report->SetNumberField(TEXT("skeletonBones"),Asset->GetRefSkeleton().GetNum());
    bool bValid=Cloth.GetNumSimPatterns()==2 && Cloth.GetNumSeams()==1 && Cloth.GetNumRenderVertices()>Cloth.GetNumSimVertices3D();
    bValid &= Asset->GetRefSkeleton().GetNum()==(bBodySkin?ReferenceBody->GetRefSkeleton().GetNum():1);
    for(const auto& Material : Asset->GetMaterials())
        bValid &= Material.MaterialInterface && Material.MaterialInterface->GetPathName().StartsWith(TEXT("/LaceGarment/"));

    // Reconstruct from the independent source fixture, never from the saved output.
    FBuildResult ExpectedBuild;
    if(!BuildGarment(MakeProbe(),ExpectedBuild,Error)) return 10;
    if(bBodySkin && !TransferBodySkin(ExpectedBuild,ReferenceBody,Error,EBodySkinTransfer::SurfaceVertex)) return 12;
    UMaterial* ExpectedMaterial=LoadObject<UMaterial>(nullptr,TEXT("/LaceGarment/Development/M_NativeCloth.M_NativeCloth"));
    auto ExpectedCollection=MakeClothCollection(ExpectedBuild,ExpectedMaterial,Error);
    if(!ExpectedCollection) return 10;
    FCollectionClothConstFacade Expected(ExpectedCollection.ToSharedRef());
    bool bSourceMatches=Same(Cloth.GetSimPosition2D(),Expected.GetSimPosition2D())
        && Same(Cloth.GetSimPosition3D(),Expected.GetSimPosition3D())
        && Same(Cloth.GetSimIndices2D(),Expected.GetSimIndices2D())
        && Same(Cloth.GetSimIndices3D(),Expected.GetSimIndices3D())
        && Same(Cloth.GetSimVertex3DLookup(),Expected.GetSimVertex3DLookup())
        && Same(Cloth.GetRenderPosition(),Expected.GetRenderPosition())
        && Same(Cloth.GetRenderIndices(),Expected.GetRenderIndices())
        && Same(Cloth.GetRenderUVs(),Expected.GetRenderUVs())
        && Same(Cloth.GetSimBoneIndices(),Expected.GetSimBoneIndices())
        && Same(Cloth.GetSimBoneWeights(),Expected.GetSimBoneWeights())
        && Same(Cloth.GetRenderBoneIndices(),Expected.GetRenderBoneIndices())
        && Same(Cloth.GetRenderBoneWeights(),Expected.GetRenderBoneWeights())
        && Same(Cloth.GetWeightMap(TEXT("MaxDistance")),Expected.GetWeightMap(TEXT("MaxDistance")))
        && Cloth.GetSkeletalMeshSoftObjectPathName()==Expected.GetSkeletalMeshSoftObjectPathName()
        && Cloth.GetNumSeams()==Expected.GetNumSeams();
    if(bSourceMatches)
        for(int32 I=0; I<Cloth.GetNumSeams(); ++I)
            bSourceMatches &= Same(Cloth.GetSeam(I).GetSeamStitch2DEndIndices(),Expected.GetSeam(I).GetSeamStitch2DEndIndices());
    Report->SetBoolField(TEXT("savedGeometryMatchesSource"),bSourceMatches);
    bValid &= bSourceMatches;

    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("LaceGarmentProbeWorld")),nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("p.ChaosClothAsset.WaitForAsyncClothInitialization"))) CVar->Set(1);
    AActor* Actor=World->SpawnActor<AActor>();
    UChaosClothComponent* Component=NewObject<UChaosClothComponent>(Actor);
    Actor->SetRootComponent(Component); Actor->AddInstanceComponent(Component);
    Component->SetAsset(Asset);
    Component->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->RegisterComponent();
    TMap<int32,FClothSimulData> Simulation;
    double PeakDisplacement=0,PeakPenetration=0,PeakAnchorDisplacement=0;
    int32 FramesWithParticles=0;
    const auto Anchors=Cloth.GetWeightMap(TEXT("MaxDistance"));
    for(int32 Frame=0; Frame<240; ++Frame)
    {
        static_cast<UActorComponent*>(Component)->TickComponent(1.f/60,LEVELTICK_All,&Component->PrimaryComponentTick);
        Component->WaitForExistingParallelClothSimulation_GameThread();
        if(const auto* Proxy=Component->GetClothSimulationProxy())
        {
            const auto& FrameData=Proxy->GetCurrentSimulationData_AnyThread();
            int32 FrameParticles=0;
            for(const auto& Pair : FrameData)
                for(int32 I=0; I<Pair.Value.Positions.Num(); ++I)
                {
                    ++FrameParticles;
                    const FVector3d P=Pair.Value.ComponentRelativeTransform.TransformPosition(FVector3d(Pair.Value.Positions[I]));
                    if(P.ContainsNaN() || I>=Cloth.GetNumSimVertices3D()) { bValid=false; continue; }
                    const double Displacement=(P-FVector3d(Cloth.GetSimPosition3D()[I])).Length();
                    PeakDisplacement=FMath::Max(PeakDisplacement,Displacement);
                    if(Anchors.IsValidIndex(I) && Anchors[I]==0) PeakAnchorDisplacement=FMath::Max(PeakAnchorDisplacement,Displacement);
                    const FVector3d Q(0,0,FMath::Clamp(P.Z,6.5,10.5));
                    PeakPenetration=FMath::Max(PeakPenetration,1.0-(P-Q).Length());
                }
            if(FrameParticles==Cloth.GetNumSimVertices3D()) ++FramesWithParticles;
            else if(Frame>0) bValid=false;
        }
    }
    if(const auto* Proxy=Component->GetClothSimulationProxy()) Simulation=Proxy->GetCurrentSimulationData_AnyThread();
    int32 RuntimePoints=0; double Movement=0,Penetration=0;
    TArray<FVector3d> RuntimePositions;
    for(const auto& Pair : Simulation)
    {
        RuntimePoints+=Pair.Value.Positions.Num();
        for(int32 I=0; I<Pair.Value.Positions.Num(); ++I)
        {
            FVector3d P=Pair.Value.ComponentRelativeTransform.TransformPosition(FVector3d(Pair.Value.Positions[I])); RuntimePositions.Add(P);
            if(P.ContainsNaN()) bValid=false;
            if(I<Cloth.GetSimPosition3D().Num()) Movement=FMath::Max(Movement,(P-FVector3d(Cloth.GetSimPosition3D()[I])).Length());
            FVector3d Q(0,0,FMath::Clamp(P.Z,6.5,10.5));
            Penetration=FMath::Max(Penetration,1.0-(P-Q).Length());
        }
    }
    Report->SetArrayField(TEXT("runtimeCm"),Vectors(RuntimePositions));
    Report->SetNumberField(TEXT("runtimeParticles"),RuntimePoints);
    Report->SetNumberField(TEXT("runtimeMaxDisplacementCm"),Movement);
    Report->SetNumberField(TEXT("runtimeBodyPenetrationCm"),Penetration);
    Report->SetNumberField(TEXT("runtimeFramesWithParticles"),FramesWithParticles);
    Report->SetNumberField(TEXT("runtimePeakDisplacementCm"),PeakDisplacement);
    Report->SetNumberField(TEXT("runtimePeakVertexPenetrationCm"),PeakPenetration);
    Report->SetNumberField(TEXT("runtimePeakAnchorDisplacementCm"),PeakAnchorDisplacement);
    bValid &= FramesWithParticles>=239 && PeakDisplacement<5 && PeakPenetration<0.05 && PeakAnchorDisplacement<0.001;
    bValid &= RuntimePoints==Cloth.GetNumSimVertices3D() && Movement>1.e-5 && Movement<5 && Penetration<0.05;
    if(FParse::Param(*Params,TEXT("Render")))
    {
        bValid &= CaptureInspectionViews(World,ReportDir,FVector(0,0,8.5),6,{FVector(14,-16,13),FVector(16,0,9),FVector(0,16,9)});
    }
    Report->SetBoolField(TEXT("passed"),bValid);
    Component->UnregisterComponent();
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
    if(!FFileHelper::SaveStringToFile(Json,*(ReportDir/TEXT("metrics.json")))) return 8;
    UE_LOG(LogTemp,Display,TEXT("LaceGarment probe %s: runtime particles=%d, movement=%gcm, body penetration=%gcm"),bValid?TEXT("PASS"):TEXT("FAIL"),RuntimePoints,Movement,Penetration);
    return bValid?0:9;
}
