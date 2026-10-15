#include "LaceGarmentPipeline.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "AssetCompilingManager.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Distance/DistPoint3Triangle3.h"
namespace LaceGarment
{
bool TransferBodySkin(FBuildResult& Build, USkeletalMesh* Body, FString& Error, EBodySkinTransfer Method)
{
    using namespace UE::Geometry;
    Error.Reset();
    auto Fail=[&](const TCHAR* Reason) { Error=Reason; return false; };
    if(!Build.bConverged || Build.SettledCm.IsEmpty() || !Body || !Body->GetPathName().StartsWith(TEXT("/LaceGarment/")))
        return Fail(TEXT("Skin transfer requires a converged garment and an independent reference body"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    const auto* Model=Body->GetImportedModel();
    if(!Model || Model->LODModels.IsEmpty() || !Body->GetSkeleton()) return Fail(TEXT("Reference body has no source geometry or skeleton"));
    const auto& LOD=Model->LODModels[0];
    TArray<FSoftSkinVertex> Vertices; LOD.GetVertices(Vertices);
    if(Vertices.IsEmpty() || Vertices.Num()>1000000 || LOD.IndexBuffer.Num()>3000000)
        return Fail(TEXT("Reference body geometry exceeds supported limits"));
    TArray<TMap<int32,double>> BodyWeights; BodyWeights.SetNum(Vertices.Num());
    for(const auto& Section : LOD.Sections)
        for(int32 I=0; I<Section.SoftVertices.Num(); ++I)
        {
            const int32 Index=Section.BaseVertexIndex+I;
            if(!BodyWeights.IsValidIndex(Index)) return Fail(TEXT("Invalid reference body section layout"));
            const auto& Vertex=Section.SoftVertices[I];
            uint32 Sum=0;
            for(int32 K=0; K<MAX_TOTAL_INFLUENCES; ++K)
                if(Vertex.InfluenceWeights[K])
                {
                    if(!Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[K])) return Fail(TEXT("Reference skin has invalid local bone"));
                    const int32 Joint=Section.BoneMap[Vertex.InfluenceBones[K]];
                    if(!Body->GetRefSkeleton().IsValidIndex(Joint)) return Fail(TEXT("Reference skin has invalid skeleton joint"));
                    BodyWeights[Index].FindOrAdd(Joint)+=double(Vertex.InfluenceWeights[K])/65535;
                    Sum+=Vertex.InfluenceWeights[K];
                }
            if(Sum!=65535 || BodyWeights[Index].Num()>4) return Fail(TEXT("Reference skin requires normalized one-to-four influences"));
        }
    FDynamicMesh3 Surface;
    for(const auto& V : Vertices)
    {
        if(V.Position.ContainsNaN()) return Fail(TEXT("Reference body contains non-finite positions"));
        Surface.AppendVertex(FVector3d(V.Position));
    }
    for(int32 I=0; I+2<LOD.IndexBuffer.Num(); I+=3)
        if(Surface.AppendTriangle(int32(LOD.IndexBuffer[I]),int32(LOD.IndexBuffer[I+1]),int32(LOD.IndexBuffer[I+2]))<0)
            return Fail(TEXT("Reference body contains invalid or non-manifold topology"));
    FDynamicMeshAABBTree3 Spatial(&Surface,true);
    TArray<int32> Parent;
    for(int32 I=0; I<Build.SettledCm.Num(); ++I) Parent.Add(I);
    auto Root=[&](int32 I) { while(Parent[I]!=I) { Parent[I]=Parent[Parent[I]]; I=Parent[I]; } return I; };
    for(auto S : Build.Stitches)
    {
        if(!Parent.IsValidIndex(S.X) || !Parent.IsValidIndex(S.Y)) return Fail(TEXT("Skin transfer received invalid stitches"));
        Parent[Root(S.Y)]=Root(S.X);
    }
    TMap<int32,FVector3d> GroupPoints; TMap<int32,int32> GroupCounts;
    for(int32 I=0; I<Parent.Num(); ++I)
    {
        if(Build.SettledCm[I].ContainsNaN()) return Fail(TEXT("Skin transfer received non-finite garment positions"));
        const int32 R=Root(I); GroupPoints.FindOrAdd(R,FVector3d::ZeroVector)+=Build.SettledCm[I]; ++GroupCounts.FindOrAdd(R);
    }
    TArray<TArray<int32>> Joints; TArray<TArray<float>> Weights;
    Joints.SetNum(Parent.Num()); Weights.SetNum(Parent.Num());
    for(const auto& Group : GroupPoints)
    {
        const FVector3d P=Group.Value/GroupCounts[Group.Key];
        double DistanceSquared;
        const int32 Triangle=Spatial.FindNearestTriangle(P,DistanceSquared);
        if(Triangle<0 || DistanceSquared>25) return Fail(TEXT("Garment point is more than 5 cm from the reference body"));
        const FIndex3i T=Surface.GetTriangle(Triangle);
        FDistPoint3Triangle3d Query(P,FTriangle3d(Surface.GetVertex(T.A),Surface.GetVertex(T.B),Surface.GetVertex(T.C)));
        Query.GetSquared();
        TMap<int32,double> Combined;
        if(Method==EBodySkinTransfer::SurfaceVertex)
        {
            int32 Corner=0;
            for(int32 K=1; K<3; ++K)
                if((Surface.GetVertex(T[K])-Query.ClosestTrianglePoint).SquaredLength()<(Surface.GetVertex(T[Corner])-Query.ClosestTrianglePoint).SquaredLength()) Corner=K;
            // Copy an existing surface vertex's complete influence set. This is
            // an explicit sampling mode, not truncation of interpolated weights.
            Combined=BodyWeights[T[Corner]];
        }
        else if(Method==EBodySkinTransfer::BarycentricStrict)
        {
            for(int32 K=0; K<3; ++K)
                for(const auto& W : BodyWeights[T[K]])
                {
                    const double Value=W.Value*Query.TriangleBaryCoords[K];
                    if(Value>0) Combined.FindOrAdd(W.Key)+=Value;
                }
        }
        else return Fail(TEXT("Unsupported body skin transfer method"));
        if(Combined.Num()<1 || Combined.Num()>4)
        {
            Error=FString::Printf(TEXT("Transferred skin has %d influences at %s (body triangle %d); manual redistribution is required, no weights were truncated"),Combined.Num(),*P.ToString(),Triangle);
            return false;
        }
        TArray<int32> Sorted; Combined.GetKeys(Sorted); Sorted.Sort();
        double Sum=0; for(int32 J : Sorted) Sum+=Combined[J];
        if(!FMath::IsFinite(Sum) || Sum<=0) return Fail(TEXT("Transferred skin has invalid total weight"));
        Joints[Group.Key]=Sorted;
        for(int32 J : Sorted) Weights[Group.Key].Add(float(Combined[J]/Sum));
    }
    for(int32 I=0; I<Parent.Num(); ++I)
        if(Root(I)!=I) { Joints[I]=Joints[Root(I)]; Weights[I]=Weights[Root(I)]; }
    Build.SkinJoints=MoveTemp(Joints); Build.SkinWeights=MoveTemp(Weights); Build.ReferenceMesh=FSoftObjectPath(Body); Build.SkinTransfer=Method;
    return true;
}
}
