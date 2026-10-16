#pragma once
#include "CoreMinimal.h"

namespace GarmentFitClothProbe
{
struct FPanel
{
    TArray<FVector2f> Source2D;
    TArray<FVector3f> Initial3D;
    TArray<FIntVector3> Triangles;
};
struct FInput
{
    TArray<FPanel> Panels;
    // Local vertex pairs: panel 0, panel 1. Corresponding edges must already meet.
    TArray<FIntVector2> Seam;
};

// Stable panel-local 2D identity. Sewing refinement appends new vertices without
// changing original indices. Resolve through the native collection after welding.
struct FVertexRef
{
    int32 PanelIndex = INDEX_NONE;
    int32 VertexIndex = INDEX_NONE;
    bool operator==(const FVertexRef& Other) const = default;
};

struct FRunContext
{
    FString OutputDirectory; // Absolute directory within this host's Saved/Probe.
    bool bReopen = false;
};
bool Validate(const FInput& Input, FString& Error);
bool Run(FString& Error);
}
