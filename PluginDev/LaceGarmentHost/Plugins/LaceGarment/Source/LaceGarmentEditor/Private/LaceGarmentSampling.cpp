#include "LaceGarmentSampling.h"
namespace LaceGarment
{
namespace
{
struct FCurveTable
{
    int32 Panel=0;
    const FLGEdge* Edge=nullptr;
    TArray<double> T,Length,Chosen;
    TArray<int32> Boundary;
    TArray<FVector2d> Position;
    TArray<FVector2d> UsedIntervals;
    double At(double Parameter) const
    {
        for(int32 I=1;I<T.Num();++I)
            if(Parameter<=T[I]) return FMath::Lerp(Length[I-1],Length[I],(Parameter-T[I-1])/(T[I]-T[I-1]));
        return Length.Last();
    }
    double ParameterAt(double Distance) const
    {
        for(int32 I=1;I<Length.Num();++I)
            if(Distance<=Length[I]) return FMath::Lerp(T[I-1],T[I],(Distance-Length[I-1])/(Length[I]-Length[I-1]));
        return 1;
    }
};
void SortUnique(TArray<double>& Values)
{
    Values.Sort();
    for(int32 I=Values.Num()-1;I>0;--I) if(FMath::Abs(Values[I]-Values[I-1])<1.e-10) Values.RemoveAt(I);
}
bool ValidateSourceLoops(const TArray<TArray<FVector2d>>& Loops)
{
    struct FSegment { int32 Loop,Index,Count; FVector2d A,B,Min,Max; };
    TArray<FSegment> Segments;
    constexpr double Tolerance=1.e-9;
    auto Cross=[](FVector2d A,FVector2d B){return A.X*B.Y-A.Y*B.X;};
    auto Distance=[](FVector2d P,FVector2d A,FVector2d B)
    {
        auto D=B-A; const double L=D.SquaredLength();
        return L>1.e-20?(P-(A+D*FMath::Clamp(FVector2d::DotProduct(P-A,D)/L,0.0,1.0))).Length():(P-A).Length();
    };
    for(int32 L=0;L<Loops.Num();++L)
    {
        const auto& Points=Loops[L];
        for(int32 I=0;I<Points.Num();++I)
        {
        const auto A=Points[I],B=Points[(I+1)%Points.Num()],C=Points[(I+2)%Points.Num()];
        if((B-A).SquaredLength()<1.e-20 || (FMath::Abs(Cross(B-A,C-B))<1.e-10 && FVector2d::DotProduct(B-A,C-B)<0)) return false;
        Segments.Add({L,I,Points.Num(),A,B,FVector2d(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),FVector2d(FMath::Max(A.X,B.X),FMath::Max(A.Y,B.Y))});
        }
    }
    Segments.Sort([](const auto& A,const auto& B){return A.Min.X<B.Min.X;});
    int32 Work=0;
    for(int32 I=0;I<Segments.Num();++I)
        for(int32 J=I+1;J<Segments.Num() && Segments[J].Min.X<=Segments[I].Max.X+Tolerance;++J)
        {
            if(++Work>5000000) return false;
            const auto& A=Segments[I]; const auto& B=Segments[J];
            const int32 Gap=FMath::Abs(A.Index-B.Index);
            if((A.Loop==B.Loop && (Gap==1 || Gap==A.Count-1)) || A.Min.Y>B.Max.Y+Tolerance || B.Min.Y>A.Max.Y+Tolerance) continue;
            const double U=Cross(A.B-A.A,B.A-A.A),V=Cross(A.B-A.A,B.B-A.A),S=Cross(B.B-B.A,A.A-B.A),T=Cross(B.B-B.A,A.B-B.A);
            if((U*V<0 && S*T<0) || FMath::Min(FMath::Min(Distance(A.A,B.A,B.B),Distance(A.B,B.A,B.B)),FMath::Min(Distance(B.A,A.A,A.B),Distance(B.B,A.A,A.B)))<=Tolerance)
                return false;
        }
    auto Inside=[&](FVector2d P,int32 L)
    {
        bool Result=false; const auto& Points=Loops[L];
        for(int32 I=0;I<Points.Num();++I)
        {
            const auto A=Points[I],B=Points[(I+1)%Points.Num()];
            if((A.Y>P.Y)!=(B.Y>P.Y)) if(P.X<(B.X-A.X)*(P.Y-A.Y)/(B.Y-A.Y)+A.X) Result=!Result;
        }
        return Result;
    };
    for(int32 L=1;L<Loops.Num();++L)
    {
        if(!Inside(Loops[L][0],0)) return false;
        for(int32 Other=1;Other<Loops.Num();++Other) if(Other!=L && Inside(Loops[L][0],Other)) return false;
    }
    return true;
}
}
bool SampleDocument(const ULaceGarmentDocument& D,FSampledDocument& Out,FString& Error)
{
    Error.Reset();
    if(!D.ValidateStructure(Error)) return false;
    auto Fail=[&](const FString& Message) { Error=Message; return false; };
    if(D.Pieces.IsEmpty()) return Fail(TEXT("Document has no pieces to generate"));
    FSampledDocument Result;
    const double Spacing=FMath::Min(D.BoundarySpacingMm,D.MeshMaxEdgeMm*.5);
    TArray<FCurveTable> Curves;
    TMap<FGuid,int32> CurveById,PanelById;
    TMap<FGuid,const FLGNotch*> NotchById;
    int32 EvaluationBudget=0;
    for(int32 Panel=0;Panel<D.Pieces.Num();++Panel)
    {
        const auto& Piece=D.Pieces[Panel]; PanelById.Add(Piece.Id,Panel);
        int32 PlannedSamples=0;
        TArray<TArray<FVector2d>> SourceLoops;
        TArray<FLGLoop> Loops={Piece.Outer}; Loops.Append(Piece.Holes);
        int32 SourceSamples=0;
        if(Piece.MirrorOf.IsValid()) return Fail(TEXT("Linked symmetry must be evaluated before sampling; generation is not implemented yet"));
        for(const auto& N : Piece.Notches) NotchById.Add(N.Id,&N);
        for(const auto& Loop : Loops)
        {
        if(Loop.Edges.Num()<3) return Fail(TEXT("Piece boundary must be closed with at least three edges"));
        TArray<FVector2d> SourceLoop;
        for(int32 K=0;K<Loop.Edges.Num();++K)
        {
            const auto* Edge=Piece.Edges.FindByPredicate([&](const FLGEdge& E){return E.Id==Loop.Edges[K];});
            const auto* Next=Piece.Edges.FindByPredicate([&](const FLGEdge& E){return E.Id==Loop.Edges[(K+1)%Loop.Edges.Num()];});
            if(Edge->End!=Next->Start) return Fail(FString::Printf(TEXT("Piece %s: boundary is not closed in edge order"),*Piece.Id.ToString()));
            FCurveTable C; C.Panel=Panel; C.Edge=Edge;
            auto Evaluate=[&](double T,FVector2D& P){return D.EvaluateEdge(Piece.Id,Edge->Id,T,P,Error);};
            FVector2D A,B;
            if(!Evaluate(0,A)||!Evaluate(1,B)) return false;
            C.T.Add(0); C.Position.Add(A);
            TFunction<bool(double,FVector2D,double,FVector2D,int32)> Subdivide;
            Subdivide=[&](double TA,FVector2D PA,double TB,FVector2D PB,int32 Depth)
            {
                if(++EvaluationBudget>1000000 || C.T.Num()>16384) return Fail(TEXT("Curve sampling exceeds work budget"));
                FVector2D Q,M,R;
                if(!Evaluate(FMath::Lerp(TA,TB,.25),Q)||!Evaluate((TA+TB)*.5,M)||!Evaluate(FMath::Lerp(TA,TB,.75),R)) return false;
                const double Deviation=FMath::Max3((Q-FMath::Lerp(PA,PB,.25)).Length(),(M-(PA+PB)*.5).Length(),(R-FMath::Lerp(PA,PB,.75)).Length());
                if(Deviation<=1.e-5)
                { C.T.Add(TB); C.Position.Add(PB); return true; }
                if(Depth>=20) return Fail(TEXT("Curve exceeds bounded sampling accuracy"));
                return Subdivide(TA,PA,(TA+TB)*.5,M,Depth+1) && Subdivide((TA+TB)*.5,M,TB,PB,Depth+1);
            };
            if(Edge->Curve==ELGCurveType::Line) { C.T.Add(1); C.Position.Add(B); }
            else if(!Subdivide(0,A,1,B,0)) return false;
            C.Length.Add(0);
            for(int32 I=1;I<C.T.Num();++I)
            {
                const double Segment=(C.Position[I]-C.Position[I-1]).Length();
                if(Segment<1.e-10) return Fail(TEXT("Curve has a degenerate interval"));
                C.Length.Add(C.Length.Last()+Segment);
            }
            const double Length=C.Length.Last();
            const double CountDouble=FMath::CeilToDouble(Length/Spacing);
            if(Length<1.e-6 || CountDouble>1024) return Fail(TEXT("Curve length or sample budget is invalid"));
            const int32 Count=FMath::Max(1,int32(CountDouble));
            PlannedSamples+=Count;
            for(const auto& N : Piece.Notches) if(N.Edge==Edge->Id) ++PlannedSamples;
            if(PlannedSamples>1024) return Fail(TEXT("Piece exceeds boundary sampling work budget"));
            for(int32 I=0;I<=Count;++I) C.Chosen.Add(C.ParameterAt(Length*I/Count));
            for(const auto& N : Piece.Notches) if(N.Edge==Edge->Id) C.Chosen.Add(N.Parameter);
            SourceSamples+=C.Position.Num()-1;
            if(SourceSamples>16384) return Fail(TEXT("Source curve validation exceeds piece work budget"));
            for(int32 I=0;I<C.Position.Num()-1;++I) SourceLoop.Add(C.Position[I]);
            CurveById.Add(Edge->Id,Curves.Num()); Curves.Add(MoveTemp(C));
        }
        SourceLoops.Add(MoveTemp(SourceLoop));
        }
        if(!ValidateSourceLoops(SourceLoops)) return Fail(FString::Printf(TEXT("Piece %s: source curve intersects, touches, has invalid cutout containment, or exceeds intersection work budget"),*Piece.Id.ToString()));
    }
    // Topological order is retained in the generated seam list.
    TArray<const FLGSeam*> Ordered;
    TSet<FGuid> Visited;
    TFunction<void(const FLGSeam&)> Order=[&](const FLGSeam& S)
    {
        if(Visited.Contains(S.Id)) return;
        Visited.Add(S.Id);
        for(const FGuid& Before : S.Prerequisites) Order(*D.Seams.FindByPredicate([&](const FLGSeam& X){return X.Id==Before;}));
        Ordered.Add(&S);
    };
    for(const auto& S : D.Seams) Order(S);
    for(const FLGSeam* Seam : Ordered)
    {
        auto SeamFail=[&](const TCHAR* Why){return Fail(FString::Printf(TEXT("Seam %s: %s"),*Seam->Id.ToString(),Why));};
        if(Seam->Type!=ELGSeamType::Join) return SeamFail(TEXT("hem and binding generation are not implemented"));
        const int32* IA=CurveById.Find(Seam->A.Edge); const int32* IB=CurveById.Find(Seam->B.Edge);
        if(!IA||!IB) return SeamFail(TEXT("interval must lie on a generated boundary"));
        auto& A=Curves[*IA]; auto& B=Curves[*IB];
        auto Claim=[&](FCurveTable& C,const FLGEdgeInterval& V)
        {
            for(auto Used : C.UsedIntervals) if(FMath::Max(Used.X,V.From)<FMath::Min(Used.Y,V.To)-1.e-10) return false;
            C.UsedIntervals.Add(FVector2d(V.From,V.To)); return true;
        };
        if(!Claim(A,Seam->A)||!Claim(B,Seam->B)) return SeamFail(TEXT("a boundary interval cannot be sewn twice"));
        const double A0=A.At(Seam->A.From),A1=A.At(Seam->A.To),B0=B.At(Seam->B.From),B1=B.At(Seam->B.To);
        const double LA=A1-A0,LB=B1-B0;
        if(LA<1.e-6||LB<1.e-6 || FMath::Abs(LA-Seam->EaseRatio*LB)>Seam->LengthToleranceMm+1.e-8)
            return SeamFail(TEXT("lengths exceed authored ease ratio and tolerance (LengthA = EaseRatio * LengthB)"));
        auto QA=[&](double T){return (A.At(T)-A0)/LA;};
        auto QB=[&](double T){const double Q=(B.At(T)-B0)/LB;return Seam->ReverseB?1-Q:Q;};
        TArray<FVector2d> Landmarks={{0,0},{1,1}};
        auto InRange=[](const FLGNotch& N,const FLGEdgeInterval& V){return N.Edge==V.Edge && N.Parameter>=V.From && N.Parameter<=V.To;};
        for(const auto& N : D.Pieces[A.Panel].Notches)
        {
            if(!InRange(N,Seam->A)||!N.MatchingNotch.IsValid()) continue;
            const auto* M=NotchById.FindRef(N.MatchingNotch);
            if(!M||!InRange(*M,Seam->B) || (M->MatchingNotch.IsValid() && M->MatchingNotch!=N.Id))
                return SeamFail(TEXT("paired notch is outside the opposite interval or conflicts"));
            Landmarks.Add(FVector2d(QA(N.Parameter),QB(M->Parameter)));
        }
        for(const auto& N : D.Pieces[B.Panel].Notches)
        {
            if(!InRange(N,Seam->B)||!N.MatchingNotch.IsValid()) continue;
            const auto* M=NotchById.FindRef(N.MatchingNotch);
            if(!M||!InRange(*M,Seam->A) || (M->MatchingNotch.IsValid() && M->MatchingNotch!=N.Id))
                return SeamFail(TEXT("paired notch is outside the opposite interval or conflicts"));
            Landmarks.Add(FVector2d(QA(M->Parameter),QB(N.Parameter)));
        }
        Landmarks.Sort([](auto X,auto Y){return X.X<Y.X;});
        for(int32 I=Landmarks.Num()-1;I>0;--I) if(Landmarks[I].Equals(Landmarks[I-1],1.e-10)) Landmarks.RemoveAt(I);
        A.Chosen.RemoveAll([&](double T){return T>=Seam->A.From-1.e-10 && T<=Seam->A.To+1.e-10;});
        B.Chosen.RemoveAll([&](double T){return T>=Seam->B.From-1.e-10 && T<=Seam->B.To+1.e-10;});
        for(int32 K=1;K<Landmarks.Num();++K)
        {
            const auto From=Landmarks[K-1],To=Landmarks[K];
            if(To.X-From.X<1.e-10 || To.Y-From.Y<1.e-10) return SeamFail(TEXT("paired notches contradict seam direction"));
            TArray<double> Fractions;
            const int32 Count=FMath::CeilToInt(FMath::Max((To.X-From.X)*LA,(To.Y-From.Y)*LB)/Spacing);
            if(Count>1024) return SeamFail(TEXT("sample count exceeds budget"));
            for(int32 I=0;I<=Count;++I) Fractions.Add(double(I)/Count);
            for(const auto& N : D.Pieces[A.Panel].Notches)
                if(InRange(N,Seam->A)) { const double Q=QA(N.Parameter); if(Q>=From.X && Q<=To.X) Fractions.Add((Q-From.X)/(To.X-From.X)); }
            for(const auto& N : D.Pieces[B.Panel].Notches)
                if(InRange(N,Seam->B)) { const double Q=QB(N.Parameter); if(Q>=From.Y && Q<=To.Y) Fractions.Add((Q-From.Y)/(To.Y-From.Y)); }
            SortUnique(Fractions);
            for(double F : Fractions)
            {
                const double UA=FMath::Lerp(From.X,To.X,F),UB=FMath::Lerp(From.Y,To.Y,F);
                A.Chosen.Add(A.ParameterAt(A0+UA*LA)); B.Chosen.Add(B.ParameterAt(B0+(Seam->ReverseB?1-UB:UB)*LB));
            }
        }
        // Source arcs and final straight mesh edges must both meet the authored tolerance.
        // Refine paired samples together; never broaden the user's length tolerance.
        for(int32 Pass=0;;++Pass)
        {
            SortUnique(A.Chosen); SortUnique(B.Chosen);
            TArray<double> TA,TB;
            for(double T : A.Chosen) if(T>=Seam->A.From-1.e-10 && T<=Seam->A.To+1.e-10) TA.Add(T);
            for(double T : B.Chosen) if(T>=Seam->B.From-1.e-10 && T<=Seam->B.To+1.e-10) TB.Add(T);
            auto ChordLength=[&](const FCurveTable& C,const TArray<double>& Parameters)
            {
                double Sum=0; FVector2D Previous;
                D.EvaluateEdge(D.Pieces[C.Panel].Id,C.Edge->Id,Parameters[0],Previous,Error);
                for(int32 I=1;I<Parameters.Num();++I)
                { FVector2D P; D.EvaluateEdge(D.Pieces[C.Panel].Id,C.Edge->Id,Parameters[I],P,Error); Sum+=(P-Previous).Length(); Previous=P; }
                return Sum;
            };
            if(FMath::Abs(ChordLength(A,TA)-Seam->EaseRatio*ChordLength(B,TB))<=Seam->LengthToleranceMm+1.e-8) break;
            if(Pass>=12 || TA.Num()!=TB.Num() || A.Chosen.Num()+TA.Num()>1024 || B.Chosen.Num()+TB.Num()>1024)
                return SeamFail(TEXT("mesh sampling cannot meet the authored length tolerance within the vertex budget"));
            for(int32 I=1;I<TA.Num();++I) A.Chosen.Add(A.ParameterAt((A.At(TA[I-1])+A.At(TA[I]))*.5));
            for(int32 I=1;I<TB.Num();++I) B.Chosen.Add(B.ParameterAt((B.At(TB[I-1])+B.At(TB[I]))*.5));
        }
    }
    Result.BoundarySource.SetNum(D.Pieces.Num()); Result.Input.Panels.SetNum(D.Pieces.Num());
    TMap<FGuid,FIntPoint> PointToBoundary;
    for(int32 Panel=0;Panel<D.Pieces.Num();++Panel)
    {
        const auto& Piece=D.Pieces[Panel]; auto& P=Result.Input.Panels[Panel];
        P.Id=Piece.Id; P.InitialPlacementCm=Piece.InitialPlacementCm; P.MaxEdgeMm=D.MeshMaxEdgeMm;
        TArray<FLGLoop> Loops={Piece.Outer}; Loops.Append(Piece.Holes);
        for(const auto& Loop : Loops)
        {
        P.LoopStarts.Add(P.BoundaryMm.Num());
        for(const FGuid& Id : Loop.Edges)
        {
            auto& C=Curves[CurveById[Id]]; SortUnique(C.Chosen);
            for(int32 I=0;I<C.Chosen.Num()-1;++I)
            {
                FVector2D V; if(!D.EvaluateEdge(Piece.Id,Id,C.Chosen[I],V,Error)) return false;
                C.Boundary.Add(P.BoundaryMm.Num()); P.BoundaryMm.Add(V);
                FBoundarySource Ref; Ref.Edge=Id; Ref.Parameter=C.Chosen[I];
                if(I==0) { Ref.Point=C.Edge->Start; PointToBoundary.Add(Ref.Point,FIntPoint(Panel,P.BoundaryMm.Num()-1)); }
                Result.BoundarySource[Panel].Add(Ref);
            }
            if(P.BoundaryMm.Num()>1024) return Fail(TEXT("Piece exceeds 1024 sampled boundary vertices"));
        }
        }
        FPanelMesh Validation;
        if(!BuildPanel(P,Validation,Error)) { Error=FString::Printf(TEXT("Piece %s: %s"),*Piece.Id.ToString(),*Error); return false; }
    }
    for(const auto& Anchor : D.Anchors)
    {
        const auto* Index=PointToBoundary.Find(Anchor.Point);
        if(!Index || Index->X!=PanelById[Anchor.Piece]) return Fail(TEXT("Anchor point must be a generated boundary endpoint, not a curve control handle"));
        FPin Pin; Pin.Panel=Index->X; Pin.BoundaryPoint=Index->Y;
        if(Anchor.bUseTarget) Pin.TargetCm=Anchor.TargetCm;
        Result.Input.Pins.Add(Pin);
    }
    if(Result.Input.Pins.IsEmpty()) return Fail(TEXT("At least one authored placement anchor is required"));
    for(const auto* Seam : Ordered)
    {
        const auto& A=Curves[CurveById[Seam->A.Edge]]; const auto& B=Curves[CurveById[Seam->B.Edge]];
        FSeam S; S.PanelA=A.Panel; S.PanelB=B.Panel; S.bReverseB=Seam->ReverseB; S.EaseRatio=Seam->EaseRatio; S.LengthToleranceMm=Seam->LengthToleranceMm;
        auto Edges=[](const FCurveTable& C,const FLGEdgeInterval& V,TArray<int32>& Output)
        {
            for(int32 I=0;I<C.Boundary.Num();++I)
                if(C.Chosen[I]>=V.From-1.e-10 && C.Chosen[I+1]<=V.To+1.e-10) Output.Add(C.Boundary[I]);
        };
        Edges(A,Seam->A,S.BoundaryEdgesA); Edges(B,Seam->B,S.BoundaryEdgesB);
        if(S.BoundaryEdgesA.IsEmpty() || S.BoundaryEdgesA.Num()!=S.BoundaryEdgesB.Num()) return Fail(TEXT("Paired boundary sampling did not preserve seam correspondence"));
        Result.Input.Seams.Add(MoveTemp(S)); Result.SeamIds.Add(Seam->Id);
    }
    Out=MoveTemp(Result); return true;
}
}
