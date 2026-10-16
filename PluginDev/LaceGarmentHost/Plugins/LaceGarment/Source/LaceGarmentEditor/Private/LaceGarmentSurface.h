#pragma once
#include "LaceGarmentPipeline.h"

namespace LaceGarment
{
// Internal inspection of already-validated build topology and matching positions.
double FacePenetration(const FBuildResult& Build,const TArray<FVector3d>& Positions,const TArray<FCapsule>& Capsules,const TArray<FConvexCollider>& Convexes);
void MeasurePanelSurface(FBuildResult& Build);
}
