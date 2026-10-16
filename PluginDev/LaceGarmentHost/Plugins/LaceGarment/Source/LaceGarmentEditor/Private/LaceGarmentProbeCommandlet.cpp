#include "LaceGarmentProbeCommandlet.h"
#include "LaceGarmentPipeline.h"
#include "LaceGarmentInspection.h"
#include "LaceGarmentTemplates.h"
#include "LaceGarmentCorrections.h"
#include "LaceGarmentCollisionProjection.h"
#include "JsonObjectConverter.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "LaceGarmentSurface.h"
#include "Engine/SkeletalMesh.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "Chaos/CollectionPropertyFacade.h"
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
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/SkinnedTriangleMeshElem.h"

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

UPhysicsAsset* MakeSkinnedBodyDiagnostic(USkeletalMesh* Body,FString& Error)
{
    if(!Body) { Error=TEXT("Missing diagnostic body"); return nullptr; }
    FAssetCompilingManager::Get().FinishAllCompilation();
    auto* Physics=NewObject<UPhysicsAsset>(GetTransientPackage());
    FPhysAssetCreateParams Parameters;
    FText BuildError;
    const int32 BodyIndex=FPhysicsAssetUtils::GenerateSkinnedTriangleMesh(Physics,Body,0,Parameters,BuildError,false);
    if(BodyIndex==INDEX_NONE || !Physics->SkeletalBodySetups.IsValidIndex(BodyIndex)
        || Physics->SkeletalBodySetups[BodyIndex]->AggGeom.SkinnedTriangleMeshElems.Num()!=1)
    { Error=BuildError.IsEmpty()?TEXT("Native skinned body generation failed"):BuildError.ToString(); return nullptr; }
    Physics->UpdateBodySetupIndexMap(); Physics->UpdateBoundsBodiesArray();
    return Physics;
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
    const bool bRuntimeOnly=FParse::Param(*Params,TEXT("RuntimeOnly"));
    const bool bGenerateTethers=FParse::Param(*Params,TEXT("GenerateTethers"));
    const bool bWaistband=FParse::Param(*Params,TEXT("RuntimeWaistband"));
    const bool bSkinnedBody=FParse::Param(*Params,TEXT("RuntimeSkinnedBody"));
    if(bSkinnedBody && (!bRuntimeOnly || !FParse::Param(*Params,TEXT("Poses")))) return 1;
    int32 RuntimeIterations=0;
    const bool bRuntimeIterations=FParse::Value(*Params,TEXT("RuntimeIterations="),RuntimeIterations);
    if(bRuntimeIterations && (!bRuntimeOnly || RuntimeIterations<1 || RuntimeIterations>400)) return 1;
    float RuntimeGravityScale=1;
    const bool bRuntimeGravity=FParse::Value(*Params,TEXT("RuntimeGravityScale="),RuntimeGravityScale);
    if(bRuntimeGravity && (!bRuntimeOnly || !FMath::IsFinite(RuntimeGravityScale) || RuntimeGravityScale<0 || RuntimeGravityScale>2)) return 1;
    float RuntimeFriction=0.4f;
    const bool bRuntimeFriction=FParse::Value(*Params,TEXT("RuntimeFriction="),RuntimeFriction);
    if(bRuntimeFriction && (!bRuntimeOnly || !FMath::IsFinite(RuntimeFriction) || RuntimeFriction<0 || RuntimeFriction>1)) return 1;
    float RuntimeBendingStrength=1;
    const bool bRuntimeBending=FParse::Value(*Params,TEXT("RuntimeBendingStrength="),RuntimeBendingStrength);
    const bool bHingeBending=FParse::Param(*Params,TEXT("HingeBending"));
    if((bRuntimeBending && (!bRuntimeOnly || !FMath::IsFinite(RuntimeBendingStrength) || RuntimeBendingStrength<0 || RuntimeBendingStrength>1)) || (bHingeBending && !bRuntimeOnly)) return 1;
    const bool bReopen=bRuntimeOnly || FParse::Param(*Params,TEXT("VerifyExisting"));
    FString Template,SourceDraft;
    const bool bTemplate=FParse::Value(*Params,TEXT("Template="),Template);
    if(bTemplate && Template!=TEXT("TShirt") && Template!=TEXT("DolphinShorts")) return 1;
    if(bWaistband && (!bRuntimeOnly || Template!=TEXT("DolphinShorts"))) return 1;
    if((bRuntimeOnly && (!bTemplate || FParse::Value(*Params,TEXT("SourceDraft="),SourceDraft))) || (bGenerateTethers && !bRuntimeOnly)) return 1;
    const bool bBodySkin=bTemplate || FParse::Param(*Params,TEXT("BodySkin"));
    const bool bPoses=FParse::Param(*Params,TEXT("Poses"));
    if(bPoses && !bBodySkin) return 1;
    const FString AssetPackage=bTemplate?TEXT("/LaceGarment/Development/CA_")+Template:
        bBodySkin?TEXT("/LaceGarment/Development/CA_BodySkinProbe"):TEXT("/LaceGarment/Development/CA_NativePanelProbe");
    USkeletalMesh* ReferenceBody=bBodySkin?LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11")):nullptr;
    if(bBodySkin && !ReferenceBody) return 11;
    if(bPoses) for(const FName Bone : {FName(TEXT("upperarm_l")),FName(TEXT("upperarm_r")),FName(TEXT("forearm_l")),FName(TEXT("forearm_r")),FName(TEXT("thigh_l")),FName(TEXT("calf_l"))})
        if(ReferenceBody->GetRefSkeleton().FindBoneIndex(Bone)==INDEX_NONE) return 14;
    FString Error;
    FBuildInput Input=MakeProbe();
    TStrongObjectPtr<ULaceGarmentDocument> Document;
    if(bTemplate && !bRuntimeOnly)
    {
        Document.Reset(NewObject<ULaceGarmentDocument>());
        if(FParse::Value(*Params,TEXT("SourceDraft="),SourceDraft))
        {
            FString Json;
            if(!FFileHelper::LoadFileToString(Json,*SourceDraft)
                || !FJsonObjectConverter::JsonObjectStringToUStruct(Json,Document.Get(),0,CPF_Transient)
                || !Document->ValidateStructure(Error))
            { UE_LOG(LogTemp,Error,TEXT("Invalid diagnostic source draft: %s"),*Error); return 13; }
        }
        else if(!ApplyTemplate(*Document,Template==TEXT("TShirt")?ETemplate::TShirt:ETemplate::DolphinShorts,{},Error))
        { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 13; }
        FSampledDocument Sampled;
        if(!SampleDocument(*Document,Sampled,Error)
            || !ReadBodyCollision(ReferenceBody,Sampled.Input.Colliders,Sampled.Input.ConvexColliders,Error))
        { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 13; }
        Input=MoveTemp(Sampled.Input);
        FParse::Value(*Params,TEXT("MaxSteps="),Input.MaxSteps);
        Input.Progress=[](const FBuildResult& B)
        { UE_LOG(LogTemp,Display,TEXT("Probe source fit: %d steps, speed %.5fcm/s, strain %.5f, seam %.5fcm"),B.Steps,B.MaxSpeedCmPerSec,B.MaxRelativeEdgeError,B.MaxSeamGapCm); };
        FString Json; FJsonObjectConverter::UStructToJsonObjectString(ULaceGarmentDocument::StaticClass(),Document.Get(),Json,0,CPF_Transient);
        if(!FFileHelper::SaveStringToFile(Json,*(ReportDir/TEXT("source-draft.json")))) return 3;
    }
    else if(bRuntimeOnly && !ReadBodyCollision(ReferenceBody,Input.Colliders,Input.ConvexColliders,Error)) return 13;
    auto Generate=[&](FBuildResult& Result)
    {
        if(!BuildGarment(Input,Result,Error)) return false;
        if(bBodySkin && !TransferBodySkin(Result,ReferenceBody,Error,EBodySkinTransfer::SurfaceVertex)) return false;
        return !bTemplate || ApplyDocumentCorrections(*Document,Result,Error);
    };
    auto BodyPenetration=[&](FVector3d P)
    {
        double Penetration=0;
        for(const auto& C : Input.Colliders)
        {
            const auto Axis=C.B-C.A;
            const auto Q=C.A+Axis*FMath::Clamp(FVector3d::DotProduct(P-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
            Penetration=FMath::Max(Penetration,C.RadiusCm-(P-Q).Length());
        }
        for(const auto& C : Input.ConvexColliders)
        {
            if(!C.GetBounds().IsInsideOrOn(P)) continue;
            FVector3d Normal; Penetration=FMath::Max(Penetration,-C.SignedDistance(P,Normal));
        }
        return Penetration;
    };
    TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("scope"),bTemplate?TEXT("Representative garment save/reopen and reference-pose runtime; articulated pose acceptance is separate"):
        TEXT("Two-panel native drape probe; not completed garment authoring or gate A"));
    if(bTemplate && bPoses) Report->SetStringField(TEXT("scope"),TEXT("Representative garment save/reopen, reference pose, arm lift, knee bend and return-to-reference inspection"));
    if(bRuntimeOnly) Report->SetStringField(TEXT("scope"),TEXT("Read-only saved Cloth runtime diagnostic; source regeneration and identity are NOT checked; not garment acceptance"));
    Report->SetBoolField(TEXT("diagnosticRuntimeOnly"),bRuntimeOnly);
    Report->SetBoolField(TEXT("diagnosticRegenerateTethers"),bGenerateTethers);
    Report->SetBoolField(TEXT("sourceIdentityChecked"),!bRuntimeOnly);
    Report->SetBoolField(TEXT("diagnosticSkinnedBody"),bSkinnedBody);
    if(bSkinnedBody)
    {
        Report->SetStringField(TEXT("scope"),TEXT("Transient native skinned-body collision comparison. Penetration metrics still use original rigid proxies; direct mesh surface acceptance is NOT evaluated; never garment acceptance."));
        Report->SetStringField(TEXT("collisionMeasurementScope"),TEXT("originalRigidProxiesOnly"));
    }
    Report->SetBoolField(TEXT("articulatedPoses"),bPoses);
    Report->SetBoolField(TEXT("reopenedInNewProcess"),bReopen);
    Report->SetStringField(TEXT("units"),TEXT("cm"));
    Report->SetStringField(TEXT("generatorVersion"),GeneratorVersion);
    Report->SetStringField(TEXT("asset"),AssetPackage);
    Report->SetStringField(TEXT("skinTransfer"),bBodySkin?TEXT("surfaceVertex"):TEXT("rootOnlyProbe"));
    Report->SetStringField(TEXT("collisionScope"),bSkinnedBody?TEXT("transientNativeSkinnedReferenceBody"):bTemplate?TEXT("fullReferenceBodyPhysicsAsset"):TEXT("singleRootCapsuleProbe"));
    Report->SetStringField(TEXT("surfaceCheckFrequency"),TEXT("endOfEachPoseStage; vertex and anchor checks every frame"));
    UChaosClothAsset* Asset=nullptr;
    FBuildResult ExpectedBuild;
    if(!bReopen)
    {
        auto& Result=ExpectedBuild;
        if(!Generate(Result)) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 2; }
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

        UPhysicsAsset* Physics=bTemplate?ReferenceBody->GetPhysicsAsset():nullptr;
        if(!bTemplate)
        {
        UPackage* PhysicsPackage=CreatePackage(TEXT("/LaceGarment/Development/PA_NativeProbe"));
        PhysicsPackage->FullyLoad();
        Physics=FindObject<UPhysicsAsset>(PhysicsPackage,TEXT("PA_NativeProbe"));
        if(!Physics) Physics=NewObject<UPhysicsAsset>(PhysicsPackage,TEXT("PA_NativeProbe"),RF_Public|RF_Standalone);
        Physics->SkeletalBodySetups.Reset();
        USkeletalBodySetup* Body=NewObject<USkeletalBodySetup>(Physics);
        Body->BoneName=Asset->GetRefSkeleton().GetBoneName(0);
        FKSphylElem Capsule; Capsule.Center=FVector(0,0,8.5); Capsule.Radius=1; Capsule.Length=4;
        Body->AggGeom.SphylElems.Add(Capsule);
        Physics->SkeletalBodySetups.Add(Body); Physics->UpdateBodySetupIndexMap(); Physics->UpdateBoundsBodiesArray();
        if(!SaveProbeObject(Physics)) return 5;
        }
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
    int32 DiagnosticWaistVertices=0;
    TStrongObjectPtr<UPhysicsAsset> DiagnosticPhysics;
    if(bGenerateTethers || bWaistband || bSkinnedBody || bRuntimeIterations || bRuntimeGravity || bRuntimeFriction || bRuntimeBending || bHingeBending)
    {
        // Isolate this one runtime variable without changing the saved fixture.
        Asset=DuplicateObject<UChaosClothAsset>(Asset,GetTransientPackage());
        const UChaosClothAsset* Original=Asset;
        auto Collection=MakeShared<FManagedArrayCollection>(*Original->GetClothCollections()[0]);
        if(bSkinnedBody)
        {
            DiagnosticPhysics.Reset(MakeSkinnedBodyDiagnostic(ReferenceBody,Error));
            if(!DiagnosticPhysics.IsValid()) { UE_LOG(LogTemp,Error,TEXT("Skinned body diagnostic: %s"),*Error); return 6; }
            FCollectionClothFacade MutableCloth(Collection);
            MutableCloth.SetPhysicsAssetSoftObjectPathName(FSoftObjectPath(DiagnosticPhysics.Get()));
            const auto& Mesh=DiagnosticPhysics->SkeletalBodySetups[0]->AggGeom.SkinnedTriangleMeshElems[0].GetSkinnedTriangleMesh();
            Report->SetNumberField(TEXT("diagnosticBodyVertices"),Mesh->GetReferencePositions().Num());
            Report->SetNumberField(TEXT("diagnosticBodyTriangles"),Mesh->GetTriangleMesh().GetElements().Num());
        }
        if(bWaistband)
        {
            // Test an authored waistband attachment instead of concentrating
            // the garment's entire load at four isolated fitting guides.
            FCollectionClothFacade MutableCloth(Collection);
            const int32 Pelvis=Asset->GetRefSkeleton().FindBoneIndex(TEXT("pelvis"));
            auto Distances=MutableCloth.GetWeightMap(TEXT("MaxDistance"));
            if(Pelvis==INDEX_NONE || Distances.Num()!=MutableCloth.GetNumSimVertices3D()) return 6;
            TSet<int32> Waist;
            const auto Lookup=MutableCloth.GetSimVertex3DLookup();
            for(int32 I=0;I<MutableCloth.GetNumSimPatterns();++I)
            {
                auto Pattern=MutableCloth.GetSimPattern(I); const auto Rest=Pattern.GetSimPosition2D();
                float Top=-FLT_MAX; for(const auto P : Rest) Top=FMath::Max(Top,P.Y);
                for(int32 V=0;V<Rest.Num();++V) if(FMath::Abs(Rest[V].Y-Top)<1.e-5f) Waist.Add(Lookup[Pattern.GetSimVertices2DOffset()+V]);
            }
            for(int32 V : Waist)
            {
                Distances[V]=0; MutableCloth.GetSimBoneIndices()[V]={Pelvis}; MutableCloth.GetSimBoneWeights()[V]={1.f};
                for(int32 R=0;R<MutableCloth.GetNumRenderVertices();++R)
                    if(MutableCloth.GetRenderPosition()[R].Equals(MutableCloth.GetSimPosition3D()[V],1.e-6f))
                    { MutableCloth.GetRenderBoneIndices()[R]={Pelvis}; MutableCloth.GetRenderBoneWeights()[R]={1.f}; }
            }
            DiagnosticWaistVertices=Waist.Num();
            if(DiagnosticWaistVertices<8 || DiagnosticWaistVertices>=Distances.Num()/2) return 6;
        }
        if(bGenerateTethers || bWaistband) FClothEngineTools::GenerateTethers(Collection,TEXT("MaxDistance"),true);
        if(bRuntimeIterations)
        {
            Chaos::Softs::FCollectionPropertyFacade Properties(Collection);
            if(Properties.SetValue(Chaos::FClothingSimulationSolver::NumIterationsName,RuntimeIterations)==INDEX_NONE
                || Properties.SetValue(Chaos::FClothingSimulationSolver::MaxNumIterationsName,RuntimeIterations)==INDEX_NONE) return 6;
        }
        if(bRuntimeGravity)
        {
            Chaos::Softs::FCollectionPropertyFacade Properties(Collection);
            if(Properties.SetValue(FName(TEXT("GravityScale")),RuntimeGravityScale)==INDEX_NONE) return 6;
        }
        if(bRuntimeFriction)
        {
            Chaos::Softs::FCollectionPropertyFacade Properties(Collection);
            if(Properties.SetValue(FName(TEXT("FrictionCoefficient")),RuntimeFriction)==INDEX_NONE) return 6;
        }
        if(bRuntimeBending || bHingeBending)
        {
            Chaos::Softs::FCollectionPropertyMutableFacade Properties(Collection);
            const float Strength=bRuntimeBending?RuntimeBendingStrength:Properties.GetValue<float>(FName(TEXT("BendingSpringStiffness")));
            if(bHingeBending)
            {
                if(Properties.SetEnabled(FName(TEXT("BendingSpringStiffness")),false)==INDEX_NONE) return 6;
                Properties.AddValue(FName(TEXT("BendingElementStiffness")),Strength);
                Properties.AddValue(FName(TEXT("BucklingRatio")),0.f);
                Properties.AddValue(FName(TEXT("BucklingStiffness")),Strength);
                Properties.AddValue(FName(TEXT("RestAngleType")),0); // Native Use3DRestAngles.
            }
            else if(Properties.SetValue(FName(TEXT("BendingSpringStiffness")),Strength)==INDEX_NONE) return 6;
        }
        FText BuildError; Asset->Build({Collection},nullptr,&BuildError);
        FAssetCompilingManager::Get().FinishAllCompilation();
        if(!BuildError.IsEmpty()) return 6;
    }
    const UChaosClothAsset* ReadAsset=Asset;
    FCollectionClothConstFacade Cloth(ReadAsset->GetClothCollections()[0]);
    Report->SetBoolField(TEXT("diagnosticWaistband"),bWaistband);
    Report->SetNumberField(TEXT("diagnosticWaistbandVertices"),DiagnosticWaistVertices);
    Chaos::Softs::FCollectionPropertyConstFacade RuntimeProperties(ReadAsset->GetClothCollections()[0]);
    Report->SetNumberField(TEXT("runtimeConfiguredIterations"),RuntimeProperties.GetValue<int32>(Chaos::FClothingSimulationSolver::NumIterationsName));
    Report->SetNumberField(TEXT("runtimeConfiguredMaxIterations"),RuntimeProperties.GetValue<int32>(Chaos::FClothingSimulationSolver::MaxNumIterationsName));
    Report->SetNumberField(TEXT("runtimeConfiguredSubsteps"),RuntimeProperties.GetValue<int32>(Chaos::FClothingSimulationSolver::NumSubstepsName));
    Report->SetNumberField(TEXT("runtimeConfiguredGravityScale"),RuntimeProperties.GetValue<float>(FName(TEXT("GravityScale"))));
    Report->SetNumberField(TEXT("runtimeConfiguredFriction"),RuntimeProperties.GetValue<float>(FName(TEXT("FrictionCoefficient"))));
    Report->SetStringField(TEXT("runtimeBendingModel"),bHingeBending?TEXT("hingeAngleDiagnostic"):TEXT("crossEdgeSpring"));
    Report->SetNumberField(TEXT("runtimeConfiguredBendingStrength"),RuntimeProperties.GetValue<float>(FName(bHingeBending?TEXT("BendingElementStiffness"):TEXT("BendingSpringStiffness"))));
    Report->SetNumberField(TEXT("simVertices"),Cloth.GetNumSimVertices3D());
    Report->SetNumberField(TEXT("renderVertices"),Cloth.GetNumRenderVertices());
    Report->SetNumberField(TEXT("simTriangles"),Cloth.GetNumSimFaces());
    Report->SetNumberField(TEXT("seams"),Cloth.GetNumSeams());
    Report->SetNumberField(TEXT("skeletonBones"),Asset->GetRefSkeleton().GetNum());
    int32 TetherCount=0;
    for(const auto& Ends : Cloth.GetTetherKinematicIndex()) TetherCount+=Ends.Num();
    Report->SetNumberField(TEXT("nativeTethers"),TetherCount);
    bool bValid=Cloth.GetNumSimPatterns()==2 && Cloth.GetNumSeams()==1 && Cloth.GetNumRenderVertices()>Cloth.GetNumSimVertices3D();
    bValid &= Asset->GetRefSkeleton().GetNum()==(bBodySkin?ReferenceBody->GetRefSkeleton().GetNum():1);
    for(const auto& Material : Asset->GetMaterials())
        bValid &= Material.MaterialInterface && Material.MaterialInterface->GetPathName().StartsWith(TEXT("/LaceGarment/"));

    // A save compares against the independent input build already produced above.
    // A new process regenerates that source; neither path trusts saved geometry
    // as its expected result. Avoid solving a garment twice in the save process.
    if(bRuntimeOnly)
    {
        // Inspect the saved 2D rest lengths against simulated positions. This
        // deliberately makes no claim about correspondence to source documents.
        const auto Lookup=Cloth.GetSimVertex3DLookup();
        for(int32 I=0;I<Cloth.GetNumSimPatterns();++I)
        {
            const auto Pattern=Cloth.GetSimPattern(I);
            FPanelMesh Panel;
            const int32 Offset=Pattern.GetSimVertices2DOffset();
            ExpectedBuild.Offsets.Add(Offset);
            for(auto P : Pattern.GetSimPosition2D()) Panel.RestMm.Add(FVector2d(P)*10);
            for(auto T : Pattern.GetSimIndices2D()) Panel.Triangles.Add(FIntVector(T.X-Offset,T.Y-Offset,T.Z-Offset));
            ExpectedBuild.Panels.Add(MoveTemp(Panel));
        }
        for(int32 I=0;I<Lookup.Num();++I) ExpectedBuild.SettledCm.Add(FVector3d(Cloth.GetSimPosition3D()[Lookup[I]]));
    }
    else if(bReopen && !Generate(ExpectedBuild)) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 10; }
    Report->SetNumberField(TEXT("sourceFitSteps"),ExpectedBuild.Steps);
    Report->SetNumberField(TEXT("sourceFitRefinementSeconds"),ExpectedBuild.RefinementSeconds);
    Report->SetNumberField(TEXT("sourceFitLinearSolveSeconds"),ExpectedBuild.LinearSolveSeconds);
    Report->SetNumberField(TEXT("sourceFitCollisionSweepSeconds"),ExpectedBuild.CollisionSweepSeconds);
    Report->SetNumberField(TEXT("sourceFitEnergyEvaluationSeconds"),ExpectedBuild.EnergyEvaluationSeconds);
    if(bTemplate) bValid &= Asset->GetPhysicsAsset()==(bSkinnedBody?DiagnosticPhysics.Get():ReferenceBody->GetPhysicsAsset());
    if(!bRuntimeOnly)
    {
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
        && Same(Cloth.GetTetherKinematicIndex(),Expected.GetTetherKinematicIndex())
        && Same(Cloth.GetTetherReferenceLength(),Expected.GetTetherReferenceLength())
        && Cloth.GetSkeletalMeshSoftObjectPathName()==Expected.GetSkeletalMeshSoftObjectPathName()
        && Cloth.GetNumSeams()==Expected.GetNumSeams();
    if(bSourceMatches)
        for(int32 I=0; I<Cloth.GetNumSeams(); ++I)
            bSourceMatches &= Same(Cloth.GetSeam(I).GetSeamStitch2DEndIndices(),Expected.GetSeam(I).GetSeamStitch2DEndIndices());
    Report->SetBoolField(TEXT("savedGeometryMatchesSource"),bSourceMatches);
    bValid &= bSourceMatches;
    }
    if(!bValid)
    {
        Report->SetBoolField(TEXT("passed"),false);
        FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*(ReportDir/TEXT("metrics.json")));
        UE_LOG(LogTemp,Error,TEXT("Saved Cloth identity or structure differs from its regenerated source")); return 9;
    }

    const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("LaceGarmentProbeWorld")),nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    UPoseableMeshComponent* BodyComponent=nullptr;
    TArray<FTransform> ReferencePose,ComponentPose;
    if(bTemplate || bPoses)
    {
        auto* BodyActor=World->SpawnActor<AActor>();
        BodyComponent=NewObject<UPoseableMeshComponent>(BodyActor);
        BodyActor->SetRootComponent(BodyComponent); BodyActor->AddInstanceComponent(BodyComponent);
        BodyComponent->SetSkinnedAssetAndUpdate(ReferenceBody); BodyComponent->RegisterComponent();
        const auto& Skeleton=ReferenceBody->GetRefSkeleton();
        for(int32 I=0;I<Skeleton.GetNum();++I)
        {
            const int32 Parent=Skeleton.GetParentIndex(I);
            ReferencePose.Add(Parent==INDEX_NONE?Skeleton.GetRefBonePose()[I]:Skeleton.GetRefBonePose()[I]*ReferencePose[Parent]);
        }
        ComponentPose=ReferencePose;
    }
    if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("p.ChaosClothAsset.WaitForAsyncClothInitialization"))) CVar->Set(1);
    AActor* Actor=World->SpawnActor<AActor>();
    UChaosClothComponent* Component=NewObject<UChaosClothComponent>(Actor);
    Actor->SetRootComponent(Component); Actor->AddInstanceComponent(Component);
    if(bPoses) Component->SetupAttachment(BodyComponent);
    Component->SetAsset(Asset);
    Component->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->RegisterComponent();
    if(bPoses) Component->SetLeaderPoseComponent(BodyComponent,true);
    TMap<int32,FClothSimulData> Simulation;
    double PeakDisplacement=0,PeakPenetration=0,PeakAnchorDisplacement=0;
    double PeakSkinMotion=0;
    double PeakAnimatedAnchorMotion=0;
    int32 FramesWithParticles=0;
    int32 MinActualIterations=MAX_int32,MaxActualIterations=0,MaxActualSubsteps=0;
    const auto Anchors=Cloth.GetWeightMap(TEXT("MaxDistance"));
    TArray<TSharedPtr<FJsonValue>> PoseChecks;
    TArray<FVector3d> PreviousFramePositions;
    const int32 TotalFrames=bPoses?720:240;
    auto AnimatedPosition=[&](int32 I)
    {
        const FVector3d Rest(Cloth.GetSimPosition3D()[I]);
        if(!bPoses) return Rest;
        FVector3d Position=FVector3d::ZeroVector;
        const auto& Bones=Cloth.GetSimBoneIndices()[I]; const auto& Weights=Cloth.GetSimBoneWeights()[I];
        for(int32 K=0;K<Bones.Num();++K)
            Position+=ComponentPose[Bones[K]].TransformPosition(ReferencePose[Bones[K]].InverseTransformPosition(Rest))*Weights[K];
        return Position;
    };
    for(int32 Frame=0; Frame<TotalFrames; ++Frame)
    {
        if(bPoses)
        {
            double Arm=0,Leg=0;
            if(Frame>=240 && Frame<360) Arm=FMath::Min((Frame-239)/60.0,1.0);
            else if(Frame>=360 && Frame<480) Arm=FMath::Max(1.0-(Frame-359)/60.0,0.0);
            else if(Frame>=480 && Frame<600) Leg=FMath::Min((Frame-479)/60.0,1.0);
            else if(Frame>=600) Leg=FMath::Max(1.0-(Frame-599)/60.0,0.0);
            const auto& Skeleton=ReferenceBody->GetRefSkeleton();
            TArray<FTransform> Local=Skeleton.GetRefBonePose();
            for(int32 I=0;I<Local.Num();++I)
            {
                const int32 Parent=Skeleton.GetParentIndex(I);
                auto Pose=Parent==INDEX_NONE?Local[I]:Local[I]*ComponentPose[Parent];
                const FName Bone=Skeleton.GetBoneName(I);
                FQuat Rotation=FQuat::Identity;
                // A small root turn moves the pinned vertices too, so a pose
                // test cannot pass merely because all anchors stayed still.
                if(I==0) Rotation=FQuat(FVector::ZAxisVector,FMath::DegreesToRadians(5*Arm));
                else if(Bone==TEXT("upperarm_l")) Rotation=FQuat(FVector::XAxisVector,FMath::DegreesToRadians(-25*Arm));
                else if(Bone==TEXT("upperarm_r")) Rotation=FQuat(FVector::XAxisVector,FMath::DegreesToRadians(25*Arm));
                else if(Bone==TEXT("thigh_l")) Rotation=FQuat(FVector::YAxisVector,FMath::DegreesToRadians(-25*Leg));
                else if(Bone==TEXT("calf_l")) Rotation=FQuat(FVector::YAxisVector,FMath::DegreesToRadians(45*Leg));
                Pose.SetRotation(Rotation*Pose.GetRotation());
                ComponentPose[I]=Pose;
                Local[I]=Parent==INDEX_NONE?Pose:Pose.GetRelativeTransform(ComponentPose[Parent]);
            }
            BodyComponent->BoneSpaceTransforms=MoveTemp(Local);
            BodyComponent->MarkRefreshTransformDirty(); BodyComponent->RefreshBoneTransforms();
            ComponentPose=BodyComponent->GetComponentSpaceTransforms();
            if(ComponentPose.Num()!=ReferencePose.Num()) { bValid=false; break; }
            if(bTemplate && !ReadBodyCollision(ReferenceBody,ComponentPose,Input.Colliders,Input.ConvexColliders,Error))
            { UE_LOG(LogTemp,Error,TEXT("Pose collision: %s"),*Error); bValid=false; break; }
        }
        static_cast<UActorComponent*>(Component)->TickComponent(1.f/60,LEVELTICK_All,&Component->PrimaryComponentTick);
        Component->WaitForExistingParallelClothSimulation_GameThread();
        if(const auto* Proxy=Component->GetClothSimulationProxy())
        {
            MinActualIterations=FMath::Min(MinActualIterations,Proxy->GetNumIterations());
            MaxActualIterations=FMath::Max(MaxActualIterations,Proxy->GetNumIterations());
            MaxActualSubsteps=FMath::Max(MaxActualSubsteps,Proxy->GetNumSubsteps());
            const auto& FrameData=Proxy->GetCurrentSimulationData_AnyThread();
            int32 FrameParticles=0;
            bool FrameFinite=true;
            TArray<FVector3d> FramePositions; FramePositions.SetNum(Cloth.GetNumSimVertices3D());
            for(const auto& Pair : FrameData)
                for(int32 I=0; I<Pair.Value.Positions.Num(); ++I)
                {
                    ++FrameParticles;
                    const FVector3d P=Pair.Value.ComponentRelativeTransform.TransformPosition(FVector3d(Pair.Value.Positions[I]));
                    if(P.ContainsNaN() || I>=Cloth.GetNumSimVertices3D()) { bValid=false; FrameFinite=false; continue; }
                    FramePositions[I]=P;
                    const auto Animated=AnimatedPosition(I);
                    PeakSkinMotion=FMath::Max(PeakSkinMotion,(Animated-FVector3d(Cloth.GetSimPosition3D()[I])).Length());
                    const double Displacement=(P-Animated).Length();
                    PeakDisplacement=FMath::Max(PeakDisplacement,Displacement);
                    if(Anchors.IsValidIndex(I) && Anchors[I]==0)
                    {
                        PeakAnchorDisplacement=FMath::Max(PeakAnchorDisplacement,Displacement);
                        PeakAnimatedAnchorMotion=FMath::Max(PeakAnimatedAnchorMotion,(Animated-FVector3d(Cloth.GetSimPosition3D()[I])).Length());
                    }
                    PeakPenetration=FMath::Max(PeakPenetration,BodyPenetration(P));
                }
            if(FrameParticles==Cloth.GetNumSimVertices3D()) ++FramesWithParticles;
            else if(Frame>0) bValid=false;
            const bool Checkpoint=Frame==239 || (bPoses && Frame>=359 && (Frame-359)%120==0);
            if(Checkpoint && FrameFinite && FrameParticles==Cloth.GetNumSimVertices3D())
            {
                const FString Name=Frame==239?TEXT("reference"):Frame==359?TEXT("arms-raised-root-turn"):Frame==479?TEXT("return-from-arms"):Frame==599?TEXT("left-knee-bent"):TEXT("return-from-knee");
                FBuildResult Surface=ExpectedBuild;
                const auto Lookup=Cloth.GetSimVertex3DLookup();
                for(int32 I=0;I<Surface.SettledCm.Num();++I) Surface.SettledCm[I]=FramePositions[Lookup[I]];
                MeasurePanelSurface(Surface);
                Surface.MaxFacePenetrationCm=FacePenetration(Surface,Surface.SettledCm,Input.Colliders,Input.ConvexColliders);
                double Strain=0,WorstRest=0;
                int32 WorstA=INDEX_NONE,WorstB=INDEX_NONE;
                for(int32 I=0;I<Surface.Panels.Num();++I) for(auto T : Surface.Panels[I].Triangles) for(int32 E=0;E<3;++E)
                {
                    const int32 A=T[E],B=T[(E+1)%3],O=Surface.Offsets[I];
                    const double Rest=(Surface.Panels[I].RestMm[A]-Surface.Panels[I].RestMm[B]).Length()/10;
                    const double EdgeStrain=FMath::Abs((Surface.SettledCm[O+A]-Surface.SettledCm[O+B]).Length()/Rest-1);
                    if(EdgeStrain>Strain) { Strain=EdgeStrain; WorstA=O+A; WorstB=O+B; WorstRest=Rest; }
                }
                auto Check=MakeShared<FJsonObject>(); Check->SetStringField(TEXT("pose"),Name);
                double MaxParticleSpeed=0,MaxDistanceRatio=0,MaxNativeRestError=0;
                int32 NearMaxDistanceVertices=0;
                for(int32 Vertex=0;Vertex<FramePositions.Num();++Vertex)
                {
                    if(PreviousFramePositions.Num()==FramePositions.Num())
                        MaxParticleSpeed=FMath::Max(MaxParticleSpeed,(FramePositions[Vertex]-PreviousFramePositions[Vertex]).Length()*60);
                    if(Anchors[Vertex]>0)
                    {
                        const double Ratio=(FramePositions[Vertex]-AnimatedPosition(Vertex)).Length()/Anchors[Vertex];
                        MaxDistanceRatio=FMath::Max(MaxDistanceRatio,Ratio);
                        if(Ratio>=.99) ++NearMaxDistanceVertices;
                    }
                }
                for(const auto Triangle : Cloth.GetSimIndices3D()) for(int32 Edge=0;Edge<3;++Edge)
                {
                    const int32 A=Triangle[Edge],B=Triangle[(Edge+1)%3];
                    const double Rest=(FVector3d(Cloth.GetSimPosition3D()[A])-FVector3d(Cloth.GetSimPosition3D()[B])).Length();
                    if(Rest>1.e-12) MaxNativeRestError=FMath::Max(MaxNativeRestError,FMath::Abs((FramePositions[A]-FramePositions[B]).Length()/Rest-1));
                }
                Check->SetBoolField(TEXT("particleSpeedMeasured"),PreviousFramePositions.Num()==FramePositions.Num());
                Check->SetNumberField(TEXT("maxParticleSpeedCmPerSecond"),MaxParticleSpeed);
                Check->SetNumberField(TEXT("maxDistanceRatio"),MaxDistanceRatio);
                Check->SetNumberField(TEXT("verticesNearMaxDistance"),NearMaxDistanceVertices);
                Check->SetNumberField(TEXT("maxRelativeNativeRestEdgeError"),MaxNativeRestError);
                Check->SetNumberField(TEXT("actualIterations"),Proxy->GetNumIterations());
                Check->SetNumberField(TEXT("actualSubsteps"),Proxy->GetNumSubsteps());
                Check->SetNumberField(TEXT("smoothedNativeSimulationTimeMs"),Proxy->GetSimulationTime());
                if(WorstA!=INDEX_NONE)
                {
                    const FVector3d A=Surface.SettledCm[WorstA],B=Surface.SettledCm[WorstB];
                    const double Length=(B-A).Length();
                    const double TargetLength=FMath::Clamp(Length,WorstRest*.95,WorstRest*1.05);
                    const bool FixedA=Anchors[Lookup[WorstA]]==0,FixedB=Anchors[Lookup[WorstB]]==0;
                    FVector3d TargetA=A,TargetB=B;
                    if(Length>1.e-12 && !(FixedA && FixedB))
                    {
                        const FVector3d Correction=(B-A)*(1-TargetLength/Length);
                        TargetA+=Correction*(FixedA?0:FixedB?1:.5);
                        TargetB-=Correction*(FixedB?0:FixedA?1:.5);
                    }
                    const auto CorrectedA=SweepBodyCollision(A,TargetA,Input.Colliders,Input.ThicknessCm,Input.ConvexColliders);
                    const auto CorrectedB=SweepBodyCollision(B,TargetB,Input.Colliders,Input.ThicknessCm,Input.ConvexColliders);
                    Check->SetNumberField(TEXT("worstEdgeSourceA"),WorstA); Check->SetNumberField(TEXT("worstEdgeSourceB"),WorstB);
                    Check->SetArrayField(TEXT("worstEdgeCorrectionTargetsCm"),Vectors({TargetA,TargetB}));
                    Check->SetNumberField(TEXT("worstEdgeCorrectionBlockedCm"),FMath::Max((CorrectedA-TargetA).Length(),(CorrectedB-TargetB).Length()));
                    Check->SetNumberField(TEXT("worstEdgeCorrectedRelativeError"),FMath::Abs((CorrectedB-CorrectedA).Length()/WorstRest-1));
                    TArray<TSharedPtr<FJsonValue>> EndpointChecks;
                    for(const int32 SourceIndex : {WorstA,WorstB})
                    {
                        const int32 Vertex=Lookup[SourceIndex];
                        const FVector3d Position=FramePositions[Vertex];
                        auto Endpoint=MakeShared<FJsonObject>();
                        Endpoint->SetNumberField(TEXT("sourceIndex"),SourceIndex);
                        Endpoint->SetNumberField(TEXT("maxDistanceCm"),Anchors[Vertex]);
                        Endpoint->SetNumberField(TEXT("distanceToAnimatedCm"),(Position-AnimatedPosition(Vertex)).Length());
                        TArray<TSharedPtr<FJsonValue>> SkinInfluences;
                        const auto& Joints=Cloth.GetSimBoneIndices()[Vertex];
                        const auto& Weights=Cloth.GetSimBoneWeights()[Vertex];
                        for(int32 K=0;K<Joints.Num();++K)
                        {
                            auto Influence=MakeShared<FJsonObject>();
                            Influence->SetStringField(TEXT("bone"),Asset->GetRefSkeleton().GetBoneName(Joints[K]).ToString());
                            Influence->SetNumberField(TEXT("weight"),Weights[K]);
                            SkinInfluences.Add(MakeShared<FJsonValueObject>(Influence));
                        }
                        Endpoint->SetArrayField(TEXT("skinInfluences"),SkinInfluences);
                        double MinCollisionPhi=DBL_MAX;
                        for(const auto& Collider : Input.Colliders)
                        {
                            const auto Axis=Collider.B-Collider.A;
                            const auto Q=Collider.A+Axis*FMath::Clamp(FVector3d::DotProduct(Position-Collider.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
                            MinCollisionPhi=FMath::Min(MinCollisionPhi,(Position-Q).Length()-Collider.RadiusCm);
                        }
                        for(const auto& Collider : Input.ConvexColliders)
                        {
                            FVector3d Normal;
                            MinCollisionPhi=FMath::Min(MinCollisionPhi,Collider.SignedDistance(Position,Normal));
                        }
                        Endpoint->SetNumberField(TEXT("collisionSurfaceDistanceCm"),MinCollisionPhi);
                        double MaxTetherRatio=0;
                        const auto& TetherEnds=Cloth.GetTetherKinematicIndex()[Vertex];
                        const auto& TetherLengths=Cloth.GetTetherReferenceLength()[Vertex];
                        for(int32 T=0;T<TetherEnds.Num();++T)
                            if(TetherLengths[T]>0) MaxTetherRatio=FMath::Max(MaxTetherRatio,(Position-FramePositions[TetherEnds[T]]).Length()/TetherLengths[T]);
                        Endpoint->SetNumberField(TEXT("maxTetherDirectDistanceRatio"),MaxTetherRatio);
                        EndpointChecks.Add(MakeShared<FJsonValueObject>(Endpoint));
                    }
                    Check->SetArrayField(TEXT("worstEdgeEndpoints"),EndpointChecks);
                }
                bool PoseReached=true;
                if(bPoses && Frame==359)
                {
                    const auto& Skeleton=ReferenceBody->GetRefSkeleton();
                    for(const FName Bone : {FName(TEXT("forearm_l")),FName(TEXT("forearm_r"))})
                    {
                        const int32 Index=Skeleton.FindBoneIndex(Bone);
                        const double HeightGain=ComponentPose[Index].GetTranslation().Z-ReferencePose[Index].GetTranslation().Z;
                        Check->SetNumberField(Bone.ToString()+TEXT("_heightGainCm"),HeightGain);
                        PoseReached &= HeightGain>.1;
                    }
                }
                Check->SetBoolField(TEXT("poseReached"),PoseReached);
                Check->SetNumberField(TEXT("frame"),Frame+1); Check->SetNumberField(TEXT("maxRelativeEdgeError"),Strain);
                Check->SetNumberField(TEXT("maxFacePenetrationCm"),Surface.MaxFacePenetrationCm);
                Check->SetNumberField(TEXT("maxInternalFoldDegrees"),Surface.MaxInternalFoldDegrees);
                Check->SetNumberField(TEXT("minAreaRatio"),Surface.MinAreaRatio); Check->SetNumberField(TEXT("maxAreaRatio"),Surface.MaxAreaRatio);
                Check->SetArrayField(TEXT("positionsCm"),Vectors(Surface.SettledCm));
                const bool SurfacePassed=PoseReached && Strain<.1 && Surface.MaxFacePenetrationCm<.005 && Surface.MaxInternalFoldDegrees<120 && Surface.MinAreaRatio>.5 && Surface.MaxAreaRatio<1.5;
                Check->SetBoolField(TEXT("passed"),SurfacePassed); bValid &= SurfacePassed;
                UE_LOG(LogTemp,Display,TEXT("Probe pose %s: strain %.6f, face penetration %.6fcm, iterations %d, substeps %d, %s"),
                    *Name,Strain,Surface.MaxFacePenetrationCm,Proxy->GetNumIterations(),Proxy->GetNumSubsteps(),SurfacePassed?TEXT("PASS"):TEXT("FAIL"));
                PoseChecks.Add(MakeShared<FJsonValueObject>(Check));
                if((bTemplate || bPoses) && FParse::Param(*Params,TEXT("Render")))
                {
                    IFileManager::Get().MakeDirectory(*(ReportDir/Name),true);
                    bValid &= CaptureInspectionViews(World,ReportDir/Name,FVector(0,0,7),12,{FVector(18,-20,14),FVector(20,0,9),FVector(0,20,9)});
                }
            }
            if(FrameFinite && FrameParticles==Cloth.GetNumSimVertices3D()) PreviousFramePositions=MoveTemp(FramePositions);
            else PreviousFramePositions.Reset();
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
            Penetration=FMath::Max(Penetration,BodyPenetration(P));
        }
    }
    Report->SetArrayField(TEXT("runtimeCm"),Vectors(RuntimePositions));
    Report->SetNumberField(TEXT("runtimeParticles"),RuntimePoints);
    Report->SetNumberField(TEXT("runtimeMaxDisplacementCm"),Movement);
    Report->SetNumberField(TEXT("runtimeBodyPenetrationCm"),Penetration);
    Report->SetNumberField(TEXT("runtimeFramesWithParticles"),FramesWithParticles);
    Report->SetNumberField(TEXT("runtimeMinActualIterations"),MinActualIterations==MAX_int32?0:MinActualIterations);
    Report->SetNumberField(TEXT("runtimeMaxActualIterations"),MaxActualIterations);
    Report->SetNumberField(TEXT("runtimeMaxActualSubsteps"),MaxActualSubsteps);
    Report->SetNumberField(TEXT("runtimePeakDisplacementCm"),PeakDisplacement);
    Report->SetNumberField(TEXT("runtimePeakVertexPenetrationCm"),PeakPenetration);
    Report->SetNumberField(TEXT("runtimePeakAnchorDisplacementCm"),PeakAnchorDisplacement);
    Report->SetNumberField(TEXT("runtimeRequestedFrames"),TotalFrames);
    Report->SetBoolField(TEXT("runtimeDisplacementRelativeToAnimatedSkin"),bPoses);
    Report->SetNumberField(TEXT("runtimePeakSkinMotionCm"),PeakSkinMotion);
    Report->SetNumberField(TEXT("runtimePeakAnimatedAnchorMotionCm"),PeakAnimatedAnchorMotion);
    Report->SetArrayField(TEXT("poseChecks"),PoseChecks);
    bValid &= FramesWithParticles>=TotalFrames-1 && PeakDisplacement<5 && PeakPenetration<0.05 && PeakAnchorDisplacement<0.001;
    if(bPoses) bValid &= PeakSkinMotion>.01 && PeakAnimatedAnchorMotion>.01;
    bValid &= RuntimePoints==Cloth.GetNumSimVertices3D() && Movement>1.e-5 && Movement<5 && Penetration<0.05;
    if(FParse::Param(*Params,TEXT("Render")))
    {
        bValid &= CaptureInspectionViews(World,ReportDir,FVector(0,0,8.5),6,{FVector(14,-16,13),FVector(16,0,9),FVector(0,16,9)});
    }
    if(bSkinnedBody)
    {
        Report->SetBoolField(TEXT("nativeMeshRuntimeCompleted"),FramesWithParticles==TotalFrames && RuntimePoints==Cloth.GetNumSimVertices3D());
        // A changed collision representation needs its own direct surface audit.
        // Rigid-proxy diagnostics cannot certify the skinned mesh experiment.
        bValid=false;
    }
    Report->SetBoolField(TEXT("passed"),bValid);
    Component->UnregisterComponent();
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
    if(!FFileHelper::SaveStringToFile(Json,*(ReportDir/TEXT("metrics.json")))) return 8;
    UE_LOG(LogTemp,Display,TEXT("LaceGarment probe %s: runtime particles=%d, movement=%gcm, body penetration=%gcm"),bValid?TEXT("PASS"):TEXT("FAIL"),RuntimePoints,Movement,Penetration);
    return bValid?0:9;
}
