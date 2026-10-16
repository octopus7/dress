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
bool Validate(const FInput& Input, FString& Error);
bool Run(FString& Error);
}
