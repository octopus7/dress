#include "Probe.h"

namespace GarmentFitClothProbe
{
static uint64 EdgeKey(int32 A, int32 B)
{
    return (uint64(FMath::Min(A, B)) << 32) | uint32(FMath::Max(A, B));
}

bool Validate(const FInput& Input, FString& Error)
{
    Error.Reset();
    auto Fail = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    if (Input.Panels.Num() != 2) return Fail(TEXT("Exactly two panels required"));
    TMap<uint64, int32> Edges[2];
    for (int32 P = 0; P < 2; ++P)
    {
        const FPanel& Panel = Input.Panels[P];
        if (Panel.Source2D.Num() < 3 || Panel.Source2D.Num() != Panel.Initial3D.Num() || Panel.Triangles.IsEmpty())
            return Fail(TEXT("Missing geometry or mismatched 2D/3D vertex counts"));
        for (int32 I = 0; I < Panel.Source2D.Num(); ++I)
            if (Panel.Source2D[I].ContainsNaN() || Panel.Initial3D[I].ContainsNaN())
                return Fail(TEXT("Non-finite coordinate"));
        for (const FIntVector3& T : Panel.Triangles)
        {
            for (int32 K = 0; K < 3; ++K)
                if (!Panel.Source2D.IsValidIndex(T[K])) return Fail(TEXT("Invalid triangle index"));
            const FVector2f A = Panel.Source2D[T.Y] - Panel.Source2D[T.X];
            const FVector2f B = Panel.Source2D[T.Z] - Panel.Source2D[T.X];
            if (FMath::Abs(A.X * B.Y - A.Y * B.X) < 1.e-6f ||
                FVector3f::CrossProduct(Panel.Initial3D[T.Y] - Panel.Initial3D[T.X],
                    Panel.Initial3D[T.Z] - Panel.Initial3D[T.X]).SizeSquared() < 1.e-12f)
                return Fail(TEXT("Degenerate triangle"));
            for (int32 K = 0; K < 3; ++K)
                if (++Edges[P].FindOrAdd(EdgeKey(T[K], T[(K + 1) % 3])) > 2)
                    return Fail(TEXT("Non-manifold edge"));
        }
    }
    if (Input.Seam.Num() < 2) return Fail(TEXT("A seam needs at least two vertex pairs"));
    TSet<int32> Used[2];
    for (int32 I = 0; I < Input.Seam.Num(); ++I)
    {
        const FIntVector2 Pair = Input.Seam[I];
        for (int32 P = 0; P < 2; ++P)
        {
            if (!Input.Panels[P].Initial3D.IsValidIndex(Pair[P])) return Fail(TEXT("Invalid seam index"));
            if (Used[P].Contains(Pair[P])) return Fail(TEXT("Duplicate seam correspondence"));
            Used[P].Add(Pair[P]);
            if (I > 0 && Edges[P].FindRef(EdgeKey(Input.Seam[I - 1][P], Pair[P])) != 1)
                return Fail(TEXT("Seam must follow consecutive boundary edges"));
        }
        if (!Input.Panels[0].Initial3D[Pair.X].Equals(Input.Panels[1].Initial3D[Pair.Y], 1.e-3f))
            return Fail(TEXT("Seam endpoints must meet in the initial placement"));
    }
    return true;
}
}
