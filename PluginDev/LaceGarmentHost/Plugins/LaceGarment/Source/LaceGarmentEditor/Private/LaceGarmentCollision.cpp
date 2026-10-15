#include "LaceGarmentPipeline.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "AssetCompilingManager.h"
#include "LaceGarmentCollisionProjection.h"
namespace LaceGarment
{
namespace
{
FVector3d AxisPoint(FVector3d P,const FCapsule& C)
{
    const auto D=C.B-C.A;
    return C.A+D*FMath::Clamp(FVector3d::DotProduct(P-C.A,D)/FMath::Max(D.SquaredLength(),1.e-12),0.0,1.0);
}
bool InBounds(FVector3d P,const FCapsule& C,double R)
{
    return P.X>=FMath::Min(C.A.X,C.B.X)-R && P.X<=FMath::Max(C.A.X,C.B.X)+R
        && P.Y>=FMath::Min(C.A.Y,C.B.Y)-R && P.Y<=FMath::Max(C.A.Y,C.B.Y)+R
        && P.Z>=FMath::Min(C.A.Z,C.B.Z)-R && P.Z<=FMath::Max(C.A.Z,C.B.Z)+R;
}
bool IsOutside(FVector3d P,TConstArrayView<FCapsule> Capsules,double Thickness,TConstArrayView<FConvexCollider> Convexes)
{
    for(const auto& C : Capsules)
    {
        const double R=C.RadiusCm+Thickness-1.e-9;
        if(InBounds(P,C,R) && (P-AxisPoint(P,C)).SquaredLength()<R*R) return false;
    }
    for(const auto& C : Convexes)
    {
        FVector3d Normal;
        if(C.GetBounds().ExpandBy(Thickness).IsInsideOrOn(P) && C.SignedDistance(P,Normal)<Thickness-1.e-7) return false;
    }
    return true;
}
// The intersection of a line with a convex capsule is one interval. A capsule
// is the union of its finite cylinder and both endpoint spheres.
bool LineInterval(FVector3d P,FVector3d D,const FCapsule& C,double Thickness,FVector2d& Interval)
{
    double Lo=DBL_MAX,Hi=-DBL_MAX;
    const double R=C.RadiusCm+Thickness;
    auto Include=[&](double A,double B) { if(A<=B) {Lo=FMath::Min(Lo,A); Hi=FMath::Max(Hi,B);} };
    auto Sphere=[&](FVector3d Center)
    {
        const auto M=P-Center;
        const double B=FVector3d::DotProduct(M,D),Disc=B*B-M.SquaredLength()+R*R;
        if(Disc>=0) {const double H=FMath::Sqrt(Disc); Include(-B-H,-B+H);}
    };
    Sphere(C.A); Sphere(C.B);
    const auto Axis=C.B-C.A; const double Length=Axis.Length();
    if(Length>1.e-9)
    {
        const auto N=Axis/Length,M=P-C.A;
        const double DZ=FVector3d::DotProduct(D,N),Z=FVector3d::DotProduct(M,N);
        const auto PerpD=D-DZ*N,PerpM=M-Z*N;
        const double A=PerpD.SquaredLength(),B=FVector3d::DotProduct(PerpD,PerpM),Q=PerpM.SquaredLength()-R*R;
        double RadialLo=-DBL_MAX,RadialHi=DBL_MAX;
        bool Hit=true;
        if(A>1.e-14)
        {
            const double Disc=B*B-A*Q;
            if(Disc<0) Hit=false;
            else {const double H=FMath::Sqrt(Disc); RadialLo=(-B-H)/A; RadialHi=(-B+H)/A;}
        }
        else Hit=Q<=0;
        if(FMath::Abs(DZ)>1.e-12)
        {
            const double T0=-Z/DZ,T1=(Length-Z)/DZ;
            RadialLo=FMath::Max(RadialLo,FMath::Min(T0,T1)); RadialHi=FMath::Min(RadialHi,FMath::Max(T0,T1));
        }
        else Hit &= Z>=0 && Z<=Length;
        if(Hit) Include(RadialLo,RadialHi);
    }
    Interval=FVector2d(Lo,Hi); return Lo<=Hi;
}
}
FVector3d ProjectBodyCollision(FVector3d Position,TConstArrayView<FCapsule> Capsules,double ThicknessCm,TConstArrayView<FConvexCollider> Convexes)
{
    const FVector3d Original=Position;
    for(const auto& C : Capsules)
    {
        const double R=C.RadiusCm+ThicknessCm;
        if(!InBounds(Position,C,R)) continue;
        const FVector3d Axis=C.B-C.A;
        const FVector3d Q=C.A+Axis*FMath::Clamp(FVector3d::DotProduct(Position-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
        const FVector3d D=Position-Q;
        if(D.SquaredLength()<R*R)
        {
            const double L=D.Length();
            Position=Q+(L>1.e-9?D/L:FVector3d(1,0,0))*R;
        }
    }
    for(const auto& C : Convexes)
    {
        if(!C.GetBounds().ExpandBy(ThicknessCm).IsInsideOrOn(Position)) continue;
        FVector3d Normal; const double Phi=C.SignedDistance(Position,Normal);
        if(Phi<ThicknessCm) Position+=Normal*(ThicknessCm-Phi+1.e-5);
    }
    if(IsOutside(Position,Capsules,ThicknessCm,Convexes)) return Position;

    // Sequential projections can cycle in overlapping shapes. In that case,
    // merge exact line/capsule intervals and take the first exit from the union.
    // The finite direction set bounds work; it is not a global nearest-point solver.
    TArray<FVector3d,TInlineAllocator<40>> Directions={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(const auto& C : Capsules)
    {
        const auto Delta=Original-AxisPoint(Original,C);
        if(Delta.SquaredLength()>1.e-16 && Delta.SquaredLength()<FMath::Square(C.RadiusCm+ThicknessCm))
        {
            Directions.Add(Delta/FMath::Sqrt(Delta.SquaredLength()));
            if(Directions.Num()>=38) break;
        }
    }
    for(const auto& C : Convexes)
    {
        FVector3d Normal;
        if(C.SignedDistance(Original,Normal)<ThicknessCm) Directions.Add(Normal);
        if(Directions.Num()>=70) break;
    }
    double BestDistance=DBL_MAX; FVector3d Best=Position;
    TArray<FVector2d,TInlineAllocator<32>> Intervals;
    for(auto D : Directions)
    {
        Intervals.Reset();
        for(const auto& C : Capsules)
        {
            FVector2d Interval;
            if(LineInterval(Original,D,C,ThicknessCm,Interval) && Interval.Y>=0) Intervals.Add(Interval);
        }
        for(const auto& C : Convexes)
        {
            FVector2d Interval; FVector3d Normal;
            if(C.LineInterval(Original,D,ThicknessCm,Interval,Normal) && Interval.Y>=0) Intervals.Add(Interval);
        }
        Intervals.Sort([](const FVector2d& A,const FVector2d& B){return A.X<B.X;});
        double Exit=0;
        for(auto Interval : Intervals)
        {
            if(Interval.X>Exit+1.e-9) break;
            Exit=FMath::Max(Exit,Interval.Y);
        }
        const auto Candidate=Original+D*(Exit+(Convexes.IsEmpty()?1.e-7:1.e-5));
        if(Exit<BestDistance && IsOutside(Candidate,Capsules,ThicknessCm,Convexes)) {BestDistance=Exit; Best=Candidate;}
    }
    return Best;
}
FVector3d SweepBodyCollision(FVector3d Previous,FVector3d Target,TConstArrayView<FCapsule> Capsules,double ThicknessCm,TConstArrayView<FConvexCollider> Convexes)
{
    FVector3d Position=ProjectBodyCollision(Previous,Capsules,ThicknessCm,Convexes);
    FVector3d Remaining=Target-Previous;
    for(int32 Contact=0;Contact<6;++Contact)
    {
        const double Length=Remaining.Length();
        if(Length<1.e-10) return Position;
        const auto D=Remaining/Length,End=Position+Remaining;
        double Earliest=Length; bool Hit=false; FVector3d Normal;
        for(int32 I=0;I<Capsules.Num();++I)
        {
            const auto& C=Capsules[I]; const double R=C.RadiusCm+ThicknessCm;
            if(FMath::Max(Position.X,End.X)<FMath::Min(C.A.X,C.B.X)-R || FMath::Min(Position.X,End.X)>FMath::Max(C.A.X,C.B.X)+R
                || FMath::Max(Position.Y,End.Y)<FMath::Min(C.A.Y,C.B.Y)-R || FMath::Min(Position.Y,End.Y)>FMath::Max(C.A.Y,C.B.Y)+R
                || FMath::Max(Position.Z,End.Z)<FMath::Min(C.A.Z,C.B.Z)-R || FMath::Min(Position.Z,End.Z)>FMath::Max(C.A.Z,C.B.Z)+R) continue;
            FVector2d Interval;
            if(LineInterval(Position,D,C,ThicknessCm,Interval) && Interval.Y>1.e-9 && Interval.X>=-1.e-7 && Interval.X<Earliest)
            {
                Earliest=FMath::Max(0.0,Interval.X); Hit=true;
                Normal=(Position+D*Earliest-AxisPoint(Position+D*Earliest,C)).GetSafeNormal();
            }
        }
        for(const auto& C : Convexes)
        {
            FBox3d SegmentBox(ForceInit); SegmentBox+=Position; SegmentBox+=End;
            if(!SegmentBox.Intersect(C.GetBounds().ExpandBy(ThicknessCm))) continue;
            FVector2d Interval; FVector3d EntryNormal;
            if(!C.LineInterval(Position,D,ThicknessCm,Interval,EntryNormal) || Interval.Y<=1.e-7 || Interval.X>=Earliest) continue;
            // GJK's conservative interval may contain an already-clear start.
            // Use the local surface normal there: the remote entry face normal
            // incorrectly blocks motion away from the opposite exit face.
            if(Interval.X<0) C.SignedDistance(Position,EntryNormal);
            if(FVector3d::DotProduct(D,EntryNormal)<-1.e-12)
            {Earliest=FMath::Max(0.0,Interval.X); Hit=true; Normal=EntryNormal;}
        }
        if(!Hit) return IsOutside(End,Capsules,ThicknessCm,Convexes)?End:Position;
        // Stop just before entry, which stays outside all shapes even at an
        // intersection of several surfaces. Retain the tangential remainder.
        Position+=D*FMath::Max(0.0,Earliest-(Convexes.IsEmpty()?1.e-7:1.e-5));
        Remaining-=D*Earliest;
        Remaining-=Normal*FMath::Min(0.0,FVector3d::DotProduct(Remaining,Normal));
    }
    // A bounded contact solve may discard excess motion, never teleport it.
    return Position;
}
bool ReadBodyCollision(USkeletalMesh* Body,TArray<FCapsule>& Out,TArray<FConvexCollider>& Convexes,FString& Error)
{
    Error.Reset();
    auto Fail=[&](const TCHAR* Why){Error=Why; return false;};
    if(!Body || !Body->GetPhysicsAsset()) return Fail(TEXT("Reference body requires a saved Physics Asset"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    const auto& Skeleton=Body->GetRefSkeleton();
    const auto& Local=Skeleton.GetRefBonePose();
    if(Local.IsEmpty() || Local.Num()>1024) return Fail(TEXT("Reference skeleton is missing or too large"));
    TArray<FTransform> Component;
    for(int32 I=0;I<Local.Num();++I)
    {
        const int32 Parent=Skeleton.GetParentIndex(I);
        if(!Local[I].IsValid() || (Parent!=INDEX_NONE && (Parent<0 || Parent>=I))) return Fail(TEXT("Reference skeleton transform or hierarchy is invalid"));
        const FTransform T=Parent==INDEX_NONE?Local[I]:Local[I]*Component[Parent];
        const FVector Scale=T.GetScale3D();
        if(!T.IsValid() || Scale.X<=0 || !Scale.Equals(FVector(Scale.X),1.e-6))
            return Fail(TEXT("Reference body collision requires positive uniform bone scale"));
        Component.Add(T);
    }
    TArray<FCapsule> Result; TArray<FConvexCollider> ConvexResult;
    for(const USkeletalBodySetup* Setup : Body->GetPhysicsAsset()->SkeletalBodySetups)
    {
        if(!Setup) return Fail(TEXT("Physics Asset has a missing body setup"));
        const int32 Bone=Skeleton.FindBoneIndex(Setup->BoneName);
        if(!Component.IsValidIndex(Bone)) return Fail(TEXT("Physics body refers to a missing reference bone"));
        const auto& Geometry=Setup->AggGeom;
        if(Geometry.GetElementCount()!=Geometry.SphylElems.Num()+Geometry.SphereElems.Num()+Geometry.ConvexElems.Num())
            return Fail(TEXT("Native fitting supports capsule, sphere and convex Physics shapes only"));
        const FTransform BoneTransform=Component[Bone];
        const double Scale=BoneTransform.GetScale3D().X;
        auto Add=[&](FVector A,FVector B,double Radius)
        {
            if(A.ContainsNaN() || B.ContainsNaN() || A.GetAbsMax()>10000 || B.GetAbsMax()>10000
                || !FMath::IsFinite(Radius) || Radius<=0 || Radius>10000 || Result.Num()>=1024) return false;
            Result.Add({A,B,Radius}); return true;
        };
        for(const auto& Shape : Geometry.SphylElems)
        {
            if(!FMath::IsFinite(Shape.Length) || Shape.Length<0) return Fail(TEXT("Physics capsule length is invalid"));
            const FTransform T=Shape.GetTransform()*BoneTransform;
            if(!T.IsValid() || !Add(T.TransformPosition(FVector(0,0,-Shape.Length*.5)),T.TransformPosition(FVector(0,0,Shape.Length*.5)),Shape.Radius*Scale))
                return Fail(TEXT("Physics capsule coordinates, radius or count are invalid"));
        }
        for(const auto& Shape : Geometry.SphereElems)
        {
            const FVector Center=BoneTransform.TransformPosition(Shape.Center);
            if(!Add(Center,Center,Shape.Radius*Scale)) return Fail(TEXT("Physics sphere coordinates, radius or count are invalid"));
        }
        for(const auto& Shape : Geometry.ConvexElems)
        {
            FConvexCollider Convex;
            if(ConvexResult.Num()+Result.Num()>=1024) return Fail(TEXT("Too many body collision shapes"));
            if(!ReadPhysicsConvex(Shape,BoneTransform,Convex,Error)) return false;
            ConvexResult.Add(MoveTemp(Convex));
        }
    }
    if(Result.IsEmpty() && ConvexResult.IsEmpty()) return Fail(TEXT("Reference body has no supported collision shapes"));
    Out=MoveTemp(Result); Convexes=MoveTemp(ConvexResult); return true;
}
bool ReadBodyCollision(USkeletalMesh* Body,TArray<FCapsule>& Out,FString& Error)
{
    TArray<FCapsule> Result; TArray<FConvexCollider> Convexes;
    if(!ReadBodyCollision(Body,Result,Convexes,Error)) return false;
    if(!Convexes.IsEmpty()) {Error=TEXT("Capsule-only reader cannot represent convex Physics shapes");return false;}
    Out=MoveTemp(Result); return true;
}
}
