#pragma once
#include "LaceGarmentSampling.h"

namespace LaceGarment
{
struct FGeneratedCorrespondence
{
    FString TopologyHash;
    // One ID per source simulation vertex; stitched/welded members share an ID.
    TArray<FGuid> SimulationVertexIds;
    TArray<FGuid> JointIds;
};
LACEGARMENTEDITOR_API bool MakeGeneratedCorrespondence(const ULaceGarmentDocument& Document,const FBuildResult& Build,FGeneratedCorrespondence& Out,FString& Error);
// Reapply source corrections after automatic skin. Failure preserves Build and
// keeps unsupported, missing-target and changed-topology corrections unresolved.
LACEGARMENTEDITOR_API bool ApplyDocumentCorrections(ULaceGarmentDocument& Document,FBuildResult& Build,FString& Error);
}
