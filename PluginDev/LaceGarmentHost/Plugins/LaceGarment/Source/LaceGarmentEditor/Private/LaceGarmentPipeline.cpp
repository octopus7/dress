#include "LaceGarmentPipeline.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Distance/DistSegment3Triangle3.h"
#include "LaceGarmentCollisionProjection.h"

namespace LaceGarment
{
namespace
{
struct FDistanceConstraint { int32 A,B; double Rest; };
FVector3d ClosestAxisPoint(FVector3d P, const FCapsule& C)
{
    FVector3d Axis=C.B-C.A;
    return C.A + Axis*FMath::Clamp(FVector3d::DotProduct(P-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
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
    TArray<FDistanceConstraint> Structural, Bending;
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
                    Bending.Add({Offset+*Other,Offset+Opposite,(Mesh.RestMm[*Other]-Mesh.RestMm[Opposite]).Length()/10});
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
    auto ProjectDistance=[&](int32 A,int32 B,double Length,double Strength=1.0)
    {
        double WA=Out.PinnedVertices.Contains(A)?0:1, WB=Out.PinnedVertices.Contains(B)?0:1;
        if(WA+WB==0) return;
        FVector3d D=Out.SettledCm[B]-Out.SettledCm[A];
        double L=D.Length();
        if(L<1.e-12) return;
        FVector3d Delta=D*(Strength*(L-Length)/(L*(WA+WB)));
        Out.SettledCm[A]+=WA*Delta; Out.SettledCm[B]-=WB*Delta;
    };
    int32 StableFrames=0;
    for(int32 Step=0; Step<Input.MaxSteps; ++Step)
    {
        TArray<FVector3d> Previous=Out.SettledCm;
        TArray<FVector3d> ContactPrevious=Previous;
        for(int32 I=0; I<Velocity.Num(); ++I)
        {
            if(Out.PinnedVertices.Contains(I)) continue;
            Velocity[I]*=0.9;
            Velocity[I].Z+=Input.GravityCm*Input.TimeStep;
            Out.SettledCm[I]+=Velocity[I]*Input.TimeStep;
        }
        const double Closing=FMath::Clamp((Step+1)/60.0,0.0,1.0);
        for(const auto& Target : PinTargets) Out.SettledCm[Target.Key]=FMath::Lerp(Out.InitialCm[Target.Key],Target.Value,Closing);
        for(int32 Iter=0; Iter<Input.Iterations; ++Iter)
        {
            // Weak opposite-vertex springs regularize panel bending while
            // permitting a curved drape. They do not bridge authored seams.
            for(const auto& E : Bending) ProjectDistance(E.A,E.B,E.Rest,Input.BendingStrength);
            // Alternate sweep direction to reduce systematic directional bias.
            for(int32 J=0; J<Structural.Num(); ++J)
            {
                const auto& E=Structural[(Iter&1)?Structural.Num()-1-J:J];
                ProjectDistance(E.A,E.B,E.Rest);
            }
            for(int32 J=0; J<Out.Stitches.Num(); ++J)
                ProjectDistance(Out.Stitches[J].X,Out.Stitches[J].Y,InitialSeamLength[J]*(1-Closing));
            for(int32 I=0; I<Out.SettledCm.Num(); ++I)
            {
                if(Out.PinnedVertices.Contains(I)) continue;
                Out.SettledCm[I]=SweepBodyCollision(ContactPrevious[I],Out.SettledCm[I],Input.Colliders,Input.ThicknessCm,Input.ConvexColliders);
                ContactPrevious[I]=Out.SettledCm[I];
            }
        }
        Out.MaxSpeedCmPerSec=0;
        for(int32 I=0; I<Velocity.Num(); ++I)
        {
            Velocity[I]=(Out.SettledCm[I]-Previous[I])/Input.TimeStep;
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
        if(Input.Progress && (Out.Steps%120==0 || Out.Steps==Input.MaxSteps)) Input.Progress(Out);
        if (Step>=60 && Out.MaxSpeedCmPerSec<0.05 && Out.MaxSeamGapCm<0.02 && Out.MaxRelativeEdgeError<0.1 && Out.MaxPenetrationCm<0.005) ++StableFrames;
        else StableFrames=0;
        if(StableFrames>=30) { Out.bConverged=true; break; }
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
