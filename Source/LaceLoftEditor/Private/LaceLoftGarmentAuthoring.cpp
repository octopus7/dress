#include "Garments/LaceLoftPatternDefinition.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "ChaosClothAsset/CollectionClothRenderPatternFacade.h"
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
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace
{
using namespace UE::Chaos::ClothAsset;
using FJson=TSharedPtr<FJsonObject>;

FVector2D Vec2(const TSharedPtr<FJsonValue>& Value)
{ const auto& A=Value->AsArray();return FVector2D(A[0]->AsNumber(),A[1]->AsNumber()); }
FVector Vec3(const TSharedPtr<FJsonValue>& Value)
{ const auto& A=Value->AsArray();return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()); }

FLaceLoftPatternPath ReadPath(const FJson& Json)
{
    FLaceLoftPatternPath Path;Path.PathId=FName(Json->GetStringField(TEXT("PathId")));
    Path.bClosed=Json->GetBoolField(TEXT("bClosed"));
    for(const auto& Value:Json->GetArrayField(TEXT("PointsMm")))Path.PointsMm.Add(Vec2(Value));
    return Path;
}

template<typename T> T* Asset(const FString& Path)
{
    UPackage* Package=CreatePackage(*Path);Package->FullyLoad();
    const FString Name=FPackageName::GetShortName(Path);
    T* Object=FindObject<T>(Package,*Name);
    if(!Object){Object=NewObject<T>(Package,*Name,RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Object);}
    return Object;
}

bool Save(UObject* Object)
{
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    const FString Filename=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Object->GetOutermost(),Object,*Filename,Args);
}

UMaterial* Material(const FString& Name,FLinearColor Color)
{
    auto* Mat=Asset<UMaterial>(TEXT("/Game/Garments/Materials/")+Name);
    if(Mat->GetExpressions().IsEmpty())
    {
        auto* Tint=CastChecked<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Mat,UMaterialExpressionVectorParameter::StaticClass()));
        Tint->ParameterName=TEXT("Tint");Tint->DefaultValue=Color;
        UMaterialEditingLibrary::ConnectMaterialProperty(Tint,TEXT(""),MP_BaseColor);
    }
    Mat->TwoSided=true;Mat->SetMaterialUsage(MATUSAGE_Clothing);Mat->SetMaterialUsage(MATUSAGE_SkeletalMesh);
    UMaterialEditingLibrary::RecompileMaterial(Mat);
    return Save(Mat)?Mat:nullptr;
}

ULaceLoftPatternDefinition* ReadPattern(const FString& Id,FJson& Json)
{
    const FString Filename=FPaths::ProjectDir()/TEXT("ArtSource/Patterns/")+Id+TEXT(".json");
    FString Source;
    if(!FFileHelper::LoadFileToString(Source,*Filename) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source),Json))return nullptr;
    auto* P=Asset<ULaceLoftPatternDefinition>(TEXT("/Game/Garments/Patterns/DA_")+Id);
    P->PatternId=FName(Json->GetStringField(TEXT("PatternId")));P->PatternVersion=Json->GetIntegerField(TEXT("PatternVersion"));
    P->DisplayName=FText::FromString(Json->GetStringField(TEXT("DisplayName")));
    P->DummyMeasurementsVersion=Json->GetStringField(TEXT("DummyMeasurementsVersion"));
    P->SourceHash=LexToString(FMD5Hash::HashFile(*Filename));
    P->Slot=Id==TEXT("TShirt")?ELaceLoftGarmentSlot::Top:ELaceLoftGarmentSlot::Bottom;
    P->Pieces.Reset();P->Seams.Reset();
    for(const auto& Value:Json->GetArrayField(TEXT("Pieces")))
    {
        const auto J=Value->AsObject();auto& Piece=P->Pieces.AddDefaulted_GetRef();
        Piece.PieceId=FName(J->GetStringField(TEXT("PieceId")));Piece.DisplayName=FText::FromString(J->GetStringField(TEXT("DisplayName")));
        Piece.LayoutMm=Vec2(J->TryGetField(TEXT("LayoutMm")));Piece.GrainDirection=Vec2(J->TryGetField(TEXT("GrainDirection")));
        Piece.CutOutline=ReadPath(J->GetObjectField(TEXT("CutOutline")));
        for(const auto& V:J->GetArrayField(TEXT("FoldLines")))Piece.FoldLines.Add(ReadPath(V->AsObject()));
        for(const auto& V:J->GetArrayField(TEXT("NotchesMm")))Piece.NotchesMm.Add(Vec2(V));
        for(const auto& V:J->GetArrayField(TEXT("SimPositionsMm")))Piece.SimPositionsMm.Add(Vec2(V));
        for(const auto& V:J->GetArrayField(TEXT("AssembledPositionsMm")))Piece.AssembledPositionsMm.Add(Vec3(V));
        for(const auto& V:J->GetArrayField(TEXT("TriangleIndices")))Piece.TriangleIndices.Add(int32(V->AsNumber()));
        for(const auto& V:J->GetArrayField(TEXT("MaxDistanceMm")))Piece.MaxDistanceMm.Add(V->AsNumber());
        for(const auto& V:J->GetArrayField(TEXT("AnchorBones")))Piece.AnchorBones.Add(FName(V->AsString()));
        for(const auto& V:J->GetArrayField(TEXT("Edges")))
        {
            const auto E=V->AsObject();auto& Edge=Piece.Edges.AddDefaulted_GetRef();
            Edge.EdgeId=FName(E->GetStringField(TEXT("EdgeId")));Edge.Path=ReadPath(E->GetObjectField(TEXT("Path")));
            Edge.bOpening=E->GetBoolField(TEXT("bOpening"));Edge.SewingDirection=E->GetIntegerField(TEXT("SewingDirection"));
            Edge.AllowanceMm=E->GetNumberField(TEXT("AllowanceMm"));
            for(const auto& Vertex:E->GetArrayField(TEXT("StitchVertices")))Edge.StitchVertices.Add(int32(Vertex->AsNumber()));
        }
    }
    for(const auto& Value:Json->GetArrayField(TEXT("Seams")))
    {
        const auto J=Value->AsObject();auto& Seam=P->Seams.AddDefaulted_GetRef();
        Seam.SeamId=FName(J->GetStringField(TEXT("SeamId")));Seam.DisplayName=FText::FromString(J->GetStringField(TEXT("DisplayName")));
        const FString Kind=J->GetStringField(TEXT("Kind"));
        Seam.Kind=Kind==TEXT("Hem")?ELaceLoftSeamKind::Hem:Kind==TEXT("Binding")?ELaceLoftSeamKind::Binding:
            Kind==TEXT("ElasticBand")?ELaceLoftSeamKind::ElasticBand:ELaceLoftSeamKind::Join;
        Seam.EaseRatio=J->GetNumberField(TEXT("EaseRatio"));
        for(const auto& V:J->GetArrayField(TEXT("Prerequisites")))Seam.Prerequisites.Add(FName(V->AsString()));
        for(const FString Key:{TEXT("SideA"),TEXT("SideB")})for(const auto& V:J->GetArrayField(Key))
        {
            const auto R=V->AsObject();FLaceLoftEdgeReference Ref;
            Ref.PieceId=FName(R->GetStringField(TEXT("PieceId")));Ref.EdgeId=FName(R->GetStringField(TEXT("EdgeId")));
            Ref.Direction=R->GetIntegerField(TEXT("Direction"));
            (Key==TEXT("SideA")?Seam.SideA:Seam.SideB).Add(Ref);
        }
    }
    const auto Errors=ValidatePattern(*P);
    for(const auto& Error:Errors)UE_LOG(LogTemp,Error,TEXT("%s: %s"),*Id,*Error);
    return Errors.IsEmpty()?P:nullptr;
}

bool BuildGarment(ULaceLoftPatternDefinition* Pattern,const FJson& Json,USkeletalMesh* Dummy,UMaterial* Main,UMaterial* Trim)
{
    auto Collection=MakeShared<FManagedArrayCollection>();FCollectionClothFacade Cloth(Collection);Cloth.DefineSchema();
    TArray<float> SourceDistances;TArray<FName> SourceAnchors;TArray<FVector2f> SourceUV;
    for(const auto& Piece:Pattern->Pieces)
    {
        TArray<FVector2f> Flat;TArray<FVector3f> Draped;TArray<FIntVector3> Faces;
        for(int32 I=0;I<Piece.SimPositionsMm.Num();++I)
        {
            Flat.Add(FVector2f(LaceLoftUnits::ToCm(Piece.SimPositionsMm[I])));
            Draped.Add(FVector3f(LaceLoftUnits::ToCm(Piece.AssembledPositionsMm[I])));
            SourceDistances.Add(LaceLoftUnits::ToCm(Piece.MaxDistanceMm[I]));SourceAnchors.Add(Piece.AnchorBones[I]);
            SourceUV.Add(FVector2f((Piece.SimPositionsMm[I]+Piece.LayoutMm)/200.));
        }
        for(int32 I=0;I<Piece.TriangleIndices.Num();I+=3)
            Faces.Emplace(Piece.TriangleIndices[I],Piece.TriangleIndices[I+1],Piece.TriangleIndices[I+2]);
        Cloth.AddGetSimPattern().Initialize(Flat,Draped,Faces);
    }
    TArray<FIntVector2> Stitches;
    TArray<FIntPoint> ValidatedStitches;TArray<FString> MappingErrors;
    if(!BuildPatternStitches(*Pattern,ValidatedStitches,MappingErrors))return false;
    for(const auto& Pair:ValidatedStitches)Stitches.Emplace(Pair.X,Pair.Y);
    Cloth.AddGetSeam().Initialize(Stitches);
    Cloth.SetSkeletalMeshSoftObjectPathName(FSoftObjectPath(Dummy));
    Cloth.SetPhysicsAssetSoftObjectPathName(FSoftObjectPath(Dummy->GetPhysicsAsset()));
    Cloth.AddWeightMap(TEXT("MaxDistance"));auto Distances=Cloth.GetWeightMap(TEXT("MaxDistance"));
    auto Bones=Cloth.GetSimBoneIndices();auto Weights=Cloth.GetSimBoneWeights();
    const auto Positions=Cloth.GetSimPosition3D();const auto SourceLookup=Cloth.GetSimVertex2DLookup();
    const auto& LOD=Dummy->GetImportedModel()->LODModels[0];
    for(int32 I=0;I<Positions.Num();++I)
    {
        float Best=MAX_flt;const FSoftSkinVertex* Closest=nullptr;const FSkelMeshSection* Section=nullptr;
        for(const auto& S:LOD.Sections)for(const auto& V:S.SoftVertices)
        {
            const float D=(V.Position-Positions[I]).SquaredLength();
            if(D<Best){Best=D;Closest=&V;Section=&S;}
        }
        if(!Closest)return false;
        float Sum=0;
        for(int32 J=0;J<UE_ARRAY_COUNT(Closest->InfluenceWeights);++J)if(Closest->InfluenceWeights[J])
        {
            Bones[I].Add(Section->BoneMap[Closest->InfluenceBones[J]]);Weights[I].Add(Closest->InfluenceWeights[J]);Sum+=Closest->InfluenceWeights[J];
        }
        for(float& W:Weights[I])W/=Sum;
        Distances[I]=MAX_flt;FName Anchor;
        for(int32 SourceIndex:SourceLookup[I])
        {
            if(SourceDistances[SourceIndex]<Distances[I])
            {Distances[I]=SourceDistances[SourceIndex];Anchor=SourceAnchors[SourceIndex];}
        }
        if(!Anchor.IsNone())
        {
            const int32 BoneIndex=Dummy->GetRefSkeleton().FindBoneIndex(Anchor);
            if(BoneIndex==INDEX_NONE)return false;
            Bones[I]={BoneIndex};Weights[I]={1.f};
        }
    }
    FClothGeometryTools::CopySimMeshToRenderMesh(Collection,FSoftObjectPath(Main),false);
    const auto Lookup=Cloth.GetSimVertex3DLookup();auto RenderBones=Cloth.GetRenderBoneIndices();
    auto RenderWeights=Cloth.GetRenderBoneWeights();auto UVs=Cloth.GetRenderUVs();
    for(int32 I=0;I<Lookup.Num();++I)
    {RenderBones[I]=Bones[Lookup[I]];RenderWeights[I]=Weights[Lookup[I]];UVs[I]={SourceUV[I]};}
    for(int32 I=0;I<Cloth.GetNumRenderPatterns();++I)
    {
        const FString Id=Pattern->Pieces[I].PieceId.ToString();
        if(Id.Contains(TEXT("rib")) || Id.Contains(TEXT("binding")) || Id==TEXT("waistband"))
            Cloth.GetRenderPattern(I).SetRenderMaterialSoftObjectPathName(FSoftObjectPath(Trim));
    }
    // A separate outer collision offset keeps the top outside the anchored
    // waistband. This is validated in simultaneous four-pose runtime captures.
    auto* Config=NewObject<UChaosClothConfig>();Config->CollisionThickness=Pattern->Slot==ELaceLoftGarmentSlot::Top?.30f:.05f;Config->MinPerParticleMass=.0000001f;
    Config->Density=.18f;Config->FrictionCoefficient=.4f;Config->DampingCoefficient=Pattern->Slot==ELaceLoftGarmentSlot::Bottom?.25f:.15f;
    Config->BendingStiffnessWeighted.Low=.2f;Config->BendingStiffnessWeighted.High=.2f;
    auto* Shared=NewObject<UChaosClothSharedSimConfig>();Shared->IterationCount=8;Shared->MaxIterationCount=12;Shared->SubdivisionCount=3;
    if(Pattern->Slot==ELaceLoftGarmentSlot::Bottom){Shared->IterationCount=12;Shared->MaxIterationCount=16;Shared->SubdivisionCount=4;}
    Chaos::FClothingSimulationConfig SimulationConfig;SimulationConfig.Initialize(Config,Shared);
    SimulationConfig.GetPropertyCollection(0)->CopyTo(&Collection.Get());
    FClothEngineTools::GenerateTethers(Collection,TEXT("MaxDistance"),true,FVector2f(0,1));
    FClothGeometryTools::CleanupAndCompactMesh(Collection);
    const FString Id=Pattern->PatternId.ToString();auto* Garment=Asset<UChaosClothAsset>(TEXT("/Game/Garments/Cloth/CA_")+Id);
    FText Error,Verbose;Garment->Build({Collection},nullptr,&Error,&Verbose);
    if(!Error.IsEmpty()){UE_LOG(LogTemp,Error,TEXT("%s: %s"),*Id,*Error.ToString());return false;}
    Garment->SetSkeleton(Dummy->GetSkeleton());
    auto* Flow=Asset<UDataflow>(TEXT("/Game/Garments/Cloth/DF_")+Id);
    const auto Nodes=Flow->GetDataflow()->GetNodes();for(const auto& Node:Nodes)Flow->GetDataflow()->RemoveNode(Node);
    UE::Dataflow::FNewNodeParameters Params;Params.Guid=FGuid::NewGuid();Params.Type=FChaosClothAssetTerminalNode_v2::StaticType();
    Params.Name=FName(Id+TEXT("Terminal"));Params.OwningObject=Flow;
    auto Node=UE::Dataflow::FNodeFactory::GetInstance()->NewNodeFromRegisteredType(*Flow->GetDataflow(),Params);
    if(!Node.IsValid())return false;
    StaticCastSharedPtr<FChaosClothAssetTerminalNode_v2>(Node)->CollectionLods[0]=*Collection;
    Garment->GetDataflowInstance().SetDataflowAsset(Flow);Garment->GetDataflowInstance().SetDataflowTerminal(Params.Name);
    Pattern->ClothAsset=Garment;FAssetCompilingManager::Get().FinishAllCompilation();
    auto Mapping=MakeShared<FJsonObject>();Mapping->SetStringField(TEXT("source_hash"),Pattern->SourceHash);
    TArray<TSharedPtr<FJsonValue>> MappedParticles;
    for(int32 I=0;I<Cloth.GetNumSimVertices3D();++I)
    {
        auto Particle=MakeShared<FJsonObject>();Particle->SetNumberField(TEXT("index"),I);
        Particle->SetNumberField(TEXT("max_distance_cm"),Cloth.GetWeightMap(TEXT("MaxDistance"))[I]);
        Particle->SetStringField(TEXT("position_cm"),FVector(Cloth.GetSimPosition3D()[I]).ToString());
        TArray<TSharedPtr<FJsonValue>> Sources;
        for(int32 Source:Cloth.GetSimVertex2DLookup()[I])Sources.Add(MakeShared<FJsonValueNumber>(Source));
        Particle->SetArrayField(TEXT("source_vertices"),Sources);MappedParticles.Add(MakeShared<FJsonValueObject>(Particle));
    }
    Mapping->SetArrayField(TEXT("particles"),MappedParticles);FString MappingJson;
    FJsonSerializer::Serialize(Mapping,TJsonWriterFactory<>::Create(&MappingJson));
    if(!FFileHelper::SaveStringToFile(MappingJson,*(FPaths::ProjectDir()/TEXT("ArtSource/Garments/")+Id+TEXT("_UE_Mapping.json"))))return false;
    UE_LOG(LogTemp,Display,TEXT("%s: %d pieces, %d vertices, %d triangles"),*Id,Cloth.GetNumSimPatterns(),Cloth.GetNumSimVertices3D(),Cloth.GetNumSimFaces());
    return Save(Flow)&&Save(Garment)&&Save(Pattern);
}
}

int32 AuthorGarments()
{
    auto* Dummy=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if(!Dummy || !Dummy->GetPhysicsAsset())return 1;
    auto* Tee=Material(TEXT("M_TShirt"),FLinearColor(.18f,.42f,.35f));
    auto* Shorts=Material(TEXT("M_DolphinShorts"),FLinearColor(.58f,.19f,.12f));
    auto* Trim=Material(TEXT("M_GarmentTrim"),FLinearColor(.87f,.8f,.63f));
    if(!Tee || !Shorts || !Trim)return 2;
    for(const FString Id:{TEXT("TShirt"),TEXT("DolphinShorts")})
    {
        FJson Json;auto* Pattern=ReadPattern(Id,Json);
        if(!Pattern || !BuildGarment(Pattern,Json,Dummy,Id==TEXT("TShirt")?Tee:Shorts,Trim))return 3;
    }
    return 0;
}
