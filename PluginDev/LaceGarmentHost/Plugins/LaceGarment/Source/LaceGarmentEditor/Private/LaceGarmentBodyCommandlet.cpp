#include "LaceGarmentBodyCommandlet.h"
#include "LaceGarmentInspection.h"
#include "LaceGarmentCollisionProjection.h"
#include "CompGeom/ConvexHull3.h"
#include "Chaos/Convex.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Animation/Skeleton.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ImageUtils.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MaterialEditingLibrary.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "EditorFramework/AssetImportData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "HAL/IConsoleManager.h"

namespace
{
bool SaveBodyObject(UObject* Object)
{
    if(!Object || !Object->GetPathName().StartsWith(TEXT("/LaceGarment/"))) return false;
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
    const FString File=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    FAssetRegistryModule::AssetCreated(Object);
    return UPackage::SavePackage(Object->GetOutermost(),Object,*File,Args);
}
TSharedPtr<FJsonValue> Numbers(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    for(double V : Values) Array.Add(MakeShared<FJsonValueNumber>(V));
    return MakeShared<FJsonValueArray>(Array);
}
bool RepairTorsoSkin(USkeletalMesh* Mesh,TSharedRef<FJsonObject> Report,FString& Error)
{
    if(!Mesh || Mesh->GetPathName()!=TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"))
    { Error=TEXT("Skin repair is restricted to the independent reference body"); return false; }
    FMeshDescription Description;
    if(!Mesh->CloneMeshDescription(0,Description) || Description.IsEmpty())
    { Error=TEXT("Missing editable body mesh description"); return false; }
    FSkeletalMeshAttributes Attributes(Description);
    const FName SourceProfile(TEXT("LaceGarmentImportedSkin"));
    const bool ExistingSource=Attributes.GetSkinWeightProfileNames().Contains(SourceProfile);
    if(!ExistingSource && !Attributes.RegisterSkinWeightAttribute(SourceProfile))
    { Error=TEXT("Cannot preserve imported skin profile"); return false; }
    auto Active=Attributes.GetVertexSkinWeights(),Source=Attributes.GetVertexSkinWeights(SourceProfile);
    const auto Positions=Attributes.GetVertexPositions(); const auto& Ref=Mesh->GetRefSkeleton();
    TArray<FTransform> Bones;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        Bones.Add(Parent==INDEX_NONE?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bones[Parent]);
    }
    const int32 HipL=Ref.FindBoneIndex(TEXT("thigh_l")),HipR=Ref.FindBoneIndex(TEXT("thigh_r"));
    const int32 ArmL=Ref.FindBoneIndex(TEXT("upperarm_l")),ArmR=Ref.FindBoneIndex(TEXT("upperarm_r"));
    if(HipL==INDEX_NONE || HipR==INDEX_NONE || ArmL==INDEX_NONE || ArmR==INDEX_NONE)
    { Error=TEXT("Reference body joint layout is missing"); return false; }
    const double HipZ=(Bones[HipL].GetTranslation().Z+Bones[HipR].GetTranslation().Z)*.5;
    auto Smooth=[](double Low,double High,double X){const double T=FMath::Clamp((X-Low)/(High-Low),0.0,1.0);return T*T*(3-2*T);};
    int32 Changed=0,WithoutTorso=0;
    using namespace UE::AnimationCore;
    FBoneWeightsSettings Settings; Settings.SetNormalizeType(EBoneWeightNormalizeType::Always);
    for(const FVertexID Vertex : Description.Vertices().GetElementIDs())
    {
        if(!ExistingSource)
        {
            TArray<FBoneWeight> Original; for(const auto W : Active.Get(Vertex)) Original.Add(W);
            Source.SetRaw(Vertex,Original);
        }
        const FVector3f P=Positions[Vertex];
        TArray<FBoneWeight> Original; TArray<double> Kept; TArray<bool> Receiver;
        double Removed=0,RecipientWeight=0;
        for(const auto W : Source.Get(Vertex))
        {
            const FString Name=Ref.GetBoneName(W.GetBoneIndex()).ToString();
            const bool Leg=Name.StartsWith(TEXT("thigh")) || Name.StartsWith(TEXT("calf")) || Name.StartsWith(TEXT("foot"));
            const bool Arm=Name.StartsWith(TEXT("upperarm")) || Name.StartsWith(TEXT("forearm")) || Name.StartsWith(TEXT("hand"));
            double Keep=1;
            // The measured 15cm reference has a broad imported influence field.
            // Fade leg influence above the hip and arm influence inside the
            // lower torso; preserve the joint transition and lateral limbs.
            if(Leg) Keep=1-Smooth(HipZ+.25,HipZ+.85,P.Z);
            if(Arm)
            {
                const FVector Shoulder=Bones[Name.EndsWith(TEXT("_l"))?ArmL:ArmR].GetTranslation();
                Keep=FMath::Max(Smooth(FMath::Abs(Shoulder.Y)-.05,FMath::Abs(Shoulder.Y)+.25,FMath::Abs(P.Y)),
                    Smooth(Shoulder.Z-.5,Shoulder.Z,P.Z));
            }
            const bool Torso=Name==TEXT("pelvis") || Name==TEXT("spine_01") || Name==TEXT("chest") || Name==TEXT("neck") || Name.StartsWith(TEXT("clavicle"));
            Original.Add(W); Kept.Add(W.GetWeight()*Keep); Receiver.Add(Torso);
            Removed+=W.GetWeight()*(1-Keep); if(Torso) RecipientWeight+=W.GetWeight();
        }
        TArray<FBoneWeight> Updated;
        if(Removed>1.e-8 && RecipientWeight>1.e-8)
        {
            for(int32 I=0;I<Original.Num();++I)
            {
                const double Weight=Kept[I]+(Receiver[I]?Removed*Original[I].GetWeight()/RecipientWeight:0);
                if(Weight>0) Updated.Emplace(Original[I].GetBoneIndex(),float(Weight));
            }
        }
        else { Updated=Original; if(Removed>1.e-8) ++WithoutTorso; }
        const auto Result=FBoneWeights::Create(Updated,Settings);
        bool Different=Active.Get(Vertex).Num()!=Result.Num();
        if(!Different) for(int32 I=0;I<Result.Num();++I)
            Different |= Active.Get(Vertex).Get(I).ToInt32()!=Result[I].ToInt32();
        if(Different) { Active.Set(Vertex,Result); ++Changed; }
    }
    const bool Commit=!ExistingSource || Changed>0;
    if(Commit)
    {
        Mesh->ModifyMeshDescription(0); Mesh->CreateMeshDescription(0,MoveTemp(Description));
        if(!Mesh->CommitMeshDescription(0)) { Error=TEXT("Cannot commit body skin weights"); return false; }
        Mesh->PostEditChange(); FAssetCompilingManager::Get().FinishAllCompilation();
        if(!SaveBodyObject(Mesh)) { Error=TEXT("Cannot save repaired body skin"); return false; }
    }
    Report->SetStringField(TEXT("skinRepairVersion"),TEXT("0.3.12-torso-influence"));
    Report->SetStringField(TEXT("sourceProfile"),SourceProfile.ToString());
    Report->SetBoolField(TEXT("sourceProfileAdded"),!ExistingSource); Report->SetBoolField(TEXT("saved"),Commit);
    Report->SetNumberField(TEXT("changedVertices"),Changed); Report->SetNumberField(TEXT("unmodifiedVerticesWithoutTorsoInfluence"),WithoutTorso);
    Report->SetStringField(TEXT("scope"),TEXT("Explicit reference-body skin authoring; original profile preserved; no new bones/influences, geometry edits or collision refit"));
    return true;
}
bool FitHeadSphere(USkeletalMesh* Mesh,UPhysicsAsset* Physics,FVector& Center,double& Radius)
{
    const auto& Skeleton=Mesh->GetRefSkeleton(); const int32 Head=Skeleton.FindBoneIndex(TEXT("head"));
    const auto* Model=Mesh->GetImportedModel();
    if(Head==INDEX_NONE || !Model || Model->LODModels.IsEmpty()) return false;
    TArray<FVector> Points; FBox Bounds(ForceInit);
    for(const auto& Section : Model->LODModels[0].Sections) for(const auto& V : Section.SoftVertices)
    {
        int32 Strongest=0;
        for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(V.InfluenceWeights[K]>V.InfluenceWeights[Strongest]) Strongest=K;
        if(Section.BoneMap.IsValidIndex(V.InfluenceBones[Strongest]) && Section.BoneMap[V.InfluenceBones[Strongest]]==Head)
        { Points.Add(FVector(V.Position)); Bounds+=FVector(V.Position); }
    }
    if(Points.Num()<4) return false;
    Center=Bounds.GetCenter();
    // Deterministic one-dimensional minimax fit: head symmetry fixes X/Y, while
    // the sphere centre height minimizes the largest actual surface distance.
    auto RadiusSq=[&](double Z)
    {
        double Result=0; const FVector C(Center.X,Center.Y,Z);
        for(auto P : Points) Result=FMath::Max(Result,(P-C).SquaredLength());
        return Result;
    };
    double Low=Bounds.Min.Z,High=Bounds.Max.Z;
    for(int32 I=0;I<70;++I)
    {
        const double A=(2*Low+High)/3,B=(Low+2*High)/3;
        if(RadiusSq(A)<RadiusSq(B)) High=B; else Low=A;
    }
    Center.Z=(Low+High)*.5; Radius=FMath::Sqrt(RadiusSq(Center.Z))+0.001;
    FTransform Bone=Skeleton.GetRefBonePose()[Head];
    for(int32 Parent=Skeleton.GetParentIndex(Head);Parent!=INDEX_NONE;Parent=Skeleton.GetParentIndex(Parent)) Bone=Bone*Skeleton.GetRefBonePose()[Parent];
    for(USkeletalBodySetup* Setup : Physics->SkeletalBodySetups)
        if(Setup && Setup->BoneName==TEXT("head"))
        {
            FKSphereElem Sphere; Sphere.Center=Bone.InverseTransformPosition(Center); Sphere.Radius=float(Radius/Bone.GetScale3D().X);
            Setup->AggGeom.EmptyElements(); Setup->AggGeom.SphereElems.Add(Sphere);
            Setup->InvalidatePhysicsData(); Setup->CreatePhysicsMeshes(); return true;
        }
    return false;
}
bool FitSurfaceCapsules(USkeletalMesh* Mesh,UPhysicsAsset* Physics)
{
    const auto& Ref=Mesh->GetRefSkeleton(); const auto* Model=Mesh->GetImportedModel();
    if(!Model || Model->LODModels.IsEmpty()) return false;
    const auto& LOD=Model->LODModels[0]; TArray<FSoftSkinVertex> Imported; LOD.GetVertices(Imported);
    TArray<int32> Dominant; Dominant.Init(INDEX_NONE,Imported.Num());
    for(const auto& Section : LOD.Sections) for(int32 I=0;I<Section.SoftVertices.Num();++I)
    {
        const auto& Vertex=Section.SoftVertices[I]; int32 Strongest=0;
        for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(Vertex.InfluenceWeights[K]>Vertex.InfluenceWeights[Strongest]) Strongest=K;
        if(!Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[Strongest])) return false;
        Dominant[Section.BaseVertexIndex+I]=Section.BoneMap[Vertex.InfluenceBones[Strongest]];
    }
    TArray<FTransform> Bones;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        Bones.Add(Parent==INDEX_NONE?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bones[Parent]);
    }
    for(const FString Name : {TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("forearm_l"),TEXT("forearm_r"),TEXT("spine_01"),TEXT("calf_l"),TEXT("calf_r"),TEXT("foot_l"),TEXT("foot_r")})
    {
        const int32 Bone=Ref.FindBoneIndex(FName(Name)); int32 Child=INDEX_NONE;
        for(int32 I=0;I<Ref.GetNum();++I) if(Ref.GetParentIndex(I)==Bone) {Child=I; break;}
        const bool Foot=Name.StartsWith(TEXT("foot")),LowerLeg=Foot || Name.StartsWith(TEXT("calf"));
        if(Bone==INDEX_NONE || (!Foot && Child==INDEX_NONE)) return false;
        // Terminal feet point along the imported reference's forward axis.
        const FVector PartitionAxis=Foot?FVector::ForwardVector:(Bones[Child].GetTranslation()-Bones[Bone].GetTranslation()).GetSafeNormal();
        const bool Torso=Name==TEXT("spine_01");
        // Torso slices follow the spine in height; each capsule follows the
        // wider left/right cross-section instead of enclosing its whole height.
        const FVector Axis=Torso?FVector::RightVector:PartitionAxis;
        const FVector Basis=FMath::Abs(FVector::DotProduct(Axis,FVector::ForwardVector))>.99?FVector::UpVector:FVector::ForwardVector;
        const FVector U=FVector::CrossProduct(Axis,Basis).GetSafeNormal(),V=FVector::CrossProduct(Axis,U);
        if(Axis.IsNearlyZero() || U.IsNearlyZero()) return false;
        TArray<FVector> Points; double MinT=DBL_MAX,MaxT=-DBL_MAX;
        TArray<bool> Included; Included.Init(false,Imported.Num());
        for(int32 I=0;I<Dominant.Num();++I) Included[I]=Dominant[I]==Bone;
        // Include complete incident faces at the ankle/knee boundaries. An
        // enclosing convex capsule then covers each face, not only its corners.
        if(LowerLeg) for(int32 I=0;I<LOD.IndexBuffer.Num();I+=3)
        {
            const int32 A=LOD.IndexBuffer[I],B=LOD.IndexBuffer[I+1],C=LOD.IndexBuffer[I+2];
            if(Dominant[A]==Bone || Dominant[B]==Bone || Dominant[C]==Bone) Included[A]=Included[B]=Included[C]=true;
        }
        for(int32 I=0;I<Imported.Num();++I) if(Included[I])
        {
            const FVector P(Imported[I].Position); Points.Add(P); const double T=FVector::DotProduct(P,PartitionAxis);
            MinT=FMath::Min(MinT,T); MaxT=FMath::Max(MaxT,T);
        }
        if(Points.Num()<4) return false;
        USkeletalBodySetup* Setup=nullptr;
        for(USkeletalBodySetup* Candidate : Physics->SkeletalBodySetups) if(Candidate && Candidate->BoneName==FName(Name)) {Setup=Candidate; break;}
        if(!Setup) return false;
        Setup->AggGeom.EmptyElements();
        // The shoulder-to-elbow surface tapers. One enclosing capsule seals the
        // real armpit gap. Two overlapping measured intervals preserve that gap.
        const int32 Count=Name.StartsWith(TEXT("upperarm")) || Torso?2:1;
        for(int32 Part=0;Part<Count;++Part)
        {
            const double Low=FMath::Lerp(MinT,MaxT,double(Part)/Count)-.05,High=FMath::Lerp(MinT,MaxT,double(Part+1)/Count)+.05;
            TArray<FVector> Region; FBox Bounds(ForceInit);
            for(auto P : Points)
            {
                const FVector Local(FVector::DotProduct(P,U),FVector::DotProduct(P,V),FVector::DotProduct(P,Axis));
                const double Partition=FVector::DotProduct(P,PartitionAxis);
                if(Partition>=Low && Partition<=High) {Region.Add(Local); Bounds+=Local;}
            }
            if(Region.Num()<4) return false;
            const FVector Center=Bounds.GetCenter(); double MinRadiusSq=0,MaxRadiusSq=0;
            for(auto P : Region)
            {
                const double Radial=FMath::Square(P.X-Center.X)+FMath::Square(P.Y-Center.Y);
                MinRadiusSq=FMath::Max(MinRadiusSq,Radial); MaxRadiusSq=FMath::Max(MaxRadiusSq,Radial+FMath::Square(P.Z-Center.Z));
            }
            auto Ends=[&](double R,double& A,double& B)
            {
                A=DBL_MAX; B=-DBL_MAX;
                for(auto P : Region)
                {
                    const double Span=FMath::Sqrt(FMath::Max(0.0,R*R-FMath::Square(P.X-Center.X)-FMath::Square(P.Y-Center.Y)));
                    A=FMath::Min(A,P.Z+Span); B=FMath::Max(B,P.Z-Span);
                }
                if(A>B) A=B=(A+B)*.5;
            };
            auto Volume=[&](double R){double A,B; Ends(R,A,B); return R*R*(B-A)+(4.0/3)*R*R*R;};
            double RadiusLow=FMath::Sqrt(MinRadiusSq),RadiusHigh=FMath::Sqrt(MaxRadiusSq);
            for(int32 K=0;K<70;++K)
            {
                const double A=(2*RadiusLow+RadiusHigh)/3,B=(RadiusLow+2*RadiusHigh)/3;
                if(Volume(A)<Volume(B)) RadiusHigh=B; else RadiusLow=A;
            }
            const double Radius=(RadiusLow+RadiusHigh)*.5; double A,B; Ends(Radius,A,B);
            const FVector WorldCenter=U*Center.X+V*Center.Y+Axis*((A+B)*.5);
            const FTransform WorldShape(FQuat::FindBetweenNormals(FVector::UpVector,Axis),WorldCenter);
            const FTransform LocalShape=WorldShape.GetRelativeTransform(Bones[Bone]); const double Scale=Bones[Bone].GetScale3D().X;
            FKSphylElem Shape; Shape.Center=LocalShape.GetTranslation(); Shape.Rotation=LocalShape.Rotator();
            Shape.Radius=(Radius+.001)/Scale; Shape.Length=(B-A)/Scale;
            Setup->AggGeom.SphylElems.Add(Shape);
        }
        Setup->InvalidatePhysicsData(); Setup->CreatePhysicsMeshes();
    }
    return true;
}
bool FitSurfaceConvexes(USkeletalMesh* Mesh,UPhysicsAsset* Physics,FString& Error)
{
    const auto& Ref=Mesh->GetRefSkeleton(); const auto* Model=Mesh->GetImportedModel();
    if(!Model || Model->LODModels.IsEmpty()) {Error=TEXT("No imported body surface");return false;}
    TArray<FTransform> Bones;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        Bones.Add(Parent==INDEX_NONE?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bones[Parent]);
    }
    const auto& LOD=Model->LODModels[0];TArray<FSoftSkinVertex> Imported;LOD.GetVertices(Imported);
    TArray<int32> Dominant;Dominant.Init(INDEX_NONE,Imported.Num());
    for(const auto& Section : LOD.Sections) for(int32 I=0;I<Section.SoftVertices.Num();++I)
    {
        const auto& V=Section.SoftVertices[I];int32 Strongest=0;
        for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(V.InfluenceWeights[K]>V.InfluenceWeights[Strongest]) Strongest=K;
        if(!Section.BoneMap.IsValidIndex(V.InfluenceBones[Strongest])) {Error=TEXT("Invalid body surface influence");return false;}
        Dominant[Section.BaseVertexIndex+I]=Section.BoneMap[V.InfluenceBones[Strongest]];
    }
    for(const FString Name : {TEXT("clavicle_l"),TEXT("clavicle_r"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("pelvis"),TEXT("spine_01"),TEXT("chest")})
    {
        const int32 Bone=Ref.FindBoneIndex(FName(Name)); int32 Child=INDEX_NONE;
        for(int32 I=0;I<Ref.GetNum();++I) if(Ref.GetParentIndex(I)==Bone) {Child=I;break;}
        if(Bone==INDEX_NONE || Child==INDEX_NONE) {Error=TEXT("Limb reference bone missing");return false;}
        const FVector Axis=(Bones[Child].GetTranslation()-Bones[Bone].GetTranslation()).GetSafeNormal();
        TArray<FIntVector3> SurfaceFaces;TSet<int32> PointIndices;
        for(int32 I=0;I<LOD.IndexBuffer.Num();I+=3)
        {
            const FIntVector3 Face(LOD.IndexBuffer[I],LOD.IndexBuffer[I+1],LOD.IndexBuffer[I+2]);
            if(!Dominant.IsValidIndex(Face.X) || !Dominant.IsValidIndex(Face.Y) || !Dominant.IsValidIndex(Face.Z)) {Error=TEXT("Invalid body surface triangle");return false;}
            if(Dominant[Face.X]!=Bone && Dominant[Face.Y]!=Bone && Dominant[Face.Z]!=Bone) continue;
            SurfaceFaces.Add(Face);for(int32 K=0;K<3;++K) PointIndices.Add(Face[K]);
        }
        TArray<int32> SortedPoints=PointIndices.Array();SortedPoints.Sort();
        TArray<FVector3d> Points;double MinT=DBL_MAX,MaxT=-DBL_MAX;
        for(int32 I : SortedPoints)
        {const FVector3d P(Imported[I].Position);Points.Add(P);const double T=FVector3d::DotProduct(P,Axis);MinT=FMath::Min(MinT,T);MaxT=FMath::Max(MaxT,T);}
        USkeletalBodySetup* Setup=nullptr;
        for(USkeletalBodySetup* Candidate : Physics->SkeletalBodySetups) if(Candidate && Candidate->BoneName==FName(Name)) {Setup=Candidate;break;}
        if(!Setup || Points.Num()<4) {Error=TEXT("Limb collision source missing");return false;}
        TArray<FVector3d> Directions;
        for(int32 X=-1;X<=1;++X) for(int32 Y=-1;Y<=1;++Y) for(int32 Z=-1;Z<=1;++Z)
        {
            if(X==0 && Y==0 && Z==0) continue;
            Directions.Add(FVector3d(X,Y,Z).GetSafeNormal());
        }
        struct FProtectedSpace {FVector3d Center;double Radius;};
        TArray<FProtectedSpace> ProtectedSpaces;
        if(Name.StartsWith(TEXT("upperarm")) || Name.StartsWith(TEXT("thigh")))
        {
            for(int32 Zero=0;Zero<3;++Zero) for(int32 Step=0;Step<24;++Step)
            {
                const double Angle=UE_DOUBLE_PI*Step/12;
                FVector3d N=FVector3d::ZeroVector;N[(Zero+1)%3]=FMath::Cos(Angle);N[(Zero+2)%3]=FMath::Sin(Angle);
                if(!Directions.ContainsByPredicate([&](auto Q){return N.Equals(Q,1.e-9);})) Directions.Add(N);
            }
        }
        if(Name.StartsWith(TEXT("upperarm")))
        {
            // Authored negative-space samples for this fixed OB11 reference.
            // Verify them against the actual imported triangles before using
            // them to partition a concave surface into convex collision pieces.
            const double Sign=FMath::Sign(Bones[Bone].GetTranslation().Y);
            ProtectedSpaces={{FVector3d(0,Sign*1.5,8.0),.1},{FVector3d(0,Sign*1.45,8.2),.03}};
            for(const auto& Space : ProtectedSpaces)
            {
                double Distance=DBL_MAX;
                for(int32 I=0;I<LOD.IndexBuffer.Num();I+=3)
                {
                    const auto Q=FMath::ClosestPointOnTriangleToPoint(Space.Center,FVector3d(Imported[LOD.IndexBuffer[I]].Position),
                        FVector3d(Imported[LOD.IndexBuffer[I+1]].Position),FVector3d(Imported[LOD.IndexBuffer[I+2]].Position));
                    Distance=FMath::Min(Distance,(Space.Center-Q).Length());
                }
                if(Distance<=Space.Radius+.005) {Error=Name+TEXT(" protected space intersects the reference surface");return false;}
            }
        }
        const int32 Count=Name.StartsWith(TEXT("clavicle")) || Name==TEXT("chest")?1:Name.StartsWith(TEXT("thigh"))?8:2;
        UE_LOG(LogTemp,Display,TEXT("Body support partition %s: faces=%d axis=%s min=%.9g max=%.9g parts=%d"),*Name,SurfaceFaces.Num(),*Axis.ToString(),MinT,MaxT,Count);
        TArray<TArray<FIntVector3>> FaceGroups;FaceGroups.SetNum(Count);
        for(auto Face : SurfaceFaces)
        {
            const auto Center=(FVector3d(Imported[Face.X].Position)+FVector3d(Imported[Face.Y].Position)+FVector3d(Imported[Face.Z].Position))/3;
            const double T=FVector3d::DotProduct(Center,Axis);
            const int32 Part=FMath::Clamp(FMath::FloorToInt((T-MinT)/(MaxT-MinT)*Count),0,Count-1);
            FaceGroups[Part].Add(Face);
        }
        for(const auto& Space : ProtectedSpaces)
        {
            TArray<TArray<FIntVector3>> SplitGroups;
            auto Separated=[&](FIntVector3 Face,FVector3d Normal)
            {
                const double Limit=FVector3d::DotProduct(Space.Center,Normal)-Space.Radius-.005;
                for(int32 K=0;K<3;++K) if(FVector3d::DotProduct(FVector3d(Imported[Face[K]].Position),Normal)>Limit) return false;
                return true;
            };
            for(const auto& Group : FaceGroups)
            {
                TArray<FIntVector3> Remaining=Group;
                while(!Remaining.IsEmpty())
                {
                    int32 Best=INDEX_NONE,BestCount=0;
                    for(int32 N=0;N<Directions.Num();++N)
                    {
                        int32 Hits=0;for(auto Face : Remaining) if(Separated(Face,Directions[N])) ++Hits;
                        if(Hits>BestCount) {Best=N;BestCount=Hits;}
                    }
                    if(Best==INDEX_NONE) {Error=Name+TEXT(" cannot preserve a full triangle and protected air space");return false;}
                    TArray<FIntVector3> Selected,Rest;
                    for(auto Face : Remaining) (Separated(Face,Directions[Best])?Selected:Rest).Add(Face);
                    SplitGroups.Add(MoveTemp(Selected));Remaining=MoveTemp(Rest);
                    if(SplitGroups.Num()>16) {Error=Name+TEXT(" protected-space decomposition exceeds sixteen regions");return false;}
                }
            }
            FaceGroups=MoveTemp(SplitGroups);
        }
        // Rejoin groups only when a separating support plane still exists for
        // every protected sphere. This avoids unnecessary thin surface slivers.
        if(!ProtectedSpaces.IsEmpty()) for(;;)
        {
            int32 BestA=INDEX_NONE,BestB=INDEX_NONE,BestSize=0;
            for(int32 A=0;A<FaceGroups.Num();++A) for(int32 B=A+1;B<FaceGroups.Num();++B)
            {
                bool Allowed=true;
                for(const auto& Space : ProtectedSpaces)
                {
                    bool Separated=false;
                    for(auto N : Directions)
                    {
                        const double Limit=FVector3d::DotProduct(Space.Center,N)-Space.Radius-.005;
                        bool Inside=true;
                        for(int32 G : {A,B}) for(auto Face : FaceGroups[G]) for(int32 K=0;K<3;++K)
                            if(FVector3d::DotProduct(FVector3d(Imported[Face[K]].Position),N)>Limit) Inside=false;
                        if(Inside) {Separated=true;break;}
                    }
                    if(!Separated) {Allowed=false;break;}
                }
                const int32 Size=FaceGroups[A].Num()+FaceGroups[B].Num();
                if(Allowed && Size>BestSize) {BestA=A;BestB=B;BestSize=Size;}
            }
            if(BestA==INDEX_NONE) break;
            FaceGroups[BestA].Append(FaceGroups[BestB]);FaceGroups.RemoveAt(BestB);
        }
        TArray<FKConvexElem> Elements;
        for(int32 Part=0;Part<FaceGroups.Num();++Part)
        {
            // Include whole triangles, including neighbors across dominant-bone
            // boundaries. Splitting individual vertices leaves holes at joins.
            TSet<int32> RegionIndices;
            for(auto Face : FaceGroups[Part]) for(int32 K=0;K<3;++K) RegionIndices.Add(Face[K]);
            TArray<int32> SortedRegion=RegionIndices.Array();SortedRegion.Sort();TArray<FVector3d> Region;
            for(int32 I : SortedRegion) Region.Add(FVector3d(Imported[I].Position));
            FBox3d RegionBounds(ForceInit);for(auto P : Region) RegionBounds+=P;
            UE_LOG(LogTemp,Display,TEXT("Body support region %s/%d: points=%d bounds=%s"),*Name,Part,Region.Num(),*RegionBounds.ToString());
            if(Region.Num()<3) {Error=Name+TEXT(" has an empty collision partition");return false;}
            // Whole triangles already overlap across partitions; extending the
            // centroid bins inflates the inner-thigh gap unnecessarily. Use a
            // finer fixed direction set on the curved upper arms. Distinct face
            // directions remain well outside the cooker's near-parallel merge.
            bool Accepted=false;
            // If redundant diagonal supports still produce float sliver faces,
            // retain axis bounds and the mandatory space-separating planes.
            // Both alternatives must pass the same containment/space checks.
            const int32 SupportModes=Name.StartsWith(TEXT("thigh"))?3:2;
            for(int32 SupportMode=0;SupportMode<SupportModes && !Accepted;++SupportMode)
            {
                TArray<FVector3d> Normals; TArray<double> Supports;
                FVector3d Center=FVector3d::ZeroVector;for(auto P : Region) Center+=P;Center/=Region.Num();
                TArray<FVector3d> DualPoints;
                // Retain the coarse enclosing directions plus the strongest plane
                // for each protected sphere. The finer direction set is needed for
                // partitioning, not as dozens of redundant tiny faces per piece.
                TArray<FVector3d> SupportDirections;
                // Shallow thigh taper needs oblique supports; coarse axial bins
                // and 45-degree normals turn it into a much wider stepped hull.
                const int32 SupportCount=Name.StartsWith(TEXT("thigh")) && SupportMode==0?Directions.Num():26;
                for(int32 I=0;I<SupportCount;++I)
                    if(SupportMode<SupportModes-1 || Directions[I].GetAbsMax()>1-1.e-9) SupportDirections.Add(Directions[I]);
                for(const auto& Space : ProtectedSpaces)
                {
                    double Best=-DBL_MAX;FVector3d BestNormal=FVector3d::ZeroVector;
                    for(auto N : Directions)
                    {
                        double Support=-DBL_MAX;for(auto P : Region) Support=FMath::Max(Support,FVector3d::DotProduct(P,N));
                        const double Clearance=FVector3d::DotProduct(Space.Center,N)-Support;
                        if(Clearance>Best) {Best=Clearance;BestNormal=N;}
                    }
                    if(Best<Space.Radius+.005-1.e-9) {Error=Name+TEXT(" lost its separating support plane");return false;}
                    if(!SupportDirections.ContainsByPredicate([&](auto N){return N.Equals(BestNormal,1.e-9);})) SupportDirections.Add(BestNormal);
                }
                for(const FVector3d N : SupportDirections)
                {
                    double Support=-DBL_MAX;
                    for(auto P : Region) Support=FMath::Max(Support,FVector3d::DotProduct(P-Center,N));
                    Normals.Add(N); Supports.Add(Support);
                }
                // A constant plane offset bevels every sampled corner into tiny
                // sliver faces. Uniform inflation keeps coincident support vertices
                // coincident while providing at least the same containment margin.
                double MinSupport=DBL_MAX;for(double Support : Supports) MinSupport=FMath::Min(MinSupport,Support);
                if(MinSupport < -1.e-9) {Error=Name+TEXT(" collision region has no interior center");return false;}
                const double Inflation=MinSupport>1.e-7?1+.0005/MinSupport:0;
                const TArray<double> SourceSupports=Supports;
                // Both constructions enclose the same source with >=.0005cm plane
                // clearance. Float hull triangulation can favor either topology;
                // try these two bounded alternatives, keeping the strict validator.
                for(int32 InflationMode=0;InflationMode<2 && !Accepted;++InflationMode)
                {
                    if(InflationMode==0 && Inflation==0) continue;
                    DualPoints.Reset();
                    for(int32 I=0;I<Supports.Num();++I)
                    {
                        Supports[I]=InflationMode==0?SourceSupports[I]*Inflation:SourceSupports[I]+.0005;
                        DualPoints.Add(Normals[I]/Supports[I]);
                    }
                    // Polar duality enumerates only adjacent support planes, avoiding
                    // cubic enumeration of every plane triple at higher resolutions.
                    UE::Geometry::FConvexHull3d DualHull;
                    if(!DualHull.Solve(DualPoints.Num(),[&](int32 I){return DualPoints[I];})) {Error=Name+TEXT(" support hull is degenerate");return false;}
                    TArray<FVector3d> LocalPoints;
                    for(auto Triangle : DualHull.GetTriangles())
                    {
                        const int32 A=Triangle.A,B=Triangle.B,C=Triangle.C;
                        const auto BC=FVector3d::CrossProduct(Normals[B],Normals[C]); const double Det=FVector3d::DotProduct(Normals[A],BC);
                        if(FMath::Abs(Det)<1.e-9) continue;
                        const auto P=(Supports[A]*BC+Supports[B]*FVector3d::CrossProduct(Normals[C],Normals[A])+Supports[C]*FVector3d::CrossProduct(Normals[A],Normals[B]))/Det;
                        bool Inside=true;
                        for(int32 K=0;K<Normals.Num();++K) if(FVector3d::DotProduct(P,Normals[K])>Supports[K]+1.e-7) {Inside=false;break;}
                        if(!Inside) continue;
                        const auto Local=Bones[Bone].InverseTransformPosition(P+Center);
                        if(!LocalPoints.ContainsByPredicate([&](const FVector3d& Q){return Local.Equals(Q,1.e-5);})) LocalPoints.Add(Local);
                    }
                    LaceGarment::FConvexCollider Hull;
                    if(!LaceGarment::BuildConvexCollider(LocalPoints,Hull,Error)) return false;
                    FKConvexElem Element;
                    for(auto P : Hull.GetVertices()) Element.VertexData.Add(P);
                    // Feed a canonical spatial order to the second (float) hull build,
                    // rather than the first hull's incidental face traversal order.
                    Element.VertexData.Sort([](const FVector& A,const FVector& B)
                    {return A.X!=B.X?A.X<B.X:A.Y!=B.Y?A.Y<B.Y:A.Z<B.Z;});
                    Element.UpdateElemBox();
                    // Preflight uses the same default constructor as ChaosCooking.
                    // Its temporary native object is never used to bypass asset cooking.
                    FKConvexElem Probe=Element;TArray<Chaos::FConvex::FVec3Type> NativeVertices;
                    for(auto V : Element.VertexData) NativeVertices.Add(Chaos::FConvex::FVec3Type(V));
                    Probe.SetConvexMeshObject(Chaos::FConvexPtr(new Chaos::FConvex(NativeVertices,0.0f)));
                    LaceGarment::FConvexCollider Preflight;
                    if(!LaceGarment::ReadPhysicsConvex(Probe,Bones[Bone],Preflight,Error))
                    {
                        UE_LOG(LogTemp,Display,TEXT("Rejected body support candidate %s/%d supports %d inflation %d: %s"),*Name,Part,SupportMode,InflationMode,*Error);
                        continue;
                    }
                    bool PreservesSpace=true;
                    for(const auto& Space : ProtectedSpaces)
                    {
                        FVector3d N;if(Preflight.SignedDistance(Space.Center,N)<Space.Radius) {PreservesSpace=false;break;}
                    }
                    if(!PreservesSpace) {Error=TEXT("Cooked support covers protected arm space");continue;}
                    Elements.Add(MoveTemp(Element));Accepted=true;
                    UE_LOG(LogTemp,Display,TEXT("Accepted body support %s/%d: %d planes, support mode %d inflation %d"),*Name,Part,SupportDirections.Num(),SupportMode,InflationMode);
                }
            }
            if(!Accepted) {Error=Name+TEXT(": conservative support constructions failed default cooking: ")+Error;return false;}
        }
        Setup->AggGeom.EmptyElements(); Setup->AggGeom.ConvexElems=MoveTemp(Elements);
        Setup->InvalidatePhysicsData(); Setup->CreatePhysicsMeshes();
        TArray<LaceGarment::FConvexCollider> Cooked;
        for(const auto& Element : Setup->AggGeom.ConvexElems)
        {
            LaceGarment::FConvexCollider Collider;
            if(!LaceGarment::ReadPhysicsConvex(Element,Bones[Bone],Collider,Error))
            {
                using FNative=Chaos::FConvex;
                TArray<FNative::FVec3Type> Input,Vertices;for(auto V : Element.VertexData) Input.Add(FNative::FVec3Type(V));
                TArray<FNative::FPlaneType> Planes;TArray<TArray<int32>> Faces;FNative::FAABB3Type Bounds;
                Chaos::FConvexBuilder::Build(Input,Planes,Faces,Vertices,Bounds);
                auto Measure=[&](const TCHAR* Stage)
                {
                    double Worst=0;
                    for(int32 F=0;F<Faces.Num();++F) for(int32 I : Faces[F])
                        Worst=FMath::Max(Worst,FMath::Abs(double(FVector3d::DotProduct(FVector3d(Vertices[I])-FVector3d(Planes[F].X()),FVector3d(Planes[F].Normal())))));
                    UE_LOG(LogTemp,Display,TEXT("Convex cook diagnostic %s %s: vertices=%d faces=%d maxPlaneError=%.9gcm"),*Name,Stage,Vertices.Num(),Faces.Num(),Worst);
                };
                Measure(TEXT("Build"));Chaos::FConvexBuilder::MergeFaces(Planes,Faces,Vertices,1.0f);Measure(TEXT("MergeFaces"));
                Chaos::FConvexBuilder::MergeColinearEdges(Planes,Faces,Vertices,1.e-6f);Measure(TEXT("MergeColinearEdges"));
                Error=Name+TEXT(": ")+Error;return false;
            }
            Cooked.Add(MoveTemp(Collider));
        }
        for(const auto& Space : ProtectedSpaces) for(const auto& C : Cooked)
        {FVector3d N;if(C.SignedDistance(Space.Center,N)<Space.Radius) {Error=Name+TEXT(" cooked asset lost protected space");return false;}}
        for(auto P : Points)
        {
            double Distance=DBL_MAX;
            for(const auto& C : Cooked) {FVector3d N;Distance=FMath::Min(Distance,C.SignedDistance(P,N));}
            if(Distance>2.e-4) {Error=FString::Printf(TEXT("%s cooked convex loses surface by %.6fcm"),*Name,Distance);return false;}
        }
        for(auto Face : SurfaceFaces)
        {
            bool Covered=false;
            for(const auto& C : Cooked)
            {
                bool Inside=true;
                for(int32 K=0;K<3;++K) {FVector3d N;if(C.SignedDistance(FVector3d(Imported[Face[K]].Position),N)>2.e-4) {Inside=false;break;}}
                if(Inside) {Covered=true;break;}
            }
            if(!Covered) {Error=Name+TEXT(" cooked collision does not contain a complete source triangle");return false;}
        }
    }
    return true;
}
}

ULaceGarmentBodyCommandlet::ULaceGarmentBodyCommandlet()
{
    IsClient=true; IsServer=false; IsEditor=true; LogToConsole=true;
}

int32 ULaceGarmentBodyCommandlet::Main(const FString& Params)
{
    FString ReportDir;
    if(!FParse::Value(*Params,TEXT("ReportDir="),ReportDir)) return 1;
    IFileManager::Get().MakeDirectory(*ReportDir,true);
    FString InspectionTexture,InspectionPose=TEXT("Reference");
    const bool bInspectionTexture=FParse::Value(*Params,TEXT("InspectionTexture="),InspectionTexture);
    const bool bInspectionPose=FParse::Value(*Params,TEXT("InspectionPose="),InspectionPose);
    if(FParse::Param(*Params,TEXT("RepairSkin")))
    {
        if(bInspectionTexture || bInspectionPose) return 13;
        auto Report=MakeShared<FJsonObject>(); FString Error;
        auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
        const bool Repaired=RepairTorsoSkin(Mesh,Report,Error);
        Report->SetBoolField(TEXT("succeeded"),Repaired); Report->SetStringField(TEXT("error"),Error);
        FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        return FFileHelper::SaveStringToFile(Json,*(ReportDir/TEXT("skin-repair.json"))) && Repaired?0:13;
    }
    if(bInspectionTexture || bInspectionPose)
    {
        // Read-only visual QA: do not enter the import, collision authoring or
        // package-saving path below. Both the texture and material are transient.
        if(!FParse::Param(*Params,TEXT("Render"))) return 10;
        auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
        auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/LaceGarment/Authoring/Body/M_OB11.M_OB11"));
        const TSet<FString> Poses={TEXT("Reference"),TEXT("ArmsRaised"),TEXT("ArmsForward"),TEXT("Elbows90"),TEXT("Knees90"),TEXT("Sitting"),TEXT("HeadTurn")};
        if(!Mesh || !Material || !Poses.Contains(InspectionPose)) return 11;
        auto* Texture=bInspectionTexture?FImageUtils::ImportFileAsTexture2D(InspectionTexture):nullptr;
        if(bInspectionTexture && !Texture) return 11;
        if(Texture) { Texture->SRGB=true; Texture->Filter=TF_Bilinear; Texture->AddressX=TA_Clamp; Texture->AddressY=TA_Clamp; Texture->UpdateResource(); }
        FAssetCompilingManager::Get().FinishAllCompilation();
        const auto& Skeleton=Mesh->GetRefSkeleton();
        for(const FName Bone : {FName(TEXT("upperarm_l")),FName(TEXT("upperarm_r")),FName(TEXT("forearm_l")),FName(TEXT("forearm_r")),FName(TEXT("hand_l")),FName(TEXT("hand_r")),FName(TEXT("thigh_l")),FName(TEXT("thigh_r")),FName(TEXT("calf_l")),FName(TEXT("calf_r")),FName(TEXT("foot_l")),FName(TEXT("foot_r")),FName(TEXT("head"))})
            if(Skeleton.FindBoneIndex(Bone)==INDEX_NONE) return 11;
        const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
        auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("LaceGarmentBodyInspection"),nullptr,true,ERHIFeatureLevel::Num,&Init);
        auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
        auto* Actor=World->SpawnActor<AActor>();
        auto* Component=NewObject<UPoseableMeshComponent>(Actor); Actor->SetRootComponent(Component); Actor->AddInstanceComponent(Component);
        Component->SetSkinnedAssetAndUpdate(Mesh); Component->RegisterComponent();
        TArray<FTransform> Reference,Posed,Local=Skeleton.GetRefBonePose();
        for(int32 I=0;I<Local.Num();++I)
        {
            const int32 Parent=Skeleton.GetParentIndex(I);
            Reference.Add(Parent==INDEX_NONE?Local[I]:Local[I]*Reference[Parent]);
        }
        for(int32 I=0;I<Local.Num();++I)
        {
            const int32 Parent=Skeleton.GetParentIndex(I);
            auto Pose=Parent==INDEX_NONE?Local[I]:Local[I]*Posed[Parent];
            const FString Name=Skeleton.GetBoneName(I).ToString();
            FQuat Rotation=FQuat::Identity;
            if(Name.StartsWith(TEXT("upperarm_")) && (InspectionPose==TEXT("ArmsRaised") || InspectionPose==TEXT("ArmsForward")))
            {
                const int32 Child=Skeleton.FindBoneIndex(FName(*Name.Replace(TEXT("upperarm_"),TEXT("forearm_"))));
                const FVector Direction=Pose.TransformVectorNoScale(Skeleton.GetRefBonePose()[Child].GetTranslation()).GetSafeNormal();
                // Raise laterally to horizontal; overhead arms would intersect
                // this doll's deliberately oversized head.
                const FVector Target=InspectionPose==TEXT("ArmsRaised")?FVector(0,Direction.Y<0?-1:1,0):FVector::XAxisVector;
                Rotation=FQuat::FindBetweenNormals(Direction,Target);
            }
            else if(Name.StartsWith(TEXT("forearm_")) && InspectionPose==TEXT("Elbows90"))
            {
                const int32 Child=Skeleton.FindBoneIndex(FName(*Name.Replace(TEXT("forearm_"),TEXT("hand_"))));
                const FVector Direction=Pose.TransformVectorNoScale(Skeleton.GetRefBonePose()[Child].GetTranslation()).GetSafeNormal();
                const FVector Upper=(Pose.GetTranslation()-Posed[Parent].GetTranslation()).GetSafeNormal();
                const FVector Target=(FVector::XAxisVector-Upper*FVector::DotProduct(Upper,FVector::XAxisVector)).GetSafeNormal();
                Rotation=FQuat::FindBetweenNormals(Direction,Target);
            }
            else if(Name.StartsWith(TEXT("thigh_")) && InspectionPose==TEXT("Sitting"))
            {
                const int32 Child=Skeleton.FindBoneIndex(FName(*Name.Replace(TEXT("thigh_"),TEXT("calf_"))));
                const FVector Direction=Pose.TransformVectorNoScale(Skeleton.GetRefBonePose()[Child].GetTranslation()).GetSafeNormal();
                Rotation=FQuat::FindBetweenNormals(Direction,FVector::XAxisVector);
            }
            else if(Name.StartsWith(TEXT("calf_")) && (InspectionPose==TEXT("Knees90") || InspectionPose==TEXT("Sitting")))
            {
                const int32 Child=Skeleton.FindBoneIndex(FName(*Name.Replace(TEXT("calf_"),TEXT("foot_"))));
                const FVector Direction=Pose.TransformVectorNoScale(Skeleton.GetRefBonePose()[Child].GetTranslation()).GetSafeNormal();
                const FVector Upper=(Pose.GetTranslation()-Posed[Parent].GetTranslation()).GetSafeNormal();
                const FVector Desired=InspectionPose==TEXT("Sitting")?-FVector::ZAxisVector:-FVector::XAxisVector;
                const FVector Target=(Desired-Upper*FVector::DotProduct(Upper,Desired)).GetSafeNormal();
                Rotation=FQuat::FindBetweenNormals(Direction,Target);
            }
            else if(Name==TEXT("head") && InspectionPose==TEXT("HeadTurn")) Rotation=FQuat(FVector::ZAxisVector,PI/4);
            Pose.SetRotation(Rotation*Pose.GetRotation()); Posed.Add(Pose);
            Local[I]=Parent==INDEX_NONE?Pose:Pose.GetRelativeTransform(Posed[Parent]);
        }
        Component->BoneSpaceTransforms=MoveTemp(Local);
        Component->MarkRefreshTransformDirty(); Component->RefreshBoneTransforms();
        Posed=Component->GetComponentSpaceTransforms();
        auto* Instance=UMaterialInstanceDynamic::Create(Material,Component);
        if(Texture) Instance->SetTextureParameterValue(TEXT("BaseColorTexture"),Texture);
        for(int32 I=0;I<Component->GetNumMaterials();++I) Component->SetMaterial(I,Instance);
        const bool Rendered=LaceGarment::CaptureInspectionViews(World,ReportDir,FVector(0,0,7.5),18,{FVector(30,0,7.5),FVector(0,30,7.5),FVector(-30,0,7.5)});
        auto Report=MakeShared<FJsonObject>();
        Report->SetStringField(TEXT("scope"),TEXT("Read-only body pose/UV visual inspection; rendered does not assert pose, skin or collision acceptance; explicit texture resolution, no automatic mip-chain validation"));
        Report->SetStringField(TEXT("mesh"),Mesh->GetPathName()); Report->SetStringField(TEXT("inspectionTexture"),InspectionTexture);
        Report->SetStringField(TEXT("inspectionPose"),InspectionPose);
        if(Texture) { Report->SetNumberField(TEXT("textureWidth"),Texture->GetSizeX()); Report->SetNumberField(TEXT("textureHeight"),Texture->GetSizeY()); }
        TArray<TSharedPtr<FJsonValue>> BoneReport;
        for(int32 I=0;I<Posed.Num();++I)
        {
            auto Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("name"),Skeleton.GetBoneName(I).ToString());
            Item->SetNumberField(TEXT("parent"),Skeleton.GetParentIndex(I));
            const auto P=Posed[I].GetTranslation(),R=Reference[I].GetTranslation();
            const auto Q=Posed[I].GetRotation(),RQ=Reference[I].GetRotation();
            Item->SetField(TEXT("referenceOriginCm"),Numbers({R.X,R.Y,R.Z})); Item->SetField(TEXT("posedOriginCm"),Numbers({P.X,P.Y,P.Z}));
            Item->SetField(TEXT("referenceRotation"),Numbers({RQ.X,RQ.Y,RQ.Z,RQ.W})); Item->SetField(TEXT("posedRotation"),Numbers({Q.X,Q.Y,Q.Z,Q.W}));
            BoneReport.Add(MakeShared<FJsonValueObject>(Item));
        }
        Report->SetArrayField(TEXT("bones"),BoneReport);
        // Evaluate the imported full influence sets against the actual component
        // pose for independent joint-area checks after rendering.
        const auto* Model=Mesh->GetImportedModel();
        bool PoseDataValid=Model && !Model->LODModels.IsEmpty() && Posed.Num()==Reference.Num();
        TArray<TSharedPtr<FJsonValue>> SkinPositions;
        if(PoseDataValid)
        {
            const auto& LOD=Model->LODModels[0];
            TArray<FSoftSkinVertex> ReferenceVertices; LOD.GetVertices(ReferenceVertices);
            FSHA1 GeometryHash;
            for(const auto& Vertex : ReferenceVertices)
            {
                const float Values[]={Vertex.Position.X,Vertex.Position.Y,Vertex.Position.Z,Vertex.UVs[0].X,Vertex.UVs[0].Y};
                GeometryHash.Update(reinterpret_cast<const uint8*>(Values),sizeof(Values));
            }
            for(uint32 Index : LOD.IndexBuffer) GeometryHash.Update(reinterpret_cast<const uint8*>(&Index),sizeof(Index));
            GeometryHash.Final(); uint8 Digest[20]; GeometryHash.GetHash(Digest);
            Report->SetStringField(TEXT("referenceGeometrySha1"),BytesToHex(Digest,20).ToLower());
            Report->SetStringField(TEXT("referenceGeometryHashScope"),TEXT("Imported vertex XYZ/UV0 float32 little-endian followed by triangle indices uint32 little-endian"));
            TArray<FVector3d> Points; Points.SetNumZeroed(LOD.NumVertices);
            for(const auto& Section : LOD.Sections) for(int32 V=0;V<Section.SoftVertices.Num();++V)
            {
                const auto& Vertex=Section.SoftVertices[V];
                FVector3d Position=FVector3d::ZeroVector;
                for(int32 K=0;K<MAX_TOTAL_INFLUENCES;++K) if(Vertex.InfluenceWeights[K])
                {
                    if(!Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[K])) { PoseDataValid=false; continue; }
                    const int32 Bone=Section.BoneMap[Vertex.InfluenceBones[K]];
                    if(!Posed.IsValidIndex(Bone)) { PoseDataValid=false; continue; }
                    Position+=Posed[Bone].TransformPosition(Reference[Bone].InverseTransformPosition(FVector3d(Vertex.Position)))*(double(Vertex.InfluenceWeights[K])/65535);
                }
                const int32 Index=Section.BaseVertexIndex+V;
                if(Position.ContainsNaN() || !Points.IsValidIndex(Index)) PoseDataValid=false;
                else Points[Index]=Position;
            }
            for(const auto& P : Points) SkinPositions.Add(Numbers({P.X,P.Y,P.Z}));
            TArray<LaceGarment::FCapsule> Capsules; TArray<LaceGarment::FConvexCollider> Convexes; FString CollisionError;
            const bool CollisionValid=LaceGarment::ReadBodyCollision(Mesh,Posed,Capsules,Convexes,CollisionError);
            Report->SetBoolField(TEXT("posedCollisionRead"),CollisionValid);
            Report->SetStringField(TEXT("posedCollisionError"),CollisionError);
            if(CollisionValid)
            {
                int32 SampleCount=0,Uncovered=0; double MaxGap=0; FVector3d WorstGapPosition=FVector3d::ZeroVector;
                TArray<TSharedPtr<FJsonValue>> Examples;
                auto CheckSurface=[&](const FVector3d& P)
                {
                    ++SampleCount; double Phi=DBL_MAX;
                    for(const auto& C : Capsules)
                    {
                        const auto Axis=C.B-C.A;
                        const auto Q=C.A+Axis*FMath::Clamp(FVector3d::DotProduct(P-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
                        Phi=FMath::Min(Phi,(P-Q).Length()-C.RadiusCm);
                        if(Phi<=.005) return;
                    }
                    for(const auto& C : Convexes)
                    {
                        FVector3d Normal; Phi=FMath::Min(Phi,C.SignedDistance(P,Normal));
                        if(Phi<=.005) return;
                    }
                    ++Uncovered; if(Phi>MaxGap) { MaxGap=Phi; WorstGapPosition=P; }
                    if(Examples.Num()<16) Examples.Add(Numbers({P.X,P.Y,P.Z}));
                };
                for(const auto& P : Points) CheckSurface(P);
                for(int32 T=0;T+2<LOD.IndexBuffer.Num();T+=3)
                {
                    const int32 A=LOD.IndexBuffer[T],B=LOD.IndexBuffer[T+1],C=LOD.IndexBuffer[T+2];
                    if(!Points.IsValidIndex(A) || !Points.IsValidIndex(B) || !Points.IsValidIndex(C)) { PoseDataValid=false; continue; }
                    CheckSurface((Points[A]+Points[B]+Points[C])/3);
                    CheckSurface((Points[A]+Points[B])/2); CheckSurface((Points[B]+Points[C])/2); CheckSurface((Points[C]+Points[A])/2);
                }
                Report->SetNumberField(TEXT("bodyCollisionSamples"),SampleCount);
                Report->SetNumberField(TEXT("uncoveredBodySamples"),Uncovered);
                Report->SetNumberField(TEXT("maxUncoveredBodyGapCm"),MaxGap);
                Report->SetField(TEXT("worstUncoveredBodyPositionCm"),Numbers({WorstGapPosition.X,WorstGapPosition.Y,WorstGapPosition.Z}));
                TArray<TSharedPtr<FJsonValue>> Nearby;
                if(Uncovered>0)
                {
                    TArray<int32> Indices; for(int32 I=0;I<Points.Num();++I) Indices.Add(I);
                    Indices.Sort([&](int32 A,int32 B){return (Points[A]-WorstGapPosition).SquaredLength()<(Points[B]-WorstGapPosition).SquaredLength();});
                    for(int32 N=0;N<FMath::Min(3,Indices.Num());++N)
                    {
                        const int32 Index=Indices[N];
                        for(const auto& Section : LOD.Sections) if(Index>=int32(Section.BaseVertexIndex) && Index<int32(Section.BaseVertexIndex)+Section.SoftVertices.Num())
                        {
                            const auto& Vertex=Section.SoftVertices[Index-Section.BaseVertexIndex];
                            auto Item=MakeShared<FJsonObject>(); Item->SetNumberField(TEXT("index"),Index);
                            const auto R=Vertex.Position,P=FVector3f(Points[Index]);
                            Item->SetField(TEXT("referenceCm"),Numbers({R.X,R.Y,R.Z})); Item->SetField(TEXT("posedCm"),Numbers({P.X,P.Y,P.Z}));
                            TArray<TSharedPtr<FJsonValue>> Influences;
                            for(int32 K=0;K<MAX_TOTAL_INFLUENCES;++K) if(Vertex.InfluenceWeights[K] && Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[K]))
                            {
                                auto Influence=MakeShared<FJsonObject>();
                                Influence->SetStringField(TEXT("bone"),Skeleton.GetBoneName(Section.BoneMap[Vertex.InfluenceBones[K]]).ToString());
                                Influence->SetNumberField(TEXT("weight"),double(Vertex.InfluenceWeights[K])/65535);
                                Influences.Add(MakeShared<FJsonValueObject>(Influence));
                            }
                            Item->SetArrayField(TEXT("skinInfluences"),Influences); Nearby.Add(MakeShared<FJsonValueObject>(Item)); break;
                        }
                    }
                }
                Report->SetArrayField(TEXT("worstUncoveredBodyNearbyVertices"),Nearby);
                Report->SetArrayField(TEXT("uncoveredBodyExamplePositionsCm"),Examples);
                Report->SetStringField(TEXT("bodyCollisionCoverageScope"),TEXT("Vertices, triangle centroids and edge midpoints within 0.005cm; not a continuous whole-face or self-intersection proof"));
            }
            else PoseDataValid=false;
        }
        Report->SetArrayField(TEXT("skinnedPositionsCm"),SkinPositions);
        Report->SetBoolField(TEXT("poseDataValid"),PoseDataValid);
        Report->SetBoolField(TEXT("rendered"),Rendered); Report->SetBoolField(TEXT("assetsSaved"),false);
        World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
        FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        return FFileHelper::SaveStringToFile(Json,*(ReportDir/TEXT("inspection.json"))) && Rendered && PoseDataValid?0:12;
    }
    const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("LaceGarment"));
    if(!Plugin) return 2;
    const FString Source=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/TEXT("Resources/ReferenceBody"));
    const FString Folder=TEXT("/LaceGarment/Authoring/Body");
    auto& AssetTools=FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    USkeletalMesh* Mesh=nullptr;
    if(FPackageName::DoesPackageExist(Folder/TEXT("SK_OB11"))) Mesh=LoadObject<USkeletalMesh>(nullptr,*(Folder/TEXT("SK_OB11.SK_OB11")));
    else
    {
        UAssetImportTask* Task=NewObject<UAssetImportTask>();
        Task->Filename=Source/TEXT("LaceLoftDummy.fbx"); Task->DestinationPath=Folder; Task->DestinationName=TEXT("SK_OB11");
        Task->bAutomated=true; Task->bSave=false; Task->bReplaceExisting=false;
        UFbxFactory* Factory=NewObject<UFbxFactory>(); Factory->SetDetectImportTypeOnImport(false); Task->Factory=Factory;
        UFbxImportUI* Options=NewObject<UFbxImportUI>();
        Options->bAutomatedImportShouldDetectType=false; Options->MeshTypeToImport=FBXIT_SkeletalMesh;
        Options->bImportAsSkeletal=true; Options->bImportMesh=true; Options->bImportAnimations=false;
        Options->bCreatePhysicsAsset=false; Options->bImportMaterials=false; Options->bImportTextures=false;
        Options->SkeletalMeshImportData->bConvertScene=true; Options->SkeletalMeshImportData->bConvertSceneUnit=true;
        Options->SkeletalMeshImportData->bForceFrontXAxis=true; Options->SkeletalMeshImportData->NormalImportMethod=FBXNIM_ImportNormals;
        Task->Options=Options;
        if(auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.FBX"))) CVar->Set(false,ECVF_SetByCode);
        AssetTools.ImportAssetTasks({Task});
        for(UObject* Object : Task->GetObjects()) if(auto* Imported=Cast<USkeletalMesh>(Object)) Mesh=Imported;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(!Mesh || !Mesh->GetSkeleton()) return 3;
    if(Mesh->GetAssetImportData()) Mesh->GetAssetImportData()->SourceData.SourceFiles.Reset();

    UTexture2D* Texture=nullptr;
    if(FPackageName::DoesPackageExist(Folder/TEXT("T_OB11_White"))) Texture=LoadObject<UTexture2D>(nullptr,*(Folder/TEXT("T_OB11_White.T_OB11_White")));
    else
    {
        UAssetImportTask* Task=NewObject<UAssetImportTask>(); Task->Filename=Source/TEXT("BaseColor_White.png");
        Task->DestinationPath=Folder; Task->DestinationName=TEXT("T_OB11_White"); Task->bAutomated=true; Task->bSave=false;
        Task->Factory=NewObject<UTextureFactory>(); AssetTools.ImportAssetTasks({Task});
        for(UObject* Object : Task->GetObjects()) if(auto* Imported=Cast<UTexture2D>(Object)) Texture=Imported;
    }
    if(!Texture) return 4;
    Texture->SRGB=true; Texture->AddressX=TA_Clamp; Texture->AddressY=TA_Clamp; Texture->LODGroup=TEXTUREGROUP_Character;
    if(Texture->AssetImportData) Texture->AssetImportData->SourceData.SourceFiles.Reset();
    Texture->PostEditChange();
    UPackage* MaterialPackage=CreatePackage(*(Folder/TEXT("M_OB11"))); MaterialPackage->FullyLoad();
    UMaterial* Material=FindObject<UMaterial>(MaterialPackage,TEXT("M_OB11"));
    if(!Material)
    {
        Material=NewObject<UMaterial>(MaterialPackage,TEXT("M_OB11"),RF_Public|RF_Standalone);
        auto* Sample=CastChecked<UMaterialExpressionTextureSampleParameter2D>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionTextureSampleParameter2D::StaticClass()));
        Sample->ParameterName=TEXT("BaseColorTexture"); Sample->Texture=Texture;
        UMaterialEditingLibrary::ConnectMaterialProperty(Sample,TEXT("RGB"),MP_BaseColor);
        auto* Roughness=CastChecked<UMaterialExpressionConstant>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionConstant::StaticClass()));
        Roughness->R=0.65f; UMaterialEditingLibrary::ConnectMaterialProperty(Roughness,TEXT(""),MP_Roughness);
        Material->SetMaterialUsage(MATUSAGE_SkeletalMesh); UMaterialEditingLibrary::RecompileMaterial(Material);
    }
    // Upgrade the generated body's paint controls without removing source nodes
    // or replacing the user's texture. The default appearance remains white.
    UMaterialExpressionTextureSampleParameter2D* PaintSample=nullptr;
    UMaterialExpressionVectorParameter* Tint=nullptr;
    UMaterialExpressionScalarParameter* Roughness=nullptr;
    for(UMaterialExpression* Expression : Material->GetExpressions())
    {
        if(auto* P=Cast<UMaterialExpressionTextureSampleParameter2D>(Expression); P && P->ParameterName==TEXT("BaseColorTexture")) PaintSample=P;
        if(auto* P=Cast<UMaterialExpressionVectorParameter>(Expression); P && P->ParameterName==TEXT("Tint")) Tint=P;
        if(auto* P=Cast<UMaterialExpressionScalarParameter>(Expression); P && P->ParameterName==TEXT("Roughness")) Roughness=P;
    }
    if(!PaintSample) { UE_LOG(LogTemp,Error,TEXT("Generated body material is missing BaseColorTexture")); return 5; }
    bool MaterialChanged=false;
    if(!Tint)
    {
        Tint=CastChecked<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionVectorParameter::StaticClass()));
        Tint->ParameterName=TEXT("Tint"); Tint->DefaultValue=FLinearColor::White;
        auto* Multiply=CastChecked<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionMultiply::StaticClass()));
        UMaterialEditingLibrary::ConnectMaterialExpressions(PaintSample,TEXT("RGB"),Multiply,TEXT("A"));
        UMaterialEditingLibrary::ConnectMaterialExpressions(Tint,TEXT("RGB"),Multiply,TEXT("B"));
        UMaterialEditingLibrary::ConnectMaterialProperty(Multiply,TEXT(""),MP_BaseColor); MaterialChanged=true;
    }
    if(!Roughness)
    {
        Roughness=CastChecked<UMaterialExpressionScalarParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionScalarParameter::StaticClass()));
        Roughness->ParameterName=TEXT("Roughness"); Roughness->DefaultValue=.65f;
        UMaterialEditingLibrary::ConnectMaterialProperty(Roughness,TEXT(""),MP_Roughness); MaterialChanged=true;
    }
    if(MaterialChanged) UMaterialEditingLibrary::RecompileMaterial(Material);
    for(auto& Slot : Mesh->GetMaterials()) Slot.MaterialInterface=Material;
    UPackage* PhysicsPackage=CreatePackage(*(Folder/TEXT("PA_OB11"))); PhysicsPackage->FullyLoad();
    UPhysicsAsset* Physics=FindObject<UPhysicsAsset>(PhysicsPackage,TEXT("PA_OB11"));
    if(!Physics)
    {
        Physics=NewObject<UPhysicsAsset>(PhysicsPackage,TEXT("PA_OB11"),RF_Public|RF_Standalone);
        FPhysAssetCreateParams Fit; Fit.MinBoneSize=0.3f; Fit.MinWeldSize=0.01f; Fit.bCreateConstraints=false; Fit.bIncludeChildBones=false;
        FText Error;
        if(!FPhysicsAssetUtils::CreateFromSkeletalMesh(Physics,Mesh,Fit,Error,true,false))
        { UE_LOG(LogTemp,Error,TEXT("Body collision creation failed: %s"),*Error.ToString()); return 5; }
    }
    FVector HeadCenter; double HeadRadius=0;
    if(!FitHeadSphere(Mesh,Physics,HeadCenter,HeadRadius)) return 5;
    if(!FitSurfaceCapsules(Mesh,Physics)) return 5;
    FString ConvexError;
    if(!FitSurfaceConvexes(Mesh,Physics,ConvexError)) {UE_LOG(LogTemp,Error,TEXT("Body convex fit failed: %s"),*ConvexError);return 5;}
    Mesh->SetPhysicsAsset(Physics);
    TArray<LaceGarment::FCapsule> AuditCapsules;TArray<LaceGarment::FConvexCollider> AuditConvexes;
    if(!LaceGarment::ReadBodyCollision(Mesh,AuditCapsules,AuditConvexes,ConvexError)) {UE_LOG(LogTemp,Error,TEXT("Body collision extraction failed: %s"),*ConvexError);return 5;}
    FAssetCompilingManager::Get().FinishAllCompilation();
    for(UObject* Object : TArray<UObject*>{Texture,Material,Physics,Mesh->GetSkeleton(),Mesh}) if(!SaveBodyObject(Object)) return 6;

    const auto* Model=Mesh->GetImportedModel();
    if(!Model || Model->LODModels.IsEmpty()) return 7;
    TArray<FSoftSkinVertex> Vertices; Model->LODModels[0].GetVertices(Vertices);
    TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("scope"),TEXT("Independent body import; UV overlap and collision-fit acceptance are separate checks"));
    Report->SetNumberField(TEXT("heightCm"),Mesh->GetImportedBounds().BoxExtent.Z*2);
    Report->SetNumberField(TEXT("bones"),Mesh->GetRefSkeleton().GetNum());
    Report->SetNumberField(TEXT("physicsBodies"),Physics->SkeletalBodySetups.Num());
    int32 ShapeCount=0; for(const USkeletalBodySetup* Setup : Physics->SkeletalBodySetups) if(Setup) ShapeCount+=Setup->AggGeom.GetElementCount();
    Report->SetNumberField(TEXT("physicsShapes"),ShapeCount);
    Report->SetStringField(TEXT("collisionFitVersion"),TEXT("0.3.11-lower-leg-surface"));
    TArray<TSharedPtr<FJsonValue>> CapsuleReport,ConvexReport;
    for(const auto& C : AuditCapsules)
    {
        auto Item=MakeShared<FJsonObject>();Item->SetField(TEXT("aCm"),Numbers({C.A.X,C.A.Y,C.A.Z}));Item->SetField(TEXT("bCm"),Numbers({C.B.X,C.B.Y,C.B.Z}));Item->SetNumberField(TEXT("radiusCm"),C.RadiusCm);
        CapsuleReport.Add(MakeShared<FJsonValueObject>(Item));
    }
    for(const auto& C : AuditConvexes)
    {
        auto Item=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Points,Planes;
        for(auto P : C.GetVertices()) Points.Add(Numbers({P.X,P.Y,P.Z}));
        for(auto P : C.GetPlanes()) Planes.Add(Numbers({P.X,P.Y,P.Z,P.W}));
        Item->SetArrayField(TEXT("verticesCm"),Points);Item->SetArrayField(TEXT("planesCm"),Planes);ConvexReport.Add(MakeShared<FJsonValueObject>(Item));
    }
    Report->SetArrayField(TEXT("capsules"),CapsuleReport);Report->SetArrayField(TEXT("convexes"),ConvexReport);
    Report->SetField(TEXT("headProxyCenterCm"),Numbers({HeadCenter.X,HeadCenter.Y,HeadCenter.Z}));
    Report->SetNumberField(TEXT("headProxyRadiusCm"),HeadRadius);
    TArray<TSharedPtr<FJsonValue>> BoneReports;
    const auto& Ref=Mesh->GetRefSkeleton(); TArray<FTransform> BoneTransforms;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        BoneTransforms.Add(Parent==INDEX_NONE?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*BoneTransforms[Parent]);
        auto BoneReport=MakeShared<FJsonObject>(); BoneReport->SetStringField(TEXT("name"),Ref.GetBoneName(I).ToString());
        BoneReport->SetNumberField(TEXT("parent"),Parent);
        const auto Origin=BoneTransforms[I].GetTranslation(); BoneReport->SetField(TEXT("originCm"),Numbers({Origin.X,Origin.Y,Origin.Z}));
        TArray<TSharedPtr<FJsonValue>> DominantPoints;
        for(const auto& Section : Model->LODModels[0].Sections) for(const auto& V : Section.SoftVertices)
        {
            int32 Strongest=0;
            for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(V.InfluenceWeights[K]>V.InfluenceWeights[Strongest]) Strongest=K;
            if(Section.BoneMap.IsValidIndex(V.InfluenceBones[Strongest]) && Section.BoneMap[V.InfluenceBones[Strongest]]==I)
                DominantPoints.Add(Numbers({V.Position.X,V.Position.Y,V.Position.Z}));
        }
        BoneReport->SetArrayField(TEXT("dominantPointsCm"),DominantPoints); BoneReports.Add(MakeShared<FJsonValueObject>(BoneReport));
    }
    Report->SetArrayField(TEXT("boneSurface"),BoneReports);
    TArray<TSharedPtr<FJsonValue>> Positions,UV,Faces;
    for(const auto& Vertex : Vertices)
    {
        Positions.Add(Numbers({Vertex.Position.X,Vertex.Position.Y,Vertex.Position.Z})); UV.Add(Numbers({Vertex.UVs[0].X,Vertex.UVs[0].Y}));
    }
    const auto& Indices=Model->LODModels[0].IndexBuffer;
    for(int32 I=0; I+2<Indices.Num(); I+=3) Faces.Add(Numbers({double(Indices[I]),double(Indices[I+1]),double(Indices[I+2])}));
    Report->SetArrayField(TEXT("positionsCm"),Positions); Report->SetArrayField(TEXT("uv0"),UV); Report->SetArrayField(TEXT("triangles"),Faces);
    FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
    if(!FFileHelper::SaveStringToFile(Json,*(ReportDir/TEXT("body.json")))) return 8;
    if(FParse::Param(*Params,TEXT("Render")))
    {
        const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("LaceGarmentBodyWorld")),nullptr,true,ERHIFeatureLevel::Num,&Init);
        auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
        AActor* Actor=World->SpawnActor<AActor>();
        auto* Component=NewObject<USkeletalMeshComponent>(Actor); Actor->SetRootComponent(Component); Actor->AddInstanceComponent(Component);
        Component->SetSkeletalMeshAsset(Mesh); Component->RegisterComponent();
        const bool bCaptured=LaceGarment::CaptureInspectionViews(World,ReportDir,FVector(0,0,7.5),18,{FVector(30,-30,18),FVector(30,0,7.5),FVector(0,30,7.5)});
        Component->UnregisterComponent(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
        if(!bCaptured) return 9;
    }
    UE_LOG(LogTemp,Display,TEXT("Independent body saved: %d vertices, %.4f cm height, %d bones"),Vertices.Num(),Mesh->GetImportedBounds().BoxExtent.Z*2,Mesh->GetRefSkeleton().GetNum());
    return 0;
}
