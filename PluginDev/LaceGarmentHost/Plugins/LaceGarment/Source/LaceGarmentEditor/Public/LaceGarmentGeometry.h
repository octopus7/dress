#pragma once
#include "CoreMinimal.h"

namespace LaceGarment
{
inline constexpr const TCHAR* GeneratorVersion=TEXT("0.3.4-cooked-convex");
struct FPanel
{
    FGuid Id;
    TArray<FVector2d> BoundaryMm;
    FTransform InitialPlacementCm = FTransform::Identity;
    double MaxEdgeMm = 3.0;
    // Empty means one outer loop. Otherwise first index is 0; remaining loops are holes.
    TArray<int32> LoopStarts;
};

struct FPanelMesh
{
    TArray<FVector2d> RestMm;
    TArray<FVector3d> InitialCm;
    TArray<FIntVector3> Triangles;
    // Each original boundary edge retains its ordered generated vertex IDs.
    TArray<TArray<int32>> BoundaryVertices;
};

// Simple outer loop with disjoint interior cutouts; either winding. Output changes only on success.
LACEGARMENTEDITOR_API bool BuildPanel(const FPanel& Panel, FPanelMesh& Out, FString& Error);
}
