#pragma once
#include "LaceGarmentDocument.h"
#include "LaceGarmentPipeline.h"

namespace LaceGarment
{
struct FBoundarySource
{
    FGuid Edge;
    double Parameter=0;
    FGuid Point;
};
struct FSampledDocument
{
    FBuildInput Input;
    // Per-panel provenance for original boundary vertices. Interior vertices are generated later.
    TArray<TArray<FBoundarySource>> BoundarySource;
    TArray<FGuid> SeamIds;
};
// Source is immutable. Failure leaves Out unchanged. Draft geometry is checked here.
LACEGARMENTEDITOR_API bool SampleDocument(const ULaceGarmentDocument& Document, FSampledDocument& Out, FString& Error);
}
