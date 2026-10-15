#include "LaceGarmentBodyCommandlet.h"
#include "LaceGarmentInspection.h"
#include "LaceGarmentCollisionProjection.h"
#include "CompGeom/ConvexHull3.h"
#include "Chaos/Convex.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
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
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionConstant.h"
#include "MaterialEditingLibrary.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "EditorFramework/AssetImportData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
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
    TArray<FTransform> Bones;
    for(int32 I=0;I<Ref.GetNum();++I)
    {
        const int32 Parent=Ref.GetParentIndex(I);
        Bones.Add(Parent==INDEX_NONE?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bones[Parent]);
    }
    for(const FString Name : {TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("forearm_l"),TEXT("forearm_r"),TEXT("spine_01")})
    {
        const int32 Bone=Ref.FindBoneIndex(FName(Name)); int32 Child=INDEX_NONE;
        for(int32 I=0;I<Ref.GetNum();++I) if(Ref.GetParentIndex(I)==Bone) {Child=I; break;}
        if(Bone==INDEX_NONE || Child==INDEX_NONE) return false;
        const FVector PartitionAxis=(Bones[Child].GetTranslation()-Bones[Bone].GetTranslation()).GetSafeNormal();
        const bool Torso=Name==TEXT("spine_01");
        // Torso slices follow the spine in height; each capsule follows the
        // wider left/right cross-section instead of enclosing its whole height.
        const FVector Axis=Torso?FVector::RightVector:PartitionAxis;
        const FVector U=FVector::CrossProduct(Axis,FVector::ForwardVector).GetSafeNormal(),V=FVector::CrossProduct(Axis,U);
        if(Axis.IsNearlyZero() || U.IsNearlyZero()) return false;
        TArray<FVector> Points; double MinT=DBL_MAX,MaxT=-DBL_MAX;
        for(const auto& Section : Model->LODModels[0].Sections) for(const auto& Vertex : Section.SoftVertices)
        {
            int32 Strongest=0;
            for(int32 K=1;K<MAX_TOTAL_INFLUENCES;++K) if(Vertex.InfluenceWeights[K]>Vertex.InfluenceWeights[Strongest]) Strongest=K;
            if(Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[Strongest]) && Section.BoneMap[Vertex.InfluenceBones[Strongest]]==Bone)
            {
                const FVector P(Vertex.Position); Points.Add(P); const double T=FVector::DotProduct(P,PartitionAxis);
                MinT=FMath::Min(MinT,T); MaxT=FMath::Max(MaxT,T);
            }
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
    Report->SetStringField(TEXT("collisionFitVersion"),TEXT("0.3.10-thigh-taper"));
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
