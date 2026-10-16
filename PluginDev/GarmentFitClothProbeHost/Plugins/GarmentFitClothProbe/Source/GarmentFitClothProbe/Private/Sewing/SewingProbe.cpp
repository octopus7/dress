#include "Sewing/SewingProbe.h"
#include "Algo/Reverse.h"

namespace GarmentFitClothProbe::Sewing
{
namespace
{
constexpr double KnotTolerance = 1.e-9;
uint64 EdgeKey(int32 A, int32 B)
{
    return (uint64(FMath::Min(A, B)) << 32) | uint32(FMath::Max(A, B));
}
bool Fail(FString& Error, const TCHAR* Message) { Error = Message; return false; }

bool ValidatePanels(const FInput& Input, TMap<uint64, int32> (&Edges)[2], FString& Error)
{
    if (Input.Panels.Num() != 2) return Fail(Error, TEXT("Sewing requires exactly two panels"));
    for (int32 P = 0; P < 2; ++P)
    {
        const FPanel& Panel = Input.Panels[P];
        if (Panel.Source2D.Num() < 3 || Panel.Initial3D.Num() != Panel.Source2D.Num() || Panel.Triangles.IsEmpty())
            return Fail(Error, TEXT("Missing or mismatched panel geometry"));
        for (int32 I = 0; I < Panel.Source2D.Num(); ++I)
            if (Panel.Source2D[I].ContainsNaN() || Panel.Initial3D[I].ContainsNaN())
                return Fail(Error, TEXT("Non-finite source or placement coordinate"));
        for (const FIntVector3& T : Panel.Triangles)
        {
            for (int32 K = 0; K < 3; ++K)
                if (!Panel.Source2D.IsValidIndex(T[K])) return Fail(Error, TEXT("Out-of-range triangle vertex"));
            const FVector2D A = FVector2D(Panel.Source2D[T.Y]) - FVector2D(Panel.Source2D[T.X]);
            const FVector2D B = FVector2D(Panel.Source2D[T.Z]) - FVector2D(Panel.Source2D[T.X]);
            const double Cross = A.X * B.Y - A.Y * B.X;
            if (!FMath::IsFinite(Cross) || FMath::Abs(Cross) < 1.e-8)
                return Fail(Error, TEXT("Degenerate source triangle"));
            for (int32 K = 0; K < 3; ++K)
                if (++Edges[P].FindOrAdd(EdgeKey(T[K], T[(K + 1) % 3])) > 2)
                    return Fail(Error, TEXT("Non-manifold panel boundary"));
        }
    }
    return true;
}

struct FArcBoundary
{
    TArray<int32> Vertices;
    TArray<double> Knots;
    double Length = 0;
};
bool MeasureBoundary(const FPanel& Panel, const TMap<uint64, int32>& Edges,
    const FBoundary& Boundary, bool bExtraFlip, FArcBoundary& Out, FString& Error)
{
    Out.Vertices = Boundary.Vertices;
    if (Boundary.bReverse != bExtraFlip) Algo::Reverse(Out.Vertices);
    if (Out.Vertices.Num() < 2) return Fail(Error, TEXT("Boundary requires at least two vertices"));
    TSet<int32> Used;
    Out.Knots.Add(0.);
    for (int32 I = 0; I < Out.Vertices.Num(); ++I)
    {
        const int32 V = Out.Vertices[I];
        if (!Panel.Source2D.IsValidIndex(V)) return Fail(Error, TEXT("Out-of-range boundary vertex"));
        if (Used.Contains(V)) return Fail(Error, TEXT("Duplicate boundary vertex or overlapping traversal"));
        Used.Add(V);
        if (I == 0) continue;
        const int32 Previous = Out.Vertices[I - 1];
        if (Edges.FindRef(EdgeKey(Previous, V)) != 1)
            return Fail(Error, TEXT("Boundary must follow consecutive outer triangle edges"));
        const double Segment = (FVector2D(Panel.Source2D[V]) - FVector2D(Panel.Source2D[Previous])).Size();
        if (!FMath::IsFinite(Segment) || Segment <= 1.e-7) return Fail(Error, TEXT("Zero or non-finite boundary segment"));
        Out.Length += Segment;
        Out.Knots.Add(Out.Length);
    }
    if (!FMath::IsFinite(Out.Length) || Out.Length <= 1.e-7) return Fail(Error, TEXT("Zero or non-finite boundary length"));
    for (double& Knot : Out.Knots) Knot /= Out.Length;
    for (int32 I = 1; I < Out.Knots.Num(); ++I)
        if (Out.Knots[I] - Out.Knots[I - 1] <= KnotTolerance * 2)
            return Fail(Error, TEXT("Boundary segments are below normalized sampling precision"));
    return true;
}

// Split one original boundary edge into a fan while retaining the triangle's
// winding. A later adjacent edge can find and split the resulting triangle.
bool SplitBoundaryEdge(FPanel& Panel, int32 A, int32 B, const TArray<int32>& Interior, FString& Error)
{
    if (Interior.IsEmpty()) return true;
    for (int32 Face = 0; Face < Panel.Triangles.Num(); ++Face)
    {
        const FIntVector3 T = Panel.Triangles[Face];
        for (int32 Edge = 0; Edge < 3; ++Edge)
        {
            const int32 Start = T[Edge], End = T[(Edge + 1) % 3], Opposite = T[(Edge + 2) % 3];
            if (EdgeKey(Start, End) != EdgeKey(A, B)) continue;
            TArray<int32> Chain = Interior;
            if (Start != A) Algo::Reverse(Chain);
            Chain.Insert(Start, 0);
            Chain.Add(End);
            Panel.Triangles[Face] = FIntVector3(Chain[0], Chain[1], Opposite);
            for (int32 I = 1; I + 1 < Chain.Num(); ++I)
                Panel.Triangles.Emplace(Chain[I], Chain[I + 1], Opposite);
            return true;
        }
    }
    return Fail(Error, TEXT("Refinement could not find a boundary triangle"));
}

bool RefineBoundary(FPanel& Panel, const FArcBoundary& Arc, const TArray<double>& Knots,
    TArray<int32>& OutIndices, FString& Error)
{
    TArray<TArray<int32>> Inserted;
    Inserted.SetNum(Arc.Vertices.Num() - 1);
    int32 Segment = 0;
    for (const double Knot : Knots)
    {
        while (Segment + 1 < Arc.Knots.Num() - 1 && Knot > Arc.Knots[Segment + 1] + KnotTolerance) ++Segment;
        if (FMath::Abs(Knot - Arc.Knots[Segment]) <= KnotTolerance)
        { OutIndices.Add(Arc.Vertices[Segment]); continue; }
        if (FMath::Abs(Knot - Arc.Knots[Segment + 1]) <= KnotTolerance)
        { OutIndices.Add(Arc.Vertices[Segment + 1]); continue; }
        const double Alpha = (Knot - Arc.Knots[Segment]) / (Arc.Knots[Segment + 1] - Arc.Knots[Segment]);
        if (Alpha <= 0. || Alpha >= 1.) return Fail(Error, TEXT("Invalid arc interpolation interval"));
        const int32 A = Arc.Vertices[Segment], B = Arc.Vertices[Segment + 1];
        const int32 Index = Panel.Source2D.Add(FVector2f(FMath::Lerp(FVector2D(Panel.Source2D[A]), FVector2D(Panel.Source2D[B]), Alpha)));
        Panel.Initial3D.Add(FVector3f(FMath::Lerp(FVector(Panel.Initial3D[A]), FVector(Panel.Initial3D[B]), Alpha)));
        Inserted[Segment].Add(Index);
        OutIndices.Add(Index);
    }
    for (int32 I = 0; I < Inserted.Num(); ++I)
        if (!SplitBoundaryEdge(Panel, Arc.Vertices[I], Arc.Vertices[I + 1], Inserted[I], Error)) return false;
    return true;
}
}

bool ValidateCorrespondence(const FInput& Input, TConstArrayView<FIntVector2> Pairs, FString& Error)
{
    Error.Reset();
    TMap<uint64, int32> Edges[2];
    if (!ValidatePanels(Input, Edges, Error)) return false;
    if (Pairs.Num() < 2) return Fail(Error, TEXT("A seam requires at least two correspondences"));
    TSet<int32> Used[2];
    for (int32 I = 0; I < Pairs.Num(); ++I)
        for (int32 P = 0; P < 2; ++P)
        {
            const int32 V = Pairs[I][P];
            if (!Input.Panels[P].Source2D.IsValidIndex(V)) return Fail(Error, TEXT("Out-of-range correspondence"));
            if (Used[P].Contains(V)) return Fail(Error, TEXT("Duplicate or conflicting correspondence"));
            Used[P].Add(V);
            if (I > 0 && Edges[P].FindRef(EdgeKey(Pairs[I - 1][P], V)) != 1)
                return Fail(Error, TEXT("Correspondence does not follow a refined boundary"));
        }
    return true;
}

bool PrepareSeam(const FInput& Source, const FRequest& Request, FPreparedSeam& Out, FString& Error)
{
    Out = {}; Error.Reset();
    if (!Source.Seam.IsEmpty()) return Fail(Error, TEXT("Input already contains a seam; overlapping requests must be resolved explicitly"));
    if (Request.A.PanelIndex != 0 || Request.B.PanelIndex != 1)
        return Fail(Error, TEXT("This two-panel experiment requires A on panel 0 and B on panel 1"));
    if (!FMath::IsFinite(Request.EqualLengthToleranceCm) || Request.EqualLengthToleranceCm < 0.)
        return Fail(Error, TEXT("Length tolerance must be finite and nonnegative"));
    if (Request.LengthPolicy != ELengthPolicy::EqualOnly && Request.LengthPolicy != ELengthPolicy::NormalizedArcLength)
        return Fail(Error, TEXT("Unknown mismatch policy"));
    TMap<uint64, int32> Edges[2];
    if (!ValidatePanels(Source, Edges, Error)) return false;
    FArcBoundary Arcs[2];
    if (!MeasureBoundary(Source.Panels[0], Edges[0], Request.A, false, Arcs[0], Error) ||
        !MeasureBoundary(Source.Panels[1], Edges[1], Request.B, Request.bFlipSecond, Arcs[1], Error)) return false;
    FPreparedSeam Prepared;
    Prepared.OriginalLengthA = Arcs[0].Length;
    Prepared.OriginalLengthB = Arcs[1].Length;
    Prepared.LengthDifferenceBMinusA = Arcs[1].Length - Arcs[0].Length;
    Prepared.LengthRatioBOverA = Arcs[1].Length / Arcs[0].Length;
    if (Request.LengthPolicy == ELengthPolicy::EqualOnly && FMath::Abs(Prepared.LengthDifferenceBMinusA) > Request.EqualLengthToleranceCm)
    {
        Error = FString::Printf(TEXT("EqualOnly rejects original 2D lengths %.9g and %.9g cm (B/A %.9g)"),
            Prepared.OriginalLengthA, Prepared.OriginalLengthB, Prepared.LengthRatioBOverA);
        return false;
    }
    TArray<double> AllKnots = Arcs[0].Knots;
    AllKnots.Append(Arcs[1].Knots);
    AllKnots.Sort();
    for (const double Knot : AllKnots)
        if (Prepared.NormalizedKnots.IsEmpty() || Knot - Prepared.NormalizedKnots.Last() > KnotTolerance)
            Prepared.NormalizedKnots.Add(Knot);
    Prepared.Input = Source;
    TArray<int32> Indices[2];
    for (int32 P = 0; P < 2; ++P)
        if (!RefineBoundary(Prepared.Input.Panels[P], Arcs[P], Prepared.NormalizedKnots, Indices[P], Error)) return false;
    for (int32 I = 0; I < Prepared.NormalizedKnots.Num(); ++I)
    {
        const FVector3f Target = Prepared.Input.Panels[0].Initial3D[Indices[0][I]];
        FVector3f& B = Prepared.Input.Panels[1].Initial3D[Indices[1][I]];
        Prepared.MaxPreparationDisplacementCm = FMath::Max(Prepared.MaxPreparationDisplacementCm, double((B - Target).Size()));
        B = Target;
        Prepared.Input.Seam.Emplace(Indices[0][I], Indices[1][I]);
    }
    if (!ValidateCorrespondence(Prepared.Input, Prepared.Input.Seam, Error) || !GarmentFitClothProbe::Validate(Prepared.Input, Error)) return false;
    Out = MoveTemp(Prepared);
    return true;
}
}
