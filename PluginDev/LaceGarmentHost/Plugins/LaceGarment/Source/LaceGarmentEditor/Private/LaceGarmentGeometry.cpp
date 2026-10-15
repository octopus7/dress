#include "LaceGarmentGeometry.h"
#include "CompGeom/Delaunay2.h"
namespace LaceGarment
{
namespace
{
double Cross(FVector2d A, FVector2d B) { return A.X * B.Y - A.Y * B.X; }
bool OnSegment(FVector2d P, FVector2d A, FVector2d B)
{
    return FMath::Abs(Cross(B-A, P-A)) < 1.e-8 && FVector2d::DotProduct(P-A, P-B) <= 1.e-8;
}
bool Intersects(FVector2d A, FVector2d B, FVector2d C, FVector2d D)
{
    const double U = Cross(B-A, C-A), V = Cross(B-A, D-A);
    const double S = Cross(D-C, A-C), T = Cross(D-C, B-C);
    return (U*V < 0 && S*T < 0) || OnSegment(A,C,D) || OnSegment(B,C,D) || OnSegment(C,A,B) || OnSegment(D,A,B);
}
uint64 EdgeKey(int32 A, int32 B)
{
    return (uint64(FMath::Min(A,B)) << 32) | uint32(FMath::Max(A,B));
}
}

bool BuildPanel(const FPanel& Panel, FPanelMesh& Out, FString& Error)
{
    Error.Reset();
    auto Fail = [&](const TCHAR* Message) { Error = Message; return false; };
    const int32 N = Panel.BoundaryMm.Num();
    if (N < 3 || N > 1024) return Fail(TEXT("A panel requires 3 to 1024 boundary points"));
    if (!FMath::IsFinite(Panel.MaxEdgeMm) || Panel.MaxEdgeMm < 0.1 || Panel.MaxEdgeMm > 100)
        return Fail(TEXT("Maximum edge must be finite and between 0.1 and 100 mm"));
    if (!Panel.InitialPlacementCm.IsValid() || !Panel.InitialPlacementCm.GetScale3D().Equals(FVector::OneVector, 1.e-6))
        return Fail(TEXT("Initial placement must be finite, normalized, and have unit scale"));
    if (Panel.InitialPlacementCm.GetTranslation().GetAbsMax()>10000)
        return Fail(TEXT("Initial placement exceeds the supported 10000 cm coordinate range"));
    TArray<int32> Starts=Panel.LoopStarts.IsEmpty()?TArray<int32>{0}:Panel.LoopStarts;
    if(Starts.Num()>33 || Starts[0]!=0) return Fail(TEXT("Invalid boundary loop offsets"));
    TArray<int32> Next; Next.SetNum(N);
    TArray<double> LoopAreas;
    for(int32 L=0;L<Starts.Num();++L)
    {
        const int32 Begin=Starts[L],End=L+1<Starts.Num()?Starts[L+1]:N;
        if(Begin<0 || End>N || End-Begin<3) return Fail(TEXT("Boundary loop offsets must be ordered and contain at least three points"));
        double Area=0;
        for(int32 I=Begin;I<End;++I) { Next[I]=I+1<End?I+1:Begin; Area+=Cross(Panel.BoundaryMm[I],Panel.BoundaryMm[Next[I]])*.5; }
        LoopAreas.Add(FMath::Abs(Area));
    }
    for (int32 I=0; I<N; ++I)
    {
        const FVector2d A=Panel.BoundaryMm[I], B=Panel.BoundaryMm[Next[I]];
        if (!FMath::IsFinite(A.X) || !FMath::IsFinite(A.Y) || FMath::Abs(A.X)>10000 || FMath::Abs(A.Y)>10000)
            return Fail(TEXT("Boundary coordinate is non-finite or outside supported 10000 mm range"));
        if ((A-B).SquaredLength() < 1.e-10) return Fail(TEXT("Boundary contains a zero length edge"));
        for (int32 J=I+1; J<N; ++J)
        {
            if (J==Next[I] || Next[J]==I) continue;
            if (Intersects(A,B,Panel.BoundaryMm[J],Panel.BoundaryMm[Next[J]]))
                return Fail(TEXT("Boundary self-intersects or touches a nonadjacent edge"));
        }
        // Collinear backtracking at adjacent edges also overlaps the polygon boundary.
        const FVector2d C=Panel.BoundaryMm[Next[Next[I]]];
        if (FMath::Abs(Cross(B-A,C-B))<1.e-8 && FVector2d::DotProduct(B-A,C-B)<0)
            return Fail(TEXT("Adjacent boundary edges overlap"));
    }
    auto InsideLoop=[&](FVector2d P,int32 L)
    {
        bool Inside=false; const int32 End=L+1<Starts.Num()?Starts[L+1]:N;
        for(int32 I=Starts[L];I<End;++I)
        {
            const auto A=Panel.BoundaryMm[I],B=Panel.BoundaryMm[Next[I]];
            if((A.Y>P.Y)!=(B.Y>P.Y)) if(P.X<(B.X-A.X)*(P.Y-A.Y)/(B.Y-A.Y)+A.X) Inside=!Inside;
        }
        return Inside;
    };
    double ExpectedArea=LoopAreas[0];
    for(int32 L=0;L<Starts.Num();++L)
    {
        if(LoopAreas[L]<1.e-6) return Fail(TEXT("Boundary loop has zero area"));
        if(L==0) continue;
        if(!InsideLoop(Panel.BoundaryMm[Starts[L]],0)) return Fail(TEXT("Inner loop must be inside the outer boundary"));
        for(int32 Other=1;Other<Starts.Num();++Other)
            if(Other!=L && InsideLoop(Panel.BoundaryMm[Starts[L]],Other)) return Fail(TEXT("Nested inner loops are not supported"));
        ExpectedArea-=LoopAreas[L];
    }
    if(ExpectedArea<1.e-6) return Fail(TEXT("Panel has zero area after cutouts"));

    FPanelMesh Result;
    Result.RestMm = Panel.BoundaryMm;
    TArray<UE::Geometry::FIndex2i> Constraints;
    for(int32 I=0; I<N; ++I)
    {
        const int32 End=Next[I];
        int32 Segments=1;
        while((Panel.BoundaryMm[End]-Panel.BoundaryMm[I]).Length()/Segments>Panel.MaxEdgeMm*(1+1.e-10)) Segments*=2;
        if(Result.RestMm.Num()+Segments-1>16384) return Fail(TEXT("Panel boundary exceeds the supported vertex budget"));
        TArray<int32> Edge; Edge.Add(I);
        for(int32 K=1; K<Segments; ++K) Edge.Add(Result.RestMm.Add(FMath::Lerp(Panel.BoundaryMm[I],Panel.BoundaryMm[End],double(K)/Segments)));
        Edge.Add(End);
        for(int32 K=0; K<Edge.Num()-1; ++K) Constraints.Add({Edge[K],Edge[K+1]});
        Result.BoundaryVertices.Add(MoveTemp(Edge));
    }
    // Interior samples prevent long skinny fans from a densely sampled curve.
    // Constraint edges preserve every original boundary ID, including collinear samples.
    FBox2d Bounds(ForceInit); for(auto P : Panel.BoundaryMm) Bounds+=P;
    const double Spacing=Panel.MaxEdgeMm*0.6;
    const double NX=FMath::CeilToDouble(Bounds.GetSize().X/Spacing), NY=FMath::CeilToDouble(Bounds.GetSize().Y/Spacing);
    if(NX*NY>262144) return Fail(TEXT("Panel sampling exceeds the supported work budget; increase edge spacing"));
    for(int32 Y=1; Y<int32(NY); ++Y)
        for(int32 X=1; X<int32(NX); ++X)
        {
            const FVector2d P=Bounds.Min+FVector2d(X*Spacing,Y*Spacing);
            bool Inside=false; double NearestSq=DBL_MAX;
            for(int32 I=0; I<N; ++I)
            {
                const FVector2d A=Panel.BoundaryMm[I],B=Panel.BoundaryMm[Next[I]],D=B-A;
                if((A.Y>P.Y)!=(B.Y>P.Y))
                    if(P.X<(B.X-A.X)*(P.Y-A.Y)/(B.Y-A.Y)+A.X) Inside=!Inside;
                const FVector2d Q=A+D*FMath::Clamp(FVector2d::DotProduct(P-A,D)/D.SquaredLength(),0.0,1.0);
                NearestSq=FMath::Min(NearestSq,(P-Q).SquaredLength());
            }
            if(Inside && NearestSq>FMath::Square(Spacing*0.25))
            {
                if(Result.RestMm.Num()>=16384) return Fail(TEXT("Panel interior exceeds the supported vertex budget; increase edge spacing"));
                Result.RestMm.Add(P);
            }
        }
    TArray<UE::Geometry::FIndex3i> Base;
    UE::Geometry::FDelaunay2 Delaunay; Delaunay.RandomStream.Initialize(0);
    const auto FillMode=Starts.Num()==1?UE::Geometry::FDelaunay2::EFillMode::Solid:UE::Geometry::FDelaunay2::EFillMode::OddWinding;
    if(!Delaunay.Triangulate(Result.RestMm,Constraints) || !Delaunay.GetFilledTriangles(Base,Constraints,FillMode)
        || Base.IsEmpty() || Base.Num()>32768) return Fail(TEXT("Constrained triangulation failed or exceeded the triangle budget"));
    for (auto T : Base)
    {
        if (Cross(Result.RestMm[T.B]-Result.RestMm[T.A],Result.RestMm[T.C]-Result.RestMm[T.A]) < 0) Swap(T.B,T.C);
        Result.Triangles.Add(FIntVector3(T.A,T.B,T.C));
    }
    // Split only overlong shared edges. Boundary sampling depends on that edge,
    // not the longest diagonal anywhere else in the panel.
    for (;;)
    {
        TSet<uint64> SplitEdges;
        int32 PredictedFaces=0;
        for (auto T : Result.Triangles)
        {
            int32 Count=0;
            for(int32 K=0; K<3; ++K)
                if((Result.RestMm[T[K]]-Result.RestMm[T[(K+1)%3]]).SquaredLength()>FMath::Square(Panel.MaxEdgeMm)*(1+1.e-10))
                { SplitEdges.Add(EdgeKey(T[K],T[(K+1)%3])); ++Count; }
            PredictedFaces+=Count+1;
            if(PredictedFaces>32768) return Fail(TEXT("Panel refinement exceeds the supported triangle budget; increase edge spacing"));
        }
        if(SplitEdges.IsEmpty()) break;
        TMap<uint64,int32> Midpoints;
        auto Mid = [&](int32 A,int32 B)
        {
            uint64 Key=EdgeKey(A,B);
            if (const int32* Existing=Midpoints.Find(Key)) return *Existing;
            int32 Id=Result.RestMm.Add((Result.RestMm[A]+Result.RestMm[B])*0.5);
            Midpoints.Add(Key,Id);
            return Id;
        };
        TArray<FIntVector3> Refined;
        Refined.Reserve(PredictedFaces);
        for (auto T : Result.Triangles)
        {
            bool Split[3]; int32 Count=0;
            for(int32 K=0; K<3; ++K) { Split[K]=SplitEdges.Contains(EdgeKey(T[K],T[(K+1)%3])); Count+=Split[K]?1:0; }
            if(Count==0) Refined.Add(T);
            else if(Count==3)
            {
                const int32 AB=Mid(T.X,T.Y),BC=Mid(T.Y,T.Z),CA=Mid(T.Z,T.X);
                Refined.Append({{T.X,AB,CA},{AB,T.Y,BC},{CA,BC,T.Z},{AB,BC,CA}});
            }
            else if(Count==1)
            {
                int32 K=0; while(!Split[K]) ++K;
                const int32 A=T[K],B=T[(K+1)%3],C=T[(K+2)%3],AB=Mid(A,B);
                Refined.Append({{A,AB,C},{AB,B,C}});
            }
            else
            {
                int32 Missing=0; while(Split[Missing]) ++Missing;
                const int32 A=T[(Missing+1)%3],B=T[(Missing+2)%3],C=T[Missing];
                const int32 AB=Mid(A,B),BC=Mid(B,C);
                Refined.Add({AB,B,BC});
                // Choose the shorter quadrilateral diagonal to limit skinny faces.
                if((Result.RestMm[A]-Result.RestMm[BC]).SquaredLength()<=(Result.RestMm[AB]-Result.RestMm[C]).SquaredLength())
                    Refined.Append({{A,AB,BC},{A,BC,C}});
                else Refined.Append({{A,AB,C},{AB,BC,C}});
            }
        }
        for (auto& Edge : Result.BoundaryVertices)
        {
            TArray<int32> RefinedEdge;
            for(int32 K=0; K<Edge.Num()-1; ++K)
            {
                RefinedEdge.Add(Edge[K]);
                if(const int32* Midpoint=Midpoints.Find(EdgeKey(Edge[K],Edge[K+1]))) RefinedEdge.Add(*Midpoint);
            }
            RefinedEdge.Add(Edge.Last());
            Edge=MoveTemp(RefinedEdge);
        }
        Result.Triangles=MoveTemp(Refined);
    }
    double AreaSum=0;
    for (auto T : Result.Triangles)
    {
        double A=Cross(Result.RestMm[T.Y]-Result.RestMm[T.X],Result.RestMm[T.Z]-Result.RestMm[T.X])*0.5;
        if (A<=1.e-10) return Fail(TEXT("Triangulation produced a degenerate face"));
        AreaSum+=A;
    }
    if (!FMath::IsNearlyEqual(AreaSum,ExpectedArea,1.e-6*FMath::Max(1.0,AreaSum)))
        return Fail(TEXT("Triangulation does not preserve panel area"));
    for (auto P : Result.RestMm)
    {
        const FVector3d Placed=Panel.InitialPlacementCm.TransformPosition(FVector(P.X/10,P.Y/10,0));
        if (Placed.ContainsNaN() || Placed.GetAbsMax()>10000)
            return Fail(TEXT("Placed panel exceeds the supported 10000 cm coordinate range"));
        Result.InitialCm.Add(Placed);
    }
    Out=MoveTemp(Result);
    return true;
}
}
