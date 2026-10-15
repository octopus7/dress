#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "ChaosClothAsset/CollectionClothSeamFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "ChaosClothAsset/TerminalNode.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowInstance.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowGraph.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

int32 AuthorClothProbe()
{
    using namespace UE::Chaos::ClothAsset;
    USkeletalMesh* Dummy = LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if (!Dummy || !Dummy->GetPhysicsAsset()) return 1;
    FString Source;
    if (!FFileHelper::LoadFileToString(Source,*(FPaths::ProjectDir()/TEXT("ArtSource/Development/ClothProbe.json")))) return 2;
    TSharedPtr<FJsonObject> Json;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source),Json)) return 3;
    auto Collection = MakeShared<FManagedArrayCollection>();
    FCollectionClothFacade Cloth(Collection); Cloth.DefineSchema();
    for (const auto& PanelValue:Json->GetArrayField(TEXT("panels")))
    {
        const auto Panel=PanelValue->AsObject();
        TArray<FVector2f> Flat; TArray<FVector3f> Draped; TArray<FIntVector3> Faces;
        for(const auto& P:Panel->GetArrayField(TEXT("positions2d_cm")))
        { const auto& A=P->AsArray(); Flat.Emplace(A[0]->AsNumber(),A[1]->AsNumber()); }
        for(const auto& P:Panel->GetArrayField(TEXT("positions3d_cm")))
        { const auto& A=P->AsArray(); Draped.Emplace(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()); }
        for(const auto& P:Panel->GetArrayField(TEXT("triangles")))
        { const auto& A=P->AsArray(); Faces.Emplace(int32(A[0]->AsNumber()),int32(A[1]->AsNumber()),int32(A[2]->AsNumber())); }
        Cloth.AddGetSimPattern().Initialize(Flat,Draped,Faces);
    }
    TArray<FIntVector2> Stitches;
    for(const auto& P:Json->GetArrayField(TEXT("stitches")))
    { const auto& A=P->AsArray(); Stitches.Emplace(int32(A[0]->AsNumber()),int32(A[1]->AsNumber())); }
    Cloth.AddGetSeam().Initialize(Stitches);
    Cloth.SetSkeletalMeshSoftObjectPathName(FSoftObjectPath(Dummy));
    Cloth.SetPhysicsAssetSoftObjectPathName(FSoftObjectPath(Dummy->GetPhysicsAsset()));
    Cloth.AddWeightMap(TEXT("MaxDistance"));
    auto PinMap=Cloth.GetWeightMap(TEXT("MaxDistance"));
    auto SimBones=Cloth.GetSimBoneIndices(); auto SimWeights=Cloth.GetSimBoneWeights();
    const auto Positions=Cloth.GetSimPosition3D();
    TArray<FVector3f> CollarPins;
    for(const auto& P:Json->GetArrayField(TEXT("collar_pins_cm")))
    { const auto& A=P->AsArray(); CollarPins.Emplace(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()); }
    const auto& LOD=Dummy->GetImportedModel()->LODModels[0];
    // Transfer actual authored body weights in reference space at the same cm scale.
    for(int32 I=0;I<Positions.Num();++I)
    {
        float Best=TNumericLimits<float>::Max();
        const FSoftSkinVertex* Closest=nullptr; const FSkelMeshSection* ClosestSection=nullptr;
        for(const FSkelMeshSection& Section:LOD.Sections)
            for(const FSoftSkinVertex& Vertex:Section.SoftVertices)
            {
                const float Distance=(Vertex.Position-Positions[I]).SquaredLength();
                if(Distance<Best) { Best=Distance; Closest=&Vertex; ClosestSection=&Section; }
            }
        if(!Closest) return 4;
        float Sum=0;
        for(int32 J=0;J<UE_ARRAY_COUNT(Closest->InfluenceWeights);++J)
            if(Closest->InfluenceWeights[J])
            {
                SimBones[I].Add(ClosestSection->BoneMap[Closest->InfluenceBones[J]]);
                SimWeights[I].Add(float(Closest->InfluenceWeights[J]));
                Sum+=Closest->InfluenceWeights[J];
            }
        for(float& Weight:SimWeights[I])Weight/=Sum;
        float CollarDistance=TNumericLimits<float>::Max();
        for(const auto& Pin:CollarPins)CollarDistance=FMath::Min(CollarDistance,(Pin-Positions[I]).Length());
        PinMap[I]=CollarDistance<.001f ? 0.f : FMath::Min(2.5f,CollarDistance*2.f);
        // The collar attaches to the chest; arm skin weights must not drag fixed cloth through the torso.
        if(PinMap[I]==0)
        {
            SimBones[I]={Dummy->GetRefSkeleton().FindBoneIndex(TEXT("chest"))};
            SimWeights[I]={1.f};
        }
    }
    FClothGeometryTools::CopySimMeshToRenderMesh(Collection,
        FSoftObjectPath(TEXT("/Game/Development/ClothProbe/M_ProbeCloth.M_ProbeCloth")),false);
    // CopySimMeshToRenderMesh explicitly resets render weights to root in UE 5.8.
    const auto Lookup=Cloth.GetSimVertex3DLookup();
    auto RenderBones=Cloth.GetRenderBoneIndices(); auto RenderWeights=Cloth.GetRenderBoneWeights();
    for(int32 I=0;I<Lookup.Num();++I)
    { RenderBones[I]=SimBones[Lookup[I]]; RenderWeights[I]=SimWeights[Lookup[I]]; }
    UChaosClothConfig* Config=NewObject<UChaosClothConfig>();
    Config->CollisionThickness=Json->GetNumberField(TEXT("collision_thickness_cm"));
    Config->MinPerParticleMass=Json->GetNumberField(TEXT("minimum_particle_mass_kg"));
    Config->Density=.18f; Config->FrictionCoefficient=.4f; Config->DampingCoefficient=.1f;
    Config->BendingStiffnessWeighted.Low=.15f; Config->BendingStiffnessWeighted.High=.15f;
    UChaosClothSharedSimConfig* Shared=NewObject<UChaosClothSharedSimConfig>();
    Shared->IterationCount=8; Shared->MaxIterationCount=12; Shared->SubdivisionCount=3;
    Chaos::FClothingSimulationConfig SimulationConfig;
    SimulationConfig.Initialize(Config,Shared);
    SimulationConfig.GetPropertyCollection(0)->CopyTo(&Collection.Get());
    FClothEngineTools::GenerateTethers(Collection,TEXT("MaxDistance"),true,FVector2f(0,1));
    FClothGeometryTools::CleanupAndCompactMesh(Collection);

    UPackage* AssetPackage=CreatePackage(TEXT("/Game/Development/ClothProbe/CA_TShirtProbe"));
    AssetPackage->FullyLoad();
    UChaosClothAsset* Asset=FindObject<UChaosClothAsset>(AssetPackage,TEXT("CA_TShirtProbe"));
    if(!Asset)Asset=NewObject<UChaosClothAsset>(AssetPackage,TEXT("CA_TShirtProbe"),RF_Public|RF_Standalone);
    FText Error,Verbose;
    Asset->Build({Collection},nullptr,&Error,&Verbose);
    if(!Error.IsEmpty())
    { UE_LOG(LogTemp,Error,TEXT("Cloth build: %s / %s"),*Error.ToString(),*Verbose.ToString()); return 5; }
    Asset->SetSkeleton(Dummy->GetSkeleton());
    UPackage* FlowPackage=CreatePackage(TEXT("/Game/Development/ClothProbe/DF_TShirtProbe"));
    FlowPackage->FullyLoad();
    UDataflow* Flow=FindObject<UDataflow>(FlowPackage,TEXT("DF_TShirtProbe"));
    if(!Flow)Flow=NewObject<UDataflow>(FlowPackage,TEXT("DF_TShirtProbe"),RF_Public|RF_Standalone);
    const auto ExistingNodes=Flow->GetDataflow()->GetNodes();
    for(const auto& Existing:ExistingNodes)Flow->GetDataflow()->RemoveNode(Existing);
    UE::Dataflow::FNewNodeParameters NodeParams;
    NodeParams.Guid=FGuid::NewGuid(); NodeParams.Type=FChaosClothAssetTerminalNode_v2::StaticType();
    NodeParams.Name=TEXT("ProbeTerminal"); NodeParams.OwningObject=Flow;
    auto Node=UE::Dataflow::FNodeFactory::GetInstance()->NewNodeFromRegisteredType(*Flow->GetDataflow(),NodeParams);
    if(!Node.IsValid())return 6;
    auto Terminal=StaticCastSharedPtr<FChaosClothAssetTerminalNode_v2>(Node);
    Terminal->CollectionLods[0]=*Collection;
    Asset->GetDataflowInstance().SetDataflowAsset(Flow);
    Asset->GetDataflowInstance().SetDataflowTerminal(NodeParams.Name);
    FAssetCompilingManager::Get().FinishAllCompilation();
    for(UObject* Object:TArray<UObject*>{Flow,Asset})
    {
        FAssetRegistryModule::AssetCreated(Object);
        FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
        const FString Filename=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(Object->GetOutermost(),Object,*Filename,Args))return 7;
    }
    UE_LOG(LogTemp,Display,TEXT("Cloth probe authored: %d welded particles, %d triangles, %d render vertices"),
        Cloth.GetNumSimVertices3D(),Cloth.GetNumSimFaces(),Cloth.GetNumRenderVertices());
    return 0;
}
