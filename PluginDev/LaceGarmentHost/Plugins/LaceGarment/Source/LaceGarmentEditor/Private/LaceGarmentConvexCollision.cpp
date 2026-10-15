#include "LaceGarmentPipeline.h"
#include "CompGeom/ConvexHull3.h"
#include "Chaos/Convex.h"
#include "PhysicsEngine/ConvexElem.h"

namespace LaceGarment
{
struct FConvexShapeData
{
    Chaos::FConvexPtr Native;
    FTransform Bone=FTransform::Identity;
};
bool BuildConvexCollider(TConstArrayView<FVector3d> Points,FConvexCollider& Out,FString& Error)
{
    Error.Reset();
    auto Fail=[&](const TCHAR* Why){Error=Why;return false;};
    if(Points.Num()<4 || Points.Num()>4096) return Fail(TEXT("Convex collision requires 4..4096 points"));
    for(auto P : Points) if(P.ContainsNaN() || P.GetAbsMax()>10000) return Fail(TEXT("Convex collision point is invalid"));
    // Topology must be valid at the precision actually stored by Chaos. Building
    // in double then rounding can silently collapse thin faces or the entire hull.
    TArray<FVector3d> NativePoints;
    for(auto P : Points) NativePoints.Add(FVector3d(FVector3f(P)));
    UE::Geometry::FConvexHull3d Hull;
    Hull.bSaveTriangleNeighbors=true;
    auto Point=[&](int32 I){return NativePoints[I];};
    if(!Hull.Solve(Points.Num(),Point) || !Hull.IsSolutionAvailable()) return Fail(TEXT("Convex collision must span three dimensions"));
    FConvexCollider Result;
    TMap<int32,int32> Remap;
    TArray<Chaos::FConvex::FPlaneType> Planes;
    TArray<Chaos::FConvex::FVec3Type> Vertices;
    TArray<TArray<int32>> Faces;
    bool Valid=true;
    Hull.GetFaces([&](TArray<int32>& Polygon,FVector3d N)
    {
        if(Polygon.Num()<3 || N.ContainsNaN() || N.SquaredLength()<1.e-12) {Valid=false;return;}
        N.Normalize();
        TArray<int32> Face;
        for(int32 Index : Polygon)
        {
            int32* Existing=Remap.Find(Index);
            if(!Existing)
            {
                const int32 NewIndex=Vertices.Num(); Remap.Add(Index,NewIndex);
                // Use the same float vertices and planes as Chaos Cloth.
                Vertices.Add(Chaos::FConvex::FVec3Type(Points[Index]));
                Result.VerticesCm.Add(FVector3d(Vertices.Last()));
                Result.Bounds+=Result.VerticesCm.Last(); Face.Add(NewIndex);
            }
            else Face.Add(*Existing);
        }
        const Chaos::FConvex::FVec3Type NativeNormal(N);
        Planes.Emplace(Vertices[Face[0]],NativeNormal);
        Result.PlanesCm.Add(FPlane4d(Result.VerticesCm[Face[0]],FVector3d(NativeNormal)));
        Faces.Add(MoveTemp(Face));
    },Point);
    if(!Valid || Faces.Num()<4 || Vertices.Num()<4) return Fail(TEXT("Convex collision has invalid polygon faces"));
    for(auto P : Points) for(const auto& Plane : Result.PlanesCm)
        if(Plane.PlaneDot(P)>2.e-5) return Fail(TEXT("Convex collision loses source surface points at native precision"));
    auto Data=MakeShared<FConvexShapeData>();
    Data->Native=new Chaos::FConvex(MoveTemp(Planes),MoveTemp(Faces),MoveTemp(Vertices));
    if(Data->Native->NumVertices()!=Result.VerticesCm.Num() || Data->Native->GetFaces().Num()!=Result.PlanesCm.Num())
        return Fail(TEXT("Native convex collision structure is incomplete"));
    for(int32 I=0;I<Result.PlanesCm.Num();++I)
        if(Data->Native->NumPlaneVertices(I)<3) return Fail(TEXT("Native convex face is degenerate"));
    Result.Shape=Data;
    Out=MoveTemp(Result); return true;
}
double FConvexCollider::SignedDistance(FVector3d Position,FVector3d& Normal) const
{
    check(IsValid());
    Chaos::FVec3 N;
    const double Phi=Shape->Native->PhiWithNormal(Chaos::FVec3(Shape->Bone.InverseTransformPosition(Position)),N);
    Normal=Shape->Bone.TransformVectorNoScale(FVector3d(N)); return Phi;
}
bool FConvexCollider::LineInterval(FVector3d Origin,FVector3d Direction,double Thickness,FVector2d& Interval,FVector3d& EntryNormal) const
{
    check(IsValid());
    const auto Box=Bounds.ExpandBy(Thickness);
    double Lo=-DBL_MAX,Hi=DBL_MAX;
    for(int32 K=0;K<3;++K)
    {
        if(FMath::Abs(Direction[K])<1.e-12)
        {if(Origin[K]<Box.Min[K] || Origin[K]>Box.Max[K]) return false;}
        else
        {
            const double A=(Box.Min[K]-Origin[K])/Direction[K],B=(Box.Max[K]-Origin[K])/Direction[K];
            Lo=FMath::Max(Lo,FMath::Min(A,B)); Hi=FMath::Min(Hi,FMath::Max(A,B));
        }
    }
    if(Lo>Hi) return false;
    // The rounded hull is contained in the intersection of offset halfspaces.
    // Reject a line outside that intersection before the more expensive GJK casts.
    for(const auto& Plane : PlanesCm)
    {
        const double Slope=FVector3d::DotProduct(FVector3d(Plane.X,Plane.Y,Plane.Z),Direction);
        const double Distance=Plane.PlaneDot(Origin)-Thickness;
        if(FMath::Abs(Slope)<1.e-12) {if(Distance>1.e-7) return false;}
        else if(Slope<0) Lo=FMath::Max(Lo,-Distance/Slope);
        else Hi=FMath::Min(Hi,-Distance/Slope);
    }
    if(Lo>Hi+1.e-7) return false;
    // Flat-face contacts are exact when both halfspace endpoints also lie on
    // the native rounded surface. Corner/edge contacts still use native GJK.
    FVector3d EnterN,LeaveN;
    const double EnterPhi=SignedDistance(Origin+Direction*Lo,EnterN),LeavePhi=SignedDistance(Origin+Direction*Hi,LeaveN);
    if(FMath::Abs(EnterPhi-Thickness)<1.e-7 && FMath::Abs(LeavePhi-Thickness)<1.e-7)
    {Interval={Lo,Hi};EntryNormal=EnterN;return true;}
    // Start both sphere casts strictly outside. Chaos leaves hit normals undefined
    // on initial overlap, and shifting planes would incorrectly square off corners.
    Lo-=1; Hi+=1;
    const auto LocalOrigin=Shape->Bone.InverseTransformPosition(Origin),LocalDirection=Shape->Bone.InverseTransformVectorNoScale(Direction);
    Chaos::FReal Entry,Exit; Chaos::FVec3 P,N,ReverseN; int32 Face;
    if(!Shape->Native->Raycast(Chaos::FVec3(LocalOrigin+LocalDirection*Lo),Chaos::FVec3(LocalDirection),Hi-Lo,Thickness,Entry,P,N,Face)
        || !Shape->Native->Raycast(Chaos::FVec3(LocalOrigin+LocalDirection*Hi),Chaos::FVec3(-LocalDirection),Hi-Lo,Thickness,Exit,P,ReverseN,Face)) return false;
    if(Entry<=0 || Exit<=0 || !FMath::IsFinite(Entry) || !FMath::IsFinite(Exit) || N.ContainsNaN()) return false;
    Interval={Lo+Entry,Hi-Exit}; EntryNormal=Shape->Bone.TransformVectorNoScale(FVector3d(N)).GetSafeNormal();
    return Interval.X<=Interval.Y+1.e-6;
}
double FConvexCollider::TrianglePenetration(FVector3d A,FVector3d B,FVector3d C) const
{
    FBox3d TriangleBox(ForceInit); TriangleBox+=A; TriangleBox+=B; TriangleBox+=C;
    if(!Bounds.Intersect(TriangleBox)) return 0;
    // Deepest interior point of a triangle: intersect it with the hull whose
    // physical faces have moved inward by Depth. No cloth thickness is included.
    auto Intersects=[&](double Depth)
    {
        TArray<FVector3d,TInlineAllocator<32>> Polygon={A,B,C},Next;
        for(const auto& Plane : PlanesCm)
        {
            Next.Reset();
            for(int32 I=0;I<Polygon.Num();++I)
            {
                const auto U=Polygon[I],V=Polygon[(I+1)%Polygon.Num()];
                const double DU=Plane.PlaneDot(U)+Depth,DV=Plane.PlaneDot(V)+Depth;
                if(DU<=0) Next.Add(U);
                if((DU<0 && DV>0) || (DU>0 && DV<0)) Next.Add(U+(V-U)*(DU/(DU-DV)));
            }
            Swap(Polygon,Next);
            if(Polygon.IsEmpty()) return false;
        }
        return true;
    };
    if(!Intersects(0)) return 0;
    double Lo=0,Hi=Bounds.GetSize().Length();
    for(int32 I=0;I<28;++I) {const double Mid=(Lo+Hi)*.5;if(Intersects(Mid)) Lo=Mid;else Hi=Mid;}
    return Lo;
}
bool ReadPhysicsConvex(const FKConvexElem& Element,const FTransform& Bone,FConvexCollider& Out,FString& Error)
{
    Error.Reset(); auto Fail=[&](const TCHAR* Why){Error=Why;return false;};
    // Cloth extracts cooked vertices directly, ignoring FKConvexElem::Transform.
    // Unit reference scale also avoids the engine's different plane/vertex scale paths.
    if(!Bone.IsValid() || !Bone.GetScale3D().Equals(FVector::OneVector,1.e-6) || !Element.GetTransform().Equals(FTransform::Identity,1.e-6))
        return Fail(TEXT("Cloth convex requires unit bone scale and identity element transform"));
    if(!Element.GetChaosConvexMesh()) return Fail(TEXT("Physics convex has no cooked Chaos shape"));
    const auto& Native=*Element.GetChaosConvexMesh();
    const auto& Planes=Native.GetFaces();
    if(Native.NumVertices()<4 || Native.NumVertices()>4096 || Planes.Num()<4 || Planes.Num()>8192)
        return Fail(TEXT("Cooked convex has an invalid vertex or face count"));
    FConvexCollider Result; FVector3d Center=FVector3d::ZeroVector;
    for(int32 I=0;I<Native.NumVertices();++I)
    {
        const FVector3d P(Native.GetVertex(I));
        const auto World=Bone.TransformPosition(P);
        if(P.ContainsNaN() || World.ContainsNaN() || World.GetAbsMax()>10000) return Fail(TEXT("Cooked convex vertex is invalid"));
        Center+=P; Result.VerticesCm.Add(World); Result.Bounds+=World;
    }
    Center/=Native.NumVertices();
    TMap<uint64,int32> EdgeCounts;
    for(int32 I=0;I<Planes.Num();++I)
    {
        const FVector3d N(Planes[I].Normal()),P(Planes[I].X());
        if(N.ContainsNaN() || P.ContainsNaN() || FMath::Abs(N.SquaredLength()-1)>1.e-4)
            return Fail(TEXT("Cooked convex plane is invalid"));
        const FPlane4d Plane(P,N);
        if(Plane.PlaneDot(Center)>=-1.e-7) return Fail(TEXT("Cooked convex has no enclosed volume"));
        const int32 Count=Native.NumPlaneVertices(I);
        if(Count<3 || Count>Native.NumVertices()) return Fail(TEXT("Cooked convex polygon is invalid"));
        TSet<int32> Used; FVector3d Area=FVector3d::ZeroVector;
        for(int32 K=0;K<Count;++K)
        {
            const int32 A=Native.GetPlaneVertex(I,K),B=Native.GetPlaneVertex(I,(K+1)%Count);
            if(A<0 || A>=Native.NumVertices() || B<0 || B>=Native.NumVertices() || Used.Contains(A))
                return Fail(TEXT("Cooked convex polygon index is invalid"));
            Used.Add(A);
            const FVector3d U(Native.GetVertex(A)),V(Native.GetVertex(B));
            if(FMath::Abs(Plane.PlaneDot(U))>2.e-4)
            {
                Error=FString::Printf(TEXT("Cooked convex polygon disagrees with its plane: face %d/%d vertex %d/%d distance %.9gcm normal %s point %s"),
                    I,Planes.Num(),A,Native.NumVertices(),Plane.PlaneDot(U),*N.ToString(),*U.ToString());
                return false;
            }
            Area+=FVector3d::CrossProduct(U-P,V-P);
            ++EdgeCounts.FindOrAdd((uint64(FMath::Min(A,B))<<32)|uint32(FMath::Max(A,B)));
        }
        if(Area.SquaredLength()<1.e-16) return Fail(TEXT("Cooked convex polygon has zero area"));
        for(int32 K=0;K<Native.NumVertices();++K)
            if(Plane.PlaneDot(FVector3d(Native.GetVertex(K)))>2.e-4) return Fail(TEXT("Cooked convex is not a consistent convex hull"));
        // Detect simplification/cooking that drops the authored surface.
        for(auto V : Element.VertexData)
            if(V.ContainsNaN() || Plane.PlaneDot(V)>2.e-4) return Fail(TEXT("Cooked convex no longer contains the authored points"));
        Result.PlanesCm.Add(FPlane4d(Bone.TransformPosition(P),Bone.TransformVectorNoScale(N)));
    }
    for(const auto& Edge : EdgeCounts) if(Edge.Value!=2) return Fail(TEXT("Cooked convex polygon shell is not closed"));
    auto Data=MakeShared<FConvexShapeData>(); Data->Native=Native.RawCopyAsConvex(); Data->Bone=Bone;
    Result.Shape=Data; Out=MoveTemp(Result); return true;
}
}
