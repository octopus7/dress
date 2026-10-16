#include "LaceGarmentPipeline.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "LaceGarmentBending.h"
#include "LaceGarmentSurface.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Distance/DistSegment3Triangle3.h"
#include "LaceGarmentCollisionProjection.h"
#include "Misc/ScopeExit.h"
PRAGMA_DEFAULT_VISIBILITY_START
THIRD_PARTY_INCLUDES_START
#include <Eigen/SparseCholesky>
THIRD_PARTY_INCLUDES_END
PRAGMA_DEFAULT_VISIBILITY_END

namespace LaceGarment
{
namespace
{
struct FDistanceConstraint { int32 A,B; double Rest; };
struct FAngleConstraint { int32 A,B,C,D; double RestEdge; };
struct FFitRow
{
    int32 A=INDEX_NONE,B=INDEX_NONE;
    FVector3d GA=FVector3d::ZeroVector,GB=FVector3d::ZeroVector;
    double Error=0;
    int32 C=INDEX_NONE,D=INDEX_NONE;
    FVector3d GC=FVector3d::ZeroVector,GD=FVector3d::ZeroVector;
};
struct FFitBlock
{
    FVector3d Diagonal,Cross=FVector3d::ZeroVector; // xx/yy/zz, xy/xz/yz
    void Add(FVector3d G) { Diagonal+=G*G; Cross+=FVector3d(G.X*G.Y,G.X*G.Z,G.Y*G.Z); }
    FVector3d Solve(FVector3d R) const
    {
        const double A=Diagonal.X,D=Diagonal.Y,F=Diagonal.Z,B=Cross.X,C=Cross.Y,E=Cross.Z;
        const double XX=D*F-E*E,XY=C*E-B*F,XZ=B*E-C*D,YY=A*F-C*C,YZ=B*C-A*E,ZZ=A*D-B*B;
        const double Det=A*XX+B*XY+C*XZ;
        if(!FMath::IsFinite(Det) || Det<=0) return FVector3d(R.X/A,R.Y/D,R.Z/F);
        return FVector3d(XX*R.X+XY*R.Y+XZ*R.Z,XY*R.X+YY*R.Y+YZ*R.Z,XZ*R.X+YZ*R.Y+ZZ*R.Z)/Det;
    }
};
FVector3d ClosestAxisPoint(FVector3d P, const FCapsule& C)
{
    FVector3d Axis=C.B-C.A;
    return C.A + Axis*FMath::Clamp(FVector3d::DotProduct(P-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
}
}
double FacePenetration(const FBuildResult& Build,const TArray<FVector3d>& Positions,const TArray<FCapsule>& Capsules,const TArray<FConvexCollider>& Convexes)
{
    using namespace UE::Geometry;
    double Maximum=0;
    for(int32 I=0; I<Build.Panels.Num(); ++I)
        for(auto T : Build.Panels[I].Triangles)
        {
            const int32 O=Build.Offsets[I];
            const FTriangle3d Triangle(Positions[O+T.X],Positions[O+T.Y],Positions[O+T.Z]);
            for(const auto& C : Capsules)
            {
                double Distance;
                if((C.B-C.A).SquaredLength()<1.e-12)
                {
                    FDistPoint3Triangle3d Query(C.A,Triangle); Distance=Query.Get();
                }
                else
                {
                    FDistSegment3Triangle3d Query(FSegment3d(C.A,C.B),Triangle); Distance=Query.Get();
                }
                Maximum=FMath::Max(Maximum,C.RadiusCm-Distance);
            }
            for(const auto& C : Convexes)
                Maximum=FMath::Max(Maximum,C.TrianglePenetration(Positions[O+T.X],Positions[O+T.Y],Positions[O+T.Z]));
        }
    return Maximum;
}

void MeasurePanelSurface(FBuildResult& Build)
{
    Build.MaxInternalFoldDegrees=0; Build.MinAreaRatio=1; Build.MaxAreaRatio=1;
    for(int32 I=0; I<Build.Panels.Num(); ++I)
    {
        const auto& Panel=Build.Panels[I];
        TMap<uint64,FVector3d> Normals;
        for(auto T : Panel.Triangles)
        {
            const int32 O=Build.Offsets[I];
            const FVector3d N=FVector3d::CrossProduct(Build.SettledCm[O+T.Y]-Build.SettledCm[O+T.X],Build.SettledCm[O+T.Z]-Build.SettledCm[O+T.X]);
            const FVector2d A=Panel.RestMm[T.Y]-Panel.RestMm[T.X],B=Panel.RestMm[T.Z]-Panel.RestMm[T.X];
            const double Ratio=N.Length()/((A.X*B.Y-A.Y*B.X)/100);
            Build.MinAreaRatio=FMath::Min(Build.MinAreaRatio,Ratio); Build.MaxAreaRatio=FMath::Max(Build.MaxAreaRatio,Ratio);
            const FVector3d Normal=N.GetSafeNormal();
            for(int32 K=0; K<3; ++K)
            {
                const int32 X=T[K],Y=T[(K+1)%3];
                const uint64 Key=(uint64(FMath::Min(X,Y))<<32)|uint32(FMath::Max(X,Y));
                if(const FVector3d* Other=Normals.Find(Key))
                    Build.MaxInternalFoldDegrees=FMath::Max(Build.MaxInternalFoldDegrees,FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector3d::DotProduct(Normal,*Other),-1.0,1.0))));
                else Normals.Add(Key,Normal);
            }
        }
    }
}
namespace
{
bool PackUV(FBuildResult& Out, FString& Error)
{
    constexpr double Margin=16.0/2048, Gap=32.0/2048;
    TArray<FBox2d> Bounds;
    double TotalWidth=0, MaxHeight=0;
    for (const auto& Panel : Out.Panels)
    {
        FBox2d Box(ForceInit);
        for (auto P : Panel.RestMm) Box+=P;
        Bounds.Add(Box);
        TotalWidth+=Box.GetSize().X;
        MaxHeight=FMath::Max(MaxHeight,Box.GetSize().Y);
    }
    double Available=1-2*Margin-Gap*(Bounds.Num()-1);
    if (Available<=0) { Error=TEXT("Too many panels for the 2K atlas padding contract"); return false; }
    double Scale=FMath::Min(Available/TotalWidth,(1-2*Margin)/MaxHeight);
    Out.TexelsPerCm=2048*Scale*10;
    double U=Margin;
    for(int32 I=0; I<Bounds.Num(); ++I)
    {
        for(auto P : Out.Panels[I].RestMm)
            Out.UV.Add(FVector2f(U+(P.X-Bounds[I].Min.X)*Scale,Margin+(P.Y-Bounds[I].Min.Y)*Scale));
        U+=Bounds[I].GetSize().X*Scale+Gap;
    }
    return true;
}
}

bool BuildGarment(const FBuildInput& Input, FBuildResult& Out, FString& Error)
{
    Out=FBuildResult();
    Error.Reset();
    auto Fail=[&](const TCHAR* Message) { Error=Message; return false; };
    if (Input.Panels.IsEmpty() || Input.Panels.Num()>32 || Input.MaxSteps<1 || Input.MaxSteps>10000 || Input.Iterations<1 || Input.Iterations>200
        || Input.LinearIterations<1 || Input.LinearIterations>1000 || Input.RefinementIterations<1 || Input.RefinementIterations>128
        || !FMath::IsFinite(Input.TimeStep) || Input.TimeStep<1.0/1000 || Input.TimeStep>1.0/30
        || !FMath::IsFinite(Input.GravityCm) || FMath::Abs(Input.GravityCm)>2000
        || !FMath::IsFinite(Input.BendingStrength) || Input.BendingStrength<0 || Input.BendingStrength>1
        || !FMath::IsFinite(Input.ThicknessCm) || Input.ThicknessCm<0 || Input.ThicknessCm>1)
        return Fail(TEXT("Invalid generation limits or physical settings"));
    for(const FCapsule& C : Input.Colliders)
        if (C.A.ContainsNaN() || C.B.ContainsNaN() || !FMath::IsFinite(C.RadiusCm) || C.RadiusCm<=0)
            return Fail(TEXT("Invalid body collision capsule"));
    if(Input.ConvexColliders.Num()>1024) return Fail(TEXT("Too many body convex colliders"));
    for(const auto& C : Input.ConvexColliders)
        if(!C.IsValid()) return Fail(TEXT("Invalid body convex collider"));
    TArray<FDistanceConstraint> Structural;
    TArray<FAngleConstraint> Bending;
    for(const auto& Panel : Input.Panels)
    {
        FPanelMesh Mesh;
        if(!BuildPanel(Panel,Mesh,Error)) return false;
        int32 Offset=Out.InitialCm.Num();
        Out.Offsets.Add(Offset);
        Out.InitialCm.Append(Mesh.InitialCm);
        if (Out.InitialCm.Num()>65536) return Fail(TEXT("Garment exceeds 65536 simulation vertices"));
        TSet<uint64> Edges;
        TMap<uint64,int32> OppositeVertices;
        for(auto T : Mesh.Triangles)
            for(int32 K=0; K<3; ++K)
            {
                int32 A=T[K], B=T[(K+1)%3];
                uint64 Key=(uint64(FMath::Min(A,B))<<32)|uint32(FMath::Max(A,B));
                const int32 Opposite=T[(K+2)%3];
                if(const int32* Other=OppositeVertices.Find(Key))
                    Bending.Add({Offset+A,Offset+B,Offset+Opposite,Offset+*Other,(Mesh.RestMm[A]-Mesh.RestMm[B]).Length()/10});
                else OppositeVertices.Add(Key,Opposite);
                if(!Edges.Contains(Key))
                {
                    Edges.Add(Key);
                    Structural.Add({Offset+A,Offset+B,(Mesh.RestMm[A]-Mesh.RestMm[B]).Length()/10});
                }
            }
        Out.Panels.Add(MoveTemp(Mesh));
    }
    TSet<FIntPoint> UsedEdges;
    for(const auto& Seam : Input.Seams)
    {
        if (!Out.Panels.IsValidIndex(Seam.PanelA) || !Out.Panels.IsValidIndex(Seam.PanelB))
            return Fail(TEXT("Seam refers to a missing panel or edge"));
        if(!FMath::IsFinite(Seam.EaseRatio) || Seam.EaseRatio<=0 || Seam.EaseRatio>10
            || !FMath::IsFinite(Seam.LengthToleranceMm) || Seam.LengthToleranceMm<0 || Seam.LengthToleranceMm>100)
            return Fail(TEXT("Seam ease or length tolerance is invalid"));
        TArray<int32> A,B;
        auto Chain=[&](int32 Panel,const TArray<int32>& Edges,int32 Default,TArray<int32>& Vertices)
        {
            const TArray<int32> Selected=Edges.IsEmpty()?TArray<int32>{Default}:Edges;
            for(int32 Edge : Selected)
            {
                const FIntPoint Key(Panel,Edge);
                if(!Out.Panels[Panel].BoundaryVertices.IsValidIndex(Edge) || UsedEdges.Contains(Key)) return false;
                const auto& Points=Out.Panels[Panel].BoundaryVertices[Edge];
                if(!Vertices.IsEmpty() && Vertices.Last()!=Points[0]) return false;
                UsedEdges.Add(Key);
                if(Vertices.IsEmpty()) Vertices.Add(Points[0]);
                for(int32 I=1;I<Points.Num();++I) Vertices.Add(Points[I]);
            }
            return true;
        };
        if(!Chain(Seam.PanelA,Seam.BoundaryEdgesA,Seam.EdgeA,A) || !Chain(Seam.PanelB,Seam.BoundaryEdgesB,Seam.EdgeB,B))
            return Fail(TEXT("Seam boundary chain is missing, repeated, or disconnected"));
        if(A.Num()!=B.Num()) return Fail(TEXT("Seam sampling does not match; resample paired boundaries before sewing"));
        auto Length=[&](int32 Panel,const TArray<int32>& V)
        {
            double L=0;
            for(int32 I=1;I<V.Num();++I) L+=(Out.Panels[Panel].RestMm[V[I]]-Out.Panels[Panel].RestMm[V[I-1]]).Length();
            return L;
        };
        if(FMath::Abs(Length(Seam.PanelA,A)-Seam.EaseRatio*Length(Seam.PanelB,B))>Seam.LengthToleranceMm+1.e-8)
            return Fail(TEXT("Seam lengths exceed the authored ease ratio and tolerance"));
        for(int32 I=0; I<A.Num(); ++I)
            Out.Stitches.Add(FIntVector2(Out.Offsets[Seam.PanelA]+A[I],Out.Offsets[Seam.PanelB]+B[Seam.bReverseB?B.Num()-1-I:I]));
    }
    TMap<int32,FVector3d> PinTargets;
    for(const auto& Pin : Input.Pins)
    {
        if(!Input.Panels.IsValidIndex(Pin.Panel) || !Input.Panels[Pin.Panel].BoundaryMm.IsValidIndex(Pin.BoundaryPoint))
            return Fail(TEXT("Pin refers to a missing authoring boundary point"));
        const int32 Vertex=Out.Offsets[Pin.Panel]+Pin.BoundaryPoint;
        const FVector3d Target=Pin.TargetCm.Get(Out.InitialCm[Vertex]);
        if(Target.ContainsNaN() || Target.GetAbsMax()>10000) return Fail(TEXT("Anchor target is non-finite or out of range"));
        if(const auto* Previous=PinTargets.Find(Vertex))
            if(!Previous->Equals(Target,1.e-8)) return Fail(TEXT("Anchor target conflicts with another guide for the same point"));
        if(Pin.TargetCm.IsSet()) for(const auto& C : Input.Colliders)
            if((Target-ClosestAxisPoint(Target,C)).Length()<C.RadiusCm+Input.ThicknessCm-0.005)
                return Fail(TEXT("Anchor target penetrates the body; move the target before fitting"));
        if(Pin.TargetCm.IsSet()) for(const auto& C : Input.ConvexColliders)
        {
            FVector3d Normal;
            if(C.SignedDistance(Target,Normal)<Input.ThicknessCm-0.005)
                return Fail(TEXT("Anchor target penetrates the body convex; move the target before fitting"));
        }
        PinTargets.Add(Vertex,Target); Out.PinnedVertices.Add(Vertex);
    }
    if(Out.PinnedVertices.IsEmpty()) return Fail(TEXT("At least one placement anchor is required"));
    // A closed seam is a shared degree of freedom, including transitive
    // junctions of three or more panels. Keep the original 2D topology intact.
    TArray<int32> Parent, ActiveRoot;
    TArray<TArray<int32>> SeamMembers;
    TArray<bool> Welded, Fixed;
    const int32 Count=Out.InitialCm.Num();
    Parent.SetNum(Count); ActiveRoot.SetNum(Count); SeamMembers.SetNum(Count);
    Welded.Init(false,Count); Fixed.Init(false,Count);
    for(int32 I=0;I<Count;++I) { Parent[I]=ActiveRoot[I]=I; Fixed[I]=Out.PinnedVertices.Contains(I); }
    auto Root=[&](int32 V) { while(Parent[V]!=V) { Parent[V]=Parent[Parent[V]]; V=Parent[V]; } return V; };
    for(auto S : Out.Stitches) { int32 A=Root(S.X), B=Root(S.Y); Parent[FMath::Max(A,B)]=FMath::Min(A,B); }
    for(int32 I=0;I<Count;++I) SeamMembers[Root(I)].Add(I);
    TMap<int32,FVector3d> SeamTargets;
    for(const auto& Pin : PinTargets)
    {
        const int32 R=Root(Pin.Key);
        if(const auto* Other=SeamTargets.Find(R))
            if(!Other->Equals(Pin.Value,1.e-8)) return Fail(TEXT("Anchors on the same sewn junction have conflicting targets"));
        SeamTargets.Add(R,Pin.Value);
    }
    for(auto P : Out.InitialCm)
        for(const auto& C : Input.Colliders)
            if ((P-ClosestAxisPoint(P,C)).Length()<C.RadiusCm-0.01) return Fail(TEXT("Initial placement penetrates the body; move the panel before fitting"));
    for(auto P : Out.InitialCm) for(const auto& C : Input.ConvexColliders)
    {
        FVector3d Normal;
        if(C.SignedDistance(P,Normal)<-0.01) return Fail(TEXT("Initial placement penetrates the body convex; move the panel before fitting"));
    }
    if(FacePenetration(Out,Out.InitialCm,Input.Colliders,Input.ConvexColliders)>0.01)
        return Fail(TEXT("Initial panel face penetrates the body; move or refine the panel before fitting"));
    if(!PackUV(Out,Error)) return false;
    Out.SettledCm=Out.InitialCm;
    TArray<FVector3d> Velocity;
    Velocity.Init(FVector3d::ZeroVector,Out.InitialCm.Num());
    TArray<double> InitialSeamLength;
    for(auto S : Out.Stitches) InitialSeamLength.Add((Out.InitialCm[S.X]-Out.InitialCm[S.Y]).Length());
    auto SetPosition=[&](int32 R,FVector3d P)
    {
        Out.SettledCm[R]=P;
        if(Welded[R]) for(int32 V : SeamMembers[R]) Out.SettledCm[V]=P;
    };
    auto ProjectDistance=[&](int32 A,int32 B,double Length,double Strength=1.0)
    {
        A=ActiveRoot[A]; B=ActiveRoot[B];
        if(A==B) return;
        double WA=Fixed[A]?0:1.0/(Welded[A]?SeamMembers[A].Num():1);
        double WB=Fixed[B]?0:1.0/(Welded[B]?SeamMembers[B].Num():1);
        if(WA+WB==0) return;
        FVector3d D=Out.SettledCm[B]-Out.SettledCm[A];
        double L=D.Length();
        if(L<1.e-12) return;
        FVector3d Delta=D*(Strength*(L-Length)/(L*(WA+WB)));
        SetPosition(A,Out.SettledCm[A]+WA*Delta); SetPosition(B,Out.SettledCm[B]-WB*Delta);
    };
    auto ProjectBending=[&](const FAngleConstraint& E)
    {
        if(Input.BendingStrength==0) return;
        const int32 Vertices[4]={E.A,E.B,E.C,E.D};
        const auto Sample=EvaluateDihedral(Out.SettledCm[E.A],Out.SettledCm[E.B],Out.SettledCm[E.C],Out.SettledCm[E.D]);
        const double Angle=Sample.Angle;
        int32 Roots[4],NumRoots=0; FVector3d Gradients[4]; double Weights[4];
        for(int32 K=0;K<4;++K)
        {
            const int32 R=ActiveRoot[Vertices[K]];
            int32 Index=0; while(Index<NumRoots && Roots[Index]!=R) ++Index;
            if(Index==NumRoots)
            {
                Roots[Index]=R; Gradients[Index]=FVector3d::ZeroVector;
                Weights[Index]=Fixed[R]?0:1.0/(Welded[R]?SeamMembers[R].Num():1); ++NumRoots;
            }
            Gradients[Index]+=Sample.Gradient[K];
        }
        double Denominator=0;
        for(int32 K=0;K<NumRoots;++K) Denominator+=Weights[K]*Gradients[K].SquaredLength();
        if(Denominator<1.e-12 || !FMath::IsFinite(Denominator) || !FMath::IsFinite(Angle)) return;
        const double Delta=Input.BendingStrength*FMath::Clamp(double(Angle),-UE_PI*.25,UE_PI*.25)/Denominator;
        for(int32 K=0;K<NumRoots;++K) SetPosition(Roots[K],Out.SettledCm[Roots[K]]-Gradients[K]*(Weights[K]*Delta));
    };
    // Surface contact samples share the original vertices: they do not alter
    // authored topology, skin correspondence, or the final continuous face test.
    struct FSurfaceSample { FIntVector Vertices; FVector3d Weights; };
    TArray<FSurfaceSample> SurfaceSamples;
    for(const auto& E : Structural) SurfaceSamples.Add({FIntVector(E.A,E.B,E.B),FVector3d(.5,.5,0)});
    for(int32 Panel=0;Panel<Out.Panels.Num();++Panel) for(const auto T : Out.Panels[Panel].Triangles)
    {
        const int32 O=Out.Offsets[Panel];
        SurfaceSamples.Add({FIntVector(O+T.X,O+T.Y,O+T.Z),FVector3d(1.0/3,1.0/3,1.0/3)});
    }
    const double SurfaceClearance=Input.ThicknessCm+1.e-5;
    auto SweepSurfaceSamples=[&](const TArray<FVector3d>& Previous)
    {
        for(const auto& Sample : SurfaceSamples)
        {
            const auto V=Sample.Vertices; const auto W=Sample.Weights;
            const FVector3d Before=Previous[V.X]*W.X+Previous[V.Y]*W.Y+Previous[V.Z]*W.Z;
            const FVector3d Current=Out.SettledCm[V.X]*W.X+Out.SettledCm[V.Y]*W.Y+Out.SettledCm[V.Z]*W.Z;
            const FVector3d Delta=SweepBodyCollision(Before,Current,Input.Colliders,Input.ThicknessCm,Input.ConvexColliders)-Current;
            if(Delta.SquaredLength()<1.e-20) continue;
            int32 Roots[3],NumRoots=0; double Barycentric[3]={},Mobility[3]={};
            for(int32 K=0;K<3;++K)
            {
                if(W[K]==0) continue;
                const int32 R=ActiveRoot[V[K]];
                int32 Index=0; while(Index<NumRoots && Roots[Index]!=R) ++Index;
                if(Index==NumRoots)
                {
                    Roots[Index]=R; Mobility[Index]=Fixed[R]?0:1.0/(Welded[R]?SeamMembers[R].Num():1); ++NumRoots;
                }
                Barycentric[Index]+=W[K];
            }
            double Denominator=0;
            for(int32 K=0;K<NumRoots;++K) Denominator+=Mobility[K]*FMath::Square(Barycentric[K]);
            if(Denominator<1.e-12) continue;
            for(int32 K=0;K<NumRoots;++K)
                SetPosition(Roots[K],Out.SettledCm[Roots[K]]+Delta*(Mobility[K]*Barycentric[K]/Denominator));
        }
    };
    auto VisitSurfaceContacts=[&](const TArray<FVector3d>& Positions,auto&& Visit)
    {
        for(const auto& Sample : SurfaceSamples)
        {
            const auto V=Sample.Vertices; const auto W=Sample.Weights;
            const FVector3d P=Positions[V.X]*W.X+Positions[V.Y]*W.Y+Positions[V.Z]*W.Z;
            auto Contact=[&](double Phi,FVector3d Normal)
            {
                if(Phi<SurfaceClearance && !Normal.ContainsNaN() && Normal.SquaredLength()>.5)
                    Visit(Sample,Phi-SurfaceClearance,Normal);
            };
            for(const auto& C : Input.Colliders)
            {
                const FVector3d D=P-ClosestAxisPoint(P,C);
                Contact(D.Length()-C.RadiusCm,D.GetSafeNormal());
            }
            for(const auto& C : Input.ConvexColliders)
            {
                if(!C.GetBounds().ExpandBy(SurfaceClearance).IsInsideOrOn(P)) continue;
                FVector3d Normal; const double Phi=C.SignedDistance(P,Normal); Contact(Phi,Normal);
            }
        }
    };
    auto RefineGlobally=[&](const TArray<FVector3d>& Previous,const TArray<FVector3d>& Prediction)
    {
        const double RefinementStart=FPlatformTime::Seconds();
        ON_SCOPE_EXIT { Out.RefinementSeconds+=FPlatformTime::Seconds()-RefinementStart; };
        ++Out.RefinementIterationsTotal;
        // Damped Gauss-Newton on active strain/contact inequalities. The sparse
        // normal system defaults to matrix-free PCG, with engine-bundled sparse
        // LDLT available for diagnostic comparison; no external process is required.
        // Gravity and continuous collision remain active. The refinement phase
        // is overdamped so Newton corrections are not fed back as inertial kicks.
        TArray<int32> Variable,Vertex;
        Variable.Init(INDEX_NONE,Count);
        for(int32 I=0;I<Count;++I) if(ActiveRoot[I]==I && !Fixed[I]) { Variable[I]=Vertex.Num(); Vertex.Add(I); }
        const int32 N=Vertex.Num(); if(N==0) return;
        const double InitialSweepStart=FPlatformTime::Seconds();
        for(int32 R : Vertex) SetPosition(R,SweepBodyCollision(Previous[R],Out.SettledCm[R],Input.Colliders,Input.ThicknessCm,Input.ConvexColliders));
        Out.CollisionSweepSeconds+=FPlatformTime::Seconds()-InitialSweepStart;
        TArray<FFitRow> Rows;
        for(const auto& E : Structural)
        {
            const int32 RA=ActiveRoot[E.A],RB=ActiveRoot[E.B];
            const int32 A=Variable[RA],B=Variable[RB];
            if(RA==RB || (A==INDEX_NONE && B==INDEX_NONE)) continue;
            const FVector3d D=Out.SettledCm[RB]-Out.SettledCm[RA]; const double L=D.Length();
            if(L<1.e-12) continue;
            const double Target=FMath::Clamp(L,E.Rest*.95,E.Rest*1.05);
            // Compare relative strain, as the acceptance metric does. Absolute
            // centimetre residuals underweight short boundary samples and let
            // them absorb the error of their much longer interior neighbours.
            if(FMath::Abs(L-Target)>1.e-8) Rows.Add({A,B,-D/(L*E.Rest),D/(L*E.Rest),(L-Target)/E.Rest});
        }
        const double AngularWeight=.1*FMath::Sqrt(Input.BendingStrength);
        if(AngularWeight>0) for(const auto& E : Bending)
        {
            const auto Sample=EvaluateDihedral(Out.SettledCm[E.A],Out.SettledCm[E.B],Out.SettledCm[E.C],Out.SettledCm[E.D]);
            const auto& G=Sample.Gradient; const double Angle=Sample.Angle;
            const double W=AngularWeight;
            FFitRow Row;
            Row.A=Variable[ActiveRoot[E.A]]; Row.B=Variable[ActiveRoot[E.B]]; Row.C=Variable[ActiveRoot[E.C]]; Row.D=Variable[ActiveRoot[E.D]];
            Row.GA=FVector3d(G[0])*W; Row.GB=FVector3d(G[1])*W; Row.GC=FVector3d(G[2])*W; Row.GD=FVector3d(G[3])*W; Row.Error=Angle*W;
            if(FMath::IsFinite(Row.Error) && !Row.GA.ContainsNaN() && !Row.GB.ContainsNaN() && !Row.GC.ContainsNaN() && !Row.GD.ContainsNaN()) Rows.Add(Row);
        }
        constexpr double ContactWeight=4, Regularization=.02;
        const double Clearance=Input.ThicknessCm+1.e-5;
        VisitSurfaceContacts(Out.SettledCm,[&](const FSurfaceSample& Sample,double Distance,FVector3d Normal)
        {
            FFitRow Row;
            Row.A=Variable[ActiveRoot[Sample.Vertices.X]]; Row.B=Variable[ActiveRoot[Sample.Vertices.Y]];
            Row.C=Sample.Weights.Z>0?Variable[ActiveRoot[Sample.Vertices.Z]]:INDEX_NONE;
            Row.GA=Normal*(ContactWeight*Sample.Weights.X); Row.GB=Normal*(ContactWeight*Sample.Weights.Y);
            Row.GC=Normal*(ContactWeight*Sample.Weights.Z); Row.Error=Distance*ContactWeight;
            if(Row.A!=INDEX_NONE || Row.B!=INDEX_NONE || Row.C!=INDEX_NONE) Rows.Add(Row);
        });
        for(int32 I=0;I<N;++I)
        {
            const FVector3d P=Out.SettledCm[Vertex[I]];
            auto AddContact=[&](double Phi,FVector3d Normal)
            {
                if(Phi<Clearance && !Normal.ContainsNaN() && Normal.SquaredLength()>.5)
                    Rows.Add({I,INDEX_NONE,Normal*ContactWeight,FVector3d::ZeroVector,(Phi-Clearance)*ContactWeight});
            };
            for(const auto& C : Input.Colliders)
            {
                const FVector3d D=P-ClosestAxisPoint(P,C);
                AddContact(D.Length()-C.RadiusCm,D.GetSafeNormal());
            }
            for(const auto& C : Input.ConvexColliders)
            {
                if(!C.GetBounds().ExpandBy(Clearance).IsInsideOrOn(P)) continue;
                FVector3d Normal; const double Phi=C.SignedDistance(P,Normal); AddContact(Phi,Normal);
            }
        }
        TArray<FVector3d> RHS,Delta,Residual,Z,Direction,Product;
        TArray<FFitBlock> Blocks;
        TArray<double> Damping;
        TArray<TArray<FVector3d,TInlineAllocator<3>>> ContactBasis;
        ContactBasis.SetNum(N);
        auto Tangent=[&](int32 I,FVector3d V)
        {
            if(I!=INDEX_NONE) for(auto Normal : ContactBasis[I]) V-=Normal*FVector3d::DotProduct(V,Normal);
            return V;
        };
        for(int32 I=0;I<N;++I)
        {
            const int32 R=Vertex[I]; const double D=Regularization*(Welded[R]?SeamMembers[R].Num():1);
            Damping.Add(D);
        }
        auto Dot=[&](const TArray<FVector3d>& A,const TArray<FVector3d>& B)
        { double Sum=0; for(int32 I=0;I<N;++I) Sum+=FVector3d::DotProduct(A[I],B[I]); return Sum; };
        auto Precondition=[&](const TArray<FVector3d>& R,TArray<FVector3d>& Result)
        {
            Result.SetNum(N);
            for(int32 I=0;I<N;++I) Result[I]=Tangent(I,Blocks[I].Solve(R[I]));
        };
        auto SolveLinear=[&]()
        {
        const double LinearStart=FPlatformTime::Seconds();
        ON_SCOPE_EXIT { Out.LinearSolveSeconds+=FPlatformTime::Seconds()-LinearStart; };
        TArray<FFitRow> WorkingRows=Rows;
        RHS.Init(FVector3d::ZeroVector,N); Delta=RHS; Product=RHS; Blocks.Reset();
        for(int32 I=0;I<N;++I)
        {
            Blocks.Add({FVector3d(Damping[I],Damping[I],Damping[I])});
            RHS[I]=-Tangent(I,Out.SettledCm[Vertex[I]]-Prediction[Vertex[I]])*Damping[I];
        }
        for(auto& Row : WorkingRows)
        {
            Row.GA=Tangent(Row.A,Row.GA); Row.GB=Tangent(Row.B,Row.GB);
            Row.GC=Tangent(Row.C,Row.GC); Row.GD=Tangent(Row.D,Row.GD);
            if(Row.A!=INDEX_NONE) { RHS[Row.A]-=Row.GA*Row.Error; Blocks[Row.A].Add(Row.GA); }
            if(Row.B!=INDEX_NONE) { RHS[Row.B]-=Row.GB*Row.Error; Blocks[Row.B].Add(Row.GB); }
            if(Row.C!=INDEX_NONE) { RHS[Row.C]-=Row.GC*Row.Error; Blocks[Row.C].Add(Row.GC); }
            if(Row.D!=INDEX_NONE) { RHS[Row.D]-=Row.GD*Row.Error; Blocks[Row.D].Add(Row.GD); }
        }
        // Optional exact linear solve for diagnosing iteration-cutoff effects.
        // Large source meshes keep the bounded matrix-free PCG path below.
        if(Input.bDirectLinearSolve && N<=4096)
        {
            TArray<Eigen::Triplet<double>> Entries;
            Entries.Reserve(N*3+WorkingRows.Num()*60);
            Eigen::VectorXd B(3*N);
            for(int32 I=0;I<N;++I) for(int32 K=0;K<3;++K)
            { Entries.Emplace(3*I+K,3*I+K,Damping[I]); B[3*I+K]=RHS[I][K]; }
            for(const auto& Row : WorkingRows)
            {
                const int32 Indices[4]={Row.A,Row.B,Row.C,Row.D};
                const FVector3d G[4]={Row.GA,Row.GB,Row.GC,Row.GD};
                int32 Columns[12],Size=0; double Values[12];
                for(int32 I=0;I<4;++I) if(Indices[I]!=INDEX_NONE) for(int32 K=0;K<3;++K)
                    if(FMath::Abs(G[I][K])>1.e-16) { Columns[Size]=3*Indices[I]+K; Values[Size++]=G[I][K]; }
                for(int32 I=0;I<Size;++I) for(int32 J=0;J<Size;++J)
                    Entries.Emplace(Columns[I],Columns[J],Values[I]*Values[J]);
            }
            Eigen::SparseMatrix<double> Matrix(3*N,3*N);
            Matrix.setFromTriplets(Entries.GetData(),Entries.GetData()+Entries.Num());
            Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> Solver;
            Solver.compute(Matrix);
            if(Solver.info()==Eigen::Success)
            {
                const Eigen::VectorXd X=Solver.solve(B);
                if(Solver.info()==Eigen::Success && X.allFinite())
                {
                    const double Absolute=(Matrix*X-B).norm();
                    const double Relative=Absolute/FMath::Max(B.norm(),1.e-15);
                    if(Relative<1.e-7 || Absolute<1.e-8)
                    {
                        for(int32 I=0;I<N;++I) Delta[I]=Tangent(I,FVector3d(X[3*I],X[3*I+1],X[3*I+2]));
                        Out.LastLinearRelativeResidual=Relative; ++Out.DirectLinearSolves; return;
                    }
                }
            }
        }
        ++Out.IterativeLinearSolves;
        Residual=RHS; Precondition(Residual,Z); Direction=Z; double RZ=Dot(Residual,Z);
        for(int32 Iter=0;Iter<Input.LinearIterations && RZ>1.e-24;++Iter)
        {
            for(int32 I=0;I<N;++I) Product[I]=Direction[I]*Damping[I];
            for(const auto& Row : WorkingRows)
            {
                double V=0;
                if(Row.A!=INDEX_NONE) V+=FVector3d::DotProduct(Row.GA,Direction[Row.A]);
                if(Row.B!=INDEX_NONE) V+=FVector3d::DotProduct(Row.GB,Direction[Row.B]);
                if(Row.C!=INDEX_NONE) V+=FVector3d::DotProduct(Row.GC,Direction[Row.C]);
                if(Row.D!=INDEX_NONE) V+=FVector3d::DotProduct(Row.GD,Direction[Row.D]);
                if(Row.A!=INDEX_NONE) Product[Row.A]+=Row.GA*V;
                if(Row.B!=INDEX_NONE) Product[Row.B]+=Row.GB*V;
                if(Row.C!=INDEX_NONE) Product[Row.C]+=Row.GC*V;
                if(Row.D!=INDEX_NONE) Product[Row.D]+=Row.GD*V;
            }
            const double Denominator=Dot(Direction,Product);
            if(Denominator<1.e-24 || !FMath::IsFinite(Denominator)) break;
            const double Alpha=RZ/Denominator;
            for(int32 I=0;I<N;++I) { Delta[I]+=Direction[I]*Alpha; Residual[I]-=Product[I]*Alpha; }
            if(Dot(Residual,Residual)<1.e-16) break;
            Precondition(Residual,Z); const double Next=Dot(Residual,Z);
            for(int32 I=0;I<N;++I) Direction[I]=Z[I]+Direction[I]*(Next/RZ);
            RZ=Next;
        }
        Out.LastLinearRelativeResidual=FMath::Sqrt(Dot(Residual,Residual)/FMath::Max(Dot(RHS,RHS),1.e-30));
        };
        SolveLinear();
        // Solve the contact tangent space before applying a coupled correction.
        // Clipping an unconstrained Newton step afterwards destroys its coupling
        // to neighbouring vertices. Only inward directions activate a contact;
        // the basis is rebuilt each nonlinear iteration so contact can release.
        for(int32 ActiveSet=0;ActiveSet<3;++ActiveSet)
        {
            bool Added=false;
            for(int32 I=0;I<N;++I)
            {
                const auto P=Out.SettledCm[Vertex[I]];
                auto Activate=[&](double Phi,FVector3d Normal)
                {
                    if(Phi>Clearance+.001 || FVector3d::DotProduct(Delta[I],Normal)>=-1.e-9) return;
                    Normal=Tangent(I,Normal);
                    if(Normal.SquaredLength()<1.e-6 || ContactBasis[I].Num()>=3) return;
                    ContactBasis[I].Add(Normal.GetSafeNormal()); Added=true;
                };
                for(const auto& C : Input.Colliders)
                {
                    const auto D=P-ClosestAxisPoint(P,C); Activate(D.Length()-C.RadiusCm,D.GetSafeNormal());
                }
                for(const auto& C : Input.ConvexColliders)
                {
                    if(!C.GetBounds().ExpandBy(Clearance+.001).IsInsideOrOn(P)) continue;
                    FVector3d Normal; const double Phi=C.SignedDistance(P,Normal); Activate(Phi,Normal);
                }
            }
            if(!Added) break;
            SolveLinear();
        }
        auto Energy=[&](const TArray<FVector3d>& Positions)
        {
            const double EnergyStart=FPlatformTime::Seconds();
            ON_SCOPE_EXIT { Out.EnergyEvaluationSeconds+=FPlatformTime::Seconds()-EnergyStart; };
            double Value=0;
            VisitSurfaceContacts(Positions,[&](const FSurfaceSample&,double Distance,FVector3d)
            { Value+=FMath::Square(ContactWeight*Distance); });
            for(const auto& E : Structural)
            {
                const double L=(Positions[E.A]-Positions[E.B]).Length();
                Value+=FMath::Square((L-FMath::Clamp(L,E.Rest*.95,E.Rest*1.05))/E.Rest);
            }
            if(AngularWeight>0) for(const auto& E : Bending)
            {
                const double Angle=EvaluateDihedral(Positions[E.A],Positions[E.B],Positions[E.C],Positions[E.D]).Angle;
                Value+=FMath::Square(AngularWeight*Angle);
            }
            for(int32 I=0;I<N;++I)
            {
                const int32 R=Vertex[I]; const auto P=Positions[R];
                Value+=Damping[I]*(P-Prediction[R]).SquaredLength();
                for(const auto& C : Input.Colliders)
                    Value+=FMath::Square(ContactWeight*FMath::Min(0.0,(P-ClosestAxisPoint(P,C)).Length()-C.RadiusCm-Clearance));
                for(const auto& C : Input.ConvexColliders)
                {
                    if(!C.GetBounds().ExpandBy(Clearance).IsInsideOrOn(P)) continue;
                    FVector3d Normal; Value+=FMath::Square(ContactWeight*FMath::Min(0.0,C.SignedDistance(P,Normal)-Clearance));
                }
            }
            return Value;
        };
        const double BeforeEnergy=Energy(Out.SettledCm);
        // Active bounds/normals can change during a Newton step. Backtracking
        // uses the actual swept result and nonlinear residual, preventing a
        // two-cycle between different contact/strain active sets.
        for(int32 Attempt=0;Attempt<2;++Attempt)
        {
            // Contact projection can turn a Newton direction into an ascent
            // direction. Fall back to the diagonally scaled negative gradient
            // instead of getting stuck with a nonzero constraint residual.
            if(Attempt==1) Precondition(RHS,Delta);
            double Maximum=0; for(auto D : Delta) Maximum=FMath::Max(Maximum,D.Length());
            const double Scale=FMath::Min(1.0,.03/FMath::Max(Maximum,1.e-12));
            for(int32 Search=0;Search<16;++Search)
            {
                ++Out.LineSearchTrials;
                TArray<FVector3d> Candidate=Out.SettledCm;
                const double TrialScale=Scale*FMath::Pow(.5,Search);
                const double SweepStart=FPlatformTime::Seconds();
                for(int32 I=0;I<N;++I)
                {
                    const int32 R=Vertex[I];
                    const auto P=SweepBodyCollision(Out.SettledCm[R],Out.SettledCm[R]+Delta[I]*TrialScale,Input.Colliders,Input.ThicknessCm,Input.ConvexColliders);
                    Candidate[R]=P; if(Welded[R]) for(int32 V : SeamMembers[R]) Candidate[V]=P;
                }
                Out.CollisionSweepSeconds+=FPlatformTime::Seconds()-SweepStart;
                if(Energy(Candidate)<BeforeEnergy) { Out.SettledCm=MoveTemp(Candidate); return; }
            }
        }
    };
    int32 StableFrames=0;
    for(int32 Step=0; Step<Input.MaxSteps; ++Step)
    {
        for(int32 R=0;R<Count;++R)
        {
            if(Welded[R] || SeamMembers[R].Num()<2) continue;
            if(Step<60)
            {
                bool InitiallyClosed=true;
                for(int32 V : SeamMembers[R]) if(!Out.InitialCm[V].Equals(Out.InitialCm[R],1.e-10)) { InitiallyClosed=false; break; }
                if(!InitiallyClosed) continue;
            }
            FVector3d Center=FVector3d::ZeroVector, MeanVelocity=FVector3d::ZeroVector;
            for(int32 V : SeamMembers[R]) { Center+=Out.SettledCm[V]; MeanVelocity+=Velocity[V]; }
            Center/=SeamMembers[R].Num(); MeanVelocity/=SeamMembers[R].Num();
            if(const auto* Target=SeamTargets.Find(R)) Center=FMath::Lerp(Out.InitialCm[R],*Target,FMath::Min(Step/60.0,1.0));
            bool Safe=true;
            for(int32 V : SeamMembers[R])
                if((Out.SettledCm[V]-Center).Length()>.02
                    || !SweepBodyCollision(Out.SettledCm[V],Center,Input.Colliders,Input.ThicknessCm,Input.ConvexColliders).Equals(Center,1.e-7))
                { Safe=false; break; }
            // Never weld through a body or silently close an unfinished seam.
            if(!Safe) continue;
            Welded[R]=true; Fixed[R]=SeamTargets.Contains(R);
            for(int32 V : SeamMembers[R]) { ActiveRoot[V]=R; Velocity[V]=Fixed[R]?FVector3d::ZeroVector:MeanVelocity; }
            SetPosition(R,Center);
        }
        TArray<FVector3d> Previous=Out.SettledCm;
        TArray<FVector3d> ContactPrevious=Previous;
        bool GlobalFit=Step>=120;
        if(GlobalFit) for(auto S : Out.Stitches) if(ActiveRoot[S.X]!=ActiveRoot[S.Y]) { GlobalFit=false; break; }
        for(int32 I=0; I<Velocity.Num(); ++I)
        {
            if(ActiveRoot[I]!=I || Fixed[I]) continue;
            Velocity[I]*=GlobalFit?0.0:0.9;
            Velocity[I].Z+=Input.GravityCm*Input.TimeStep;
            SetPosition(I,Out.SettledCm[I]+Velocity[I]*Input.TimeStep);
        }
        const double Closing=FMath::Clamp((Step+1)/60.0,0.0,1.0);
        for(const auto& Target : PinTargets) SetPosition(ActiveRoot[Target.Key],FMath::Lerp(Out.InitialCm[Target.Key],Target.Value,Closing));
        if(GlobalFit)
        {
            const TArray<FVector3d> Prediction=Out.SettledCm;
            // The gravity prediction belongs in the proximal objective, not in
            // the initial feasible iterate. A projected prediction can increase
            // elastic energy before line search starts; an incomplete nonlinear
            // solve then repeats that impulse and can chatter indefinitely.
            // Starting from the previous feasible state makes every accepted
            // correction decrease the same gravity-inclusive objective. Gravity
            // is still present through (Position - Prediction) in the RHS/energy.
            Out.SettledCm=Previous;
            Out.LastNonlinearIterations=0;
            // A single linearization leaves too much of the gravity prediction
            // unresolved on a garment-sized graph. Rebuild the active set and
            // nonlinear derivatives while keeping gravity integration once per
            // time step, just as in the sequential projection phase.
            for(int32 Nonlinear=0;Nonlinear<Input.RefinementIterations;++Nonlinear)
            {
                const TArray<FVector3d> Before=Out.SettledCm;
                RefineGlobally(ContactPrevious,Prediction);
                ++Out.LastNonlinearIterations;
                ContactPrevious=Out.SettledCm;
                double Movement=0;
                for(int32 I=0;I<Count;++I) Movement=FMath::Max(Movement,(Out.SettledCm[I]-Before[I]).SquaredLength());
                Out.LastRefinementCorrectionCm=FMath::Sqrt(Movement);
                if(Movement<1.e-12) break;
            }
            ++Out.RefinementSteps;
        }
        else for(int32 Iter=0; Iter<Input.Iterations; ++Iter)
        {
            // Bending changes the angle between faces, not in-plane distances.
            // Rest panels are flat; authored seams are not bending constraints.
            for(const auto& E : Bending) ProjectBending(E);
            // Alternate sweep direction to reduce systematic directional bias.
            for(int32 J=0; J<Structural.Num(); ++J)
            {
                const auto& E=Structural[(Iter&1)?Structural.Num()-1-J:J];
                // After sewing, solve a bounded rest-metric feasibility problem.
                // Exact-length projections otherwise concentrate unavoidable ease
                // next to pins while repeatedly undoing neighbouring corrections.
                // The 5% band retains margin below the independent 10% acceptance
                // limit; source rest lengths and final measurements never change.
                const double Current=(Out.SettledCm[E.A]-Out.SettledCm[E.B]).Length();
                const double Target=Closing<1?E.Rest:FMath::Clamp(Current,E.Rest*.95,E.Rest*1.05);
                ProjectDistance(E.A,E.B,Target);
            }
            for(int32 J=0; J<Out.Stitches.Num(); ++J)
                ProjectDistance(Out.Stitches[J].X,Out.Stitches[J].Y,InitialSeamLength[J]*(1-Closing));
            SweepSurfaceSamples(ContactPrevious);
            for(int32 I=0; I<Out.SettledCm.Num(); ++I)
            {
                if(ActiveRoot[I]!=I || Fixed[I]) continue;
                SetPosition(I,SweepBodyCollision(ContactPrevious[I],Out.SettledCm[I],Input.Colliders,Input.ThicknessCm,Input.ConvexColliders));
            }
            // Include welded members: surface samples reference original indices.
            ContactPrevious=Out.SettledCm;
        }
        Out.MaxSpeedCmPerSec=0;
        Out.LastStepDisplacementCm.SetNum(Velocity.Num());
        for(int32 I=0; I<Velocity.Num(); ++I)
        {
            Out.LastStepDisplacementCm[I]=Out.SettledCm[I]-Previous[I];
            Velocity[I]=Out.LastStepDisplacementCm[I]/Input.TimeStep;
            if(Velocity[I].ContainsNaN() || Velocity[I].SquaredLength()>1.e12) return Fail(TEXT("Fitting diverged; revise placement and seams"));
            Out.MaxSpeedCmPerSec=FMath::Max(Out.MaxSpeedCmPerSec,Velocity[I].Length());
        }
        Out.MaxSeamGapCm=0; Out.MaxPenetrationCm=0; Out.MaxRelativeEdgeError=0;
        for(auto S : Out.Stitches) Out.MaxSeamGapCm=FMath::Max(Out.MaxSeamGapCm,(Out.SettledCm[S.X]-Out.SettledCm[S.Y]).Length());
        for(const auto& E : Structural) Out.MaxRelativeEdgeError=FMath::Max(Out.MaxRelativeEdgeError,FMath::Abs((Out.SettledCm[E.A]-Out.SettledCm[E.B]).Length()/E.Rest-1));
        for(auto P : Out.SettledCm) for(const auto& C : Input.Colliders)
            Out.MaxPenetrationCm=FMath::Max(Out.MaxPenetrationCm,C.RadiusCm+Input.ThicknessCm-(P-ClosestAxisPoint(P,C)).Length());
        for(auto P : Out.SettledCm) for(const auto& C : Input.ConvexColliders)
        {
            if(!C.GetBounds().ExpandBy(Input.ThicknessCm).IsInsideOrOn(P)) continue;
            FVector3d Normal;
            Out.MaxPenetrationCm=FMath::Max(Out.MaxPenetrationCm,Input.ThicknessCm-C.SignedDistance(P,Normal));
        }
        Out.Steps=Step+1;
        if(Input.Progress && (Out.Steps%30==0 || Out.Steps==Input.MaxSteps)) Input.Progress(Out);
        if (Step>=60 && Out.MaxSpeedCmPerSec<0.05 && Out.MaxSeamGapCm<0.02 && Out.MaxRelativeEdgeError<0.1 && Out.MaxPenetrationCm<0.005) ++StableFrames;
        else StableFrames=0;
        if(StableFrames>=30)
        {
            // Vertex rest alone cannot finish while a face still cuts the body.
            if(FacePenetration(Out,Out.SettledCm,Input.Colliders,Input.ConvexColliders)<=.005)
            { Out.bConverged=true; break; }
            StableFrames=0;
        }
    }
    // Failed fits also need real surface diagnostics; zero must not mean unmeasured.
    MeasurePanelSurface(Out);
    Out.MaxFacePenetrationCm=FacePenetration(Out,Out.SettledCm,Input.Colliders,Input.ConvexColliders);
    Out.bSurfaceMeasured=true;
    if(!Out.bConverged)
    {
        Error=FString::Printf(TEXT("Fitting did not converge after %d steps: speed %.5f cm/s, seam %.5f cm, edge error %.3f, penetration %.5f cm"),Out.Steps,Out.MaxSpeedCmPerSec,Out.MaxSeamGapCm,Out.MaxRelativeEdgeError,Out.MaxPenetrationCm);
        double Worst=0;
        for(const auto& E : Structural)
        {
            double Relative=FMath::Abs((Out.SettledCm[E.A]-Out.SettledCm[E.B]).Length()/E.Rest-1);
            if(Relative>Worst) Worst=Relative;
        }
        for(const auto& E : Structural)
            if(FMath::IsNearlyEqual(FMath::Abs((Out.SettledCm[E.A]-Out.SettledCm[E.B]).Length()/E.Rest-1),Worst,1.e-8))
            { Error+=FString::Printf(TEXT("; worst edge %d-%d rest %.4fcm initial[%s; %s] settled[%s; %s]"),E.A,E.B,E.Rest,*Out.InitialCm[E.A].ToString(),*Out.InitialCm[E.B].ToString(),*Out.SettledCm[E.A].ToString(),*Out.SettledCm[E.B].ToString()); break; }
        return false;
    }
    if(Out.MaxFacePenetrationCm>0.005 || Out.MaxInternalFoldDegrees>120 || Out.MinAreaRatio<0.5 || Out.MaxAreaRatio>1.5)
    {
        Out.bConverged=false;
        Error=FString::Printf(TEXT("Settled surface rejected: face penetration %.5fcm, internal fold %.3f degrees, area ratio %.3f..%.3f"),Out.MaxFacePenetrationCm,Out.MaxInternalFoldDegrees,Out.MinAreaRatio,Out.MaxAreaRatio);
        return false;
    }
    return true;
}

TSharedPtr<FManagedArrayCollection> MakeClothCollection(const FBuildResult& Build, UMaterialInterface* Material, FString& Error)
{
    using namespace UE::Chaos::ClothAsset;
    Error.Reset();
    if(!Build.bConverged || Build.SettledCm.IsEmpty() || Build.UV.Num()!=Build.SettledCm.Num() || !Material)
    { Error=TEXT("A converged build and explicit material are required"); return nullptr; }
    auto Invalid=[&](const TCHAR* Reason) -> TSharedPtr<FManagedArrayCollection> { Error=Reason; return nullptr; };
    const int32 NumVertices=Build.SettledCm.Num();
    if(Build.Panels.IsEmpty() || Build.Panels.Num()>32 || Build.Offsets.Num()!=Build.Panels.Num()
        || NumVertices>65536 || Build.InitialCm.Num()!=NumVertices || Build.PinnedVertices.IsEmpty())
        return Invalid(TEXT("Incomplete or oversized build topology"));
    int32 ExpectedOffset=0;
    for(int32 I=0; I<Build.Panels.Num(); ++I)
    {
        const auto& Panel=Build.Panels[I];
        if(Build.Offsets[I]!=ExpectedOffset || Panel.RestMm.Num()<3 || Panel.InitialCm.Num()!=Panel.RestMm.Num()
            || Panel.Triangles.IsEmpty() || Panel.Triangles.Num()>32768 || Panel.RestMm.Num()>NumVertices-ExpectedOffset)
            return Invalid(TEXT("Panel offsets or topology are inconsistent"));
        for(auto P : Panel.RestMm)
            if(P.ContainsNaN() || FMath::Abs(P.X)>10000 || FMath::Abs(P.Y)>10000)
                return Invalid(TEXT("Panel rest coordinate is outside the supported range"));
        for(auto T : Panel.Triangles)
        {
            if(!Panel.RestMm.IsValidIndex(T.X) || !Panel.RestMm.IsValidIndex(T.Y) || !Panel.RestMm.IsValidIndex(T.Z))
                return Invalid(TEXT("Panel triangle references a missing vertex"));
            const FVector2d A=Panel.RestMm[T.Y]-Panel.RestMm[T.X], B=Panel.RestMm[T.Z]-Panel.RestMm[T.X];
            if(A.X*B.Y-A.Y*B.X<=1.e-10) return Invalid(TEXT("Panel rest triangle is degenerate or reversed"));
        }
        ExpectedOffset+=Panel.RestMm.Num();
    }
    if(ExpectedOffset!=NumVertices) return Invalid(TEXT("Panel vertices do not cover the build"));
    for(int32 I=0; I<NumVertices; ++I)
        if(Build.SettledCm[I].ContainsNaN() || Build.SettledCm[I].GetAbsMax()>10000
            || FVector3f(Build.SettledCm[I]).ContainsNaN() || Build.InitialCm[I].ContainsNaN()
            || Build.InitialCm[I].GetAbsMax()>10000 || Build.UV[I].ContainsNaN()
            || Build.UV[I].X<0 || Build.UV[I].X>1 || Build.UV[I].Y<0 || Build.UV[I].Y>1)
            return Invalid(TEXT("Build positions or UVs are outside the supported range"));
    for(auto Stitch : Build.Stitches)
        if(!Build.SettledCm.IsValidIndex(Stitch.X) || !Build.SettledCm.IsValidIndex(Stitch.Y) || Stitch.X==Stitch.Y)
            return Invalid(TEXT("Stitch references invalid vertices"));
    for(int32 Pin : Build.PinnedVertices)
        if(!Build.SettledCm.IsValidIndex(Pin)) return Invalid(TEXT("Anchor references a missing vertex"));
    if(!Build.MaxDistancesCm.IsEmpty())
    {
        if(Build.MaxDistancesCm.Num()!=NumVertices) return Invalid(TEXT("Incomplete manual MaxDistance map"));
        for(float Value : Build.MaxDistancesCm)
            if(!FMath::IsFinite(Value) || Value<0 || Value>100) return Invalid(TEXT("Manual MaxDistance is outside 0..100cm"));
        for(auto Stitch : Build.Stitches)
            if(Build.MaxDistancesCm[Stitch.X]!=Build.MaxDistancesCm[Stitch.Y]) return Invalid(TEXT("Welded MaxDistance values conflict"));
        for(int32 Pin : Build.PinnedVertices)
            if(Build.MaxDistancesCm[Pin]!=0) return Invalid(TEXT("Manual MaxDistance cannot release an authored anchor"));
    }
    const bool bBodySkin=!Build.ReferenceMesh.IsNull();
    if(bBodySkin)
    {
        if(!Build.ReferenceMesh.ToString().StartsWith(TEXT("/LaceGarment/")) || Build.SkinJoints.Num()!=NumVertices || Build.SkinWeights.Num()!=NumVertices)
            return Invalid(TEXT("Incomplete independent body skin data"));
        auto* Body=Cast<USkeletalMesh>(Build.ReferenceMesh.TryLoad());
        if(!Body) return Invalid(TEXT("Reference body is missing"));
        for(int32 I=0; I<NumVertices; ++I)
        {
            if(Build.SkinJoints[I].Num()!=Build.SkinWeights[I].Num() || Build.SkinJoints[I].IsEmpty() || Build.SkinJoints[I].Num()>4)
                return Invalid(TEXT("Skin requires one-to-four matching joints and weights"));
            float Sum=0;
            for(int32 K=0; K<Build.SkinJoints[I].Num(); ++K)
            {
                if(!Body->GetRefSkeleton().IsValidIndex(Build.SkinJoints[I][K]) || !FMath::IsFinite(Build.SkinWeights[I][K]) || Build.SkinWeights[I][K]<=0)
                    return Invalid(TEXT("Invalid skin influence"));
                Sum+=Build.SkinWeights[I][K];
            }
            if(!FMath::IsNearlyEqual(Sum,1.f,1.e-5f)) return Invalid(TEXT("Skin weights must be normalized"));
        }
        for(auto Stitch : Build.Stitches)
            if(Build.SkinJoints[Stitch.X]!=Build.SkinJoints[Stitch.Y] || Build.SkinWeights[Stitch.X]!=Build.SkinWeights[Stitch.Y])
                return Invalid(TEXT("Stitched vertices have conflicting skin weights; regenerate the transfer"));
    }
    else if(!Build.SkinJoints.IsEmpty() || !Build.SkinWeights.IsEmpty()) return Invalid(TEXT("Skin weights require an explicit reference body"));
    TSharedRef<FManagedArrayCollection> Collection=MakeShared<FManagedArrayCollection>();
    FCollectionClothFacade Cloth(Collection);
    Cloth.DefineSchema();
    for(int32 I=0; I<Build.Panels.Num(); ++I)
    {
        const auto& Panel=Build.Panels[I];
        TArray<FVector2f> RestCm;
        TArray<FVector3f> Settled;
        for(int32 V=0; V<Panel.RestMm.Num(); ++V)
        {
            RestCm.Add(FVector2f(Panel.RestMm[V]/10));
            Settled.Add(FVector3f(Build.SettledCm[Build.Offsets[I]+V]));
        }
        Cloth.AddGetSimPattern().Initialize(RestCm,Settled,Panel.Triangles);
    }
    if(!Build.Stitches.IsEmpty()) Cloth.AddGetSeam().Initialize(Build.Stitches);
    FClothGeometryTools::BindMeshToRootBone(Collection,true,false);
    FClothGeometryTools::CopySimMeshToRenderMesh(Collection,FSoftObjectPath(Material),false);
    if(bBodySkin)
    {
        Cloth.SetSkeletalMeshSoftObjectPathName(Build.ReferenceMesh);
        const auto VertexLookup=Cloth.GetSimVertex3DLookup();
        for(int32 I=0; I<NumVertices; ++I)
        {
            Cloth.GetSimBoneIndices()[VertexLookup[I]]=Build.SkinJoints[I];
            Cloth.GetSimBoneWeights()[VertexLookup[I]]=Build.SkinWeights[I];
            Cloth.GetRenderBoneIndices()[I]=Build.SkinJoints[I];
            Cloth.GetRenderBoneWeights()[I]=Build.SkinWeights[I];
        }
    }
    for(int32 I=0; I<Build.UV.Num(); ++I) Cloth.GetRenderUVs()[I]={Build.UV[I]};
    Cloth.AddWeightMap(FName(TEXT("MaxDistance")));
    auto Map=Cloth.GetWeightMap(FName(TEXT("MaxDistance")));
    for(float& V : Map) V=1;
    const auto Lookup=Cloth.GetSimVertex3DLookup();
    if(!Build.MaxDistancesCm.IsEmpty())
        for(int32 I=0;I<NumVertices;++I) Map[Lookup[I]]=Build.MaxDistancesCm[I];
    for(int32 Pin : Build.PinnedVertices) Map[Lookup[Pin]]=0;
    // The runtime property alone does not create attachment data. Generate it
    // after welding and final MaxDistance corrections, using the native mesh
    // geodesics so cloth can bend while resisting accumulated stretch at pins.
    FClothEngineTools::GenerateTethers(Collection,TEXT("MaxDistance"),true);
    // Copy canonical UE properties once into an empty property group. CopyTo is
    // not an overwrite operation for groups of a different size.
    UChaosClothConfig* Config=NewObject<UChaosClothConfig>();
    Config->CollisionThickness=0.03f; Config->DampingCoefficient=0.2f; Config->FrictionCoefficient=0.4f;
    Config->MinPerParticleMass=0.00001f;
    Config->EdgeStiffnessWeighted.Low=1; Config->EdgeStiffnessWeighted.High=1;
    UChaosClothSharedSimConfig* Shared=NewObject<UChaosClothSharedSimConfig>();
    Shared->IterationCount=20; Shared->MaxIterationCount=40; Shared->SubdivisionCount=8;
    Chaos::FClothingSimulationConfig RuntimeConfig;
    RuntimeConfig.Initialize(Config,Shared);
    RuntimeConfig.GetPropertyCollection(0)->CopyTo(&Collection.Get());
    return Collection;
}

UChaosClothAsset* SaveClothAsset(const FBuildResult& Build, UMaterialInterface* Material, const FString& PackageName, FString& Error)
{
    if(!PackageName.StartsWith(TEXT("/LaceGarment/")) || !FPackageName::IsValidLongPackageName(PackageName)
        || !Material || !Material->GetPathName().StartsWith(TEXT("/LaceGarment/")))
    { Error=TEXT("Probe asset and material must belong to the independent LaceGarment plugin"); return nullptr; }
    auto Collection=MakeClothCollection(Build,Material,Error);
    if(!Collection) return nullptr;
    UPackage* Package=CreatePackage(*PackageName);
    Package->FullyLoad();
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FString AssetName=FPackageName::GetLongPackageAssetName(PackageName);
    UChaosClothAsset* Asset=FindObject<UChaosClothAsset>(Package,*AssetName);
    if(!Asset) Asset=NewObject<UChaosClothAsset>(Package,*AssetName,RF_Public|RF_Standalone);
    FText BuildError,Verbose;
    Asset->Build({Collection.ToSharedRef()},nullptr,&BuildError,&Verbose);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(!BuildError.IsEmpty() || !Asset->HasValidClothSimulationModels())
    { Error=BuildError.ToString()+TEXT(" ")+Verbose.ToString(); return nullptr; }
    FSavePackageArgs Args;
    Args.TopLevelFlags=RF_Public|RF_Standalone;
    Args.SaveFlags=SAVE_NoError;
    const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
    if(!UPackage::SavePackage(Package,Asset,*Filename,Args)) { Error=TEXT("Failed to save Cloth Asset"); return nullptr; }
    FAssetRegistryModule::AssetCreated(Asset);
    return Asset;
}
}
