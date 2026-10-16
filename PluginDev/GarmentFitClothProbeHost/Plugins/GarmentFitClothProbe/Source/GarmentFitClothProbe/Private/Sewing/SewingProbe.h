#pragma once
#include "Probe.h"

class UChaosClothAsset;

namespace GarmentFitClothProbe::Sewing
{
enum class ELengthPolicy : uint8 { EqualOnly, NormalizedArcLength };

struct FBoundary
{
    int32 PanelIndex = INDEX_NONE;
    TArray<int32> Vertices; // Ordered, consecutive boundary-edge vertices in original Source2D.
    bool bReverse = false;
};

struct FRequest
{
    FBoundary A;
    FBoundary B;
    bool bFlipSecond = false;
    ELengthPolicy LengthPolicy = ELengthPolicy::EqualOnly;
    double EqualLengthToleranceCm = 1.e-4;
};

struct FPreparedSeam
{
    // Original Source2D indices/values are unchanged. Only inserted boundary knots
    // are appended; triangles are split. Initial3D is an explicit weld preparation.
    FInput Input;
    TArray<double> NormalizedKnots;
    double OriginalLengthA = 0;
    double OriginalLengthB = 0;
    double LengthDifferenceBMinusA = 0;
    double LengthRatioBOverA = 0;
    double MaxPreparationDisplacementCm = 0;
};

// This isolated two-panel experiment requires A on panel 0 and B on panel 1.
// Both input boundary directions and the optional B flip are honored.
bool PrepareSeam(const FInput& Source, const FRequest& Request, FPreparedSeam& Out, FString& Error);
bool ValidateCorrespondence(const FInput& Input, TConstArrayView<FIntVector2> Pairs, FString& Error);
bool RunExperiment(const FRunContext& Context, FString& Error);
UChaosClothAsset* CreateIntegrationAsset(const FRunContext& Context, FString& Error);
}
