#pragma once
#include "Probe.h"
#include "Engine/DataAsset.h"
#include "BodyPinProbe.generated.h"

class UChaosClothAsset;
class UStaticMesh;
struct FManagedArrayCollection;

USTRUCT()
struct FGarmentProbeBodyPin
{
    GENERATED_BODY()
    UPROPERTY() FName Id;
    UPROPERTY() int32 PanelIndex = INDEX_NONE;
    UPROPERTY() int32 VertexIndex = INDEX_NONE;
    UPROPERTY() int32 BodyTriangle = INDEX_NONE;
    UPROPERTY() FVector3f Barycentric = FVector3f(1.f / 3.f);
    UPROPERTY() float SurfaceOffsetCm = 1.5f;
    UPROPERTY() bool bEnabled = true;
};

UCLASS()
class UGarmentProbeBodyPinSet : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY() TSoftObjectPtr<UStaticMesh> BodyMesh;
    UPROPERTY() FTransform BodyTransform = FTransform::Identity;
    UPROPERTY() TArray<FGarmentProbeBodyPin> AuthoredPins;
    UPROPERTY() TArray<FGarmentProbeBodyPin> Pins;
    UPROPERTY() FString SourceCommit;
    UPROPERTY() FString SourceBlob;
    UPROPERTY() FString SnapshotUtc;
};

namespace GarmentFitClothProbe::BodyPins
{
struct FBodySurface
{
    FSoftObjectPath Mesh;
    TArray<FVector3f> Positions;
    TArray<FIntVector3> Triangles;
    TArray<FVector3f> TriangleNormals;
    TArray<FSphere> CollisionSpheres;
};
struct FResolvedPin
{
    FName Id;
    int32 WeldedVertex = INDEX_NONE;
    FVector3f BodyLocalTarget = FVector3f::ZeroVector;
    FVector3f BodyLocalNormal = FVector3f::ZeroVector;
    bool bEnabled = false;
};
bool ResolvePins(const UGarmentProbeBodyPinSet& Set, const FBodySurface& Body,
    const TSharedRef<const FManagedArrayCollection>& Collection, TArray<FResolvedPin>& Out, FString& Error);
bool CreatePin(UGarmentProbeBodyPinSet& Set, const FGarmentProbeBodyPin& Pin, const FBodySurface& Body,
    const TSharedRef<const FManagedArrayCollection>& Collection, FString& Error);
bool SetPinEnabled(UGarmentProbeBodyPinSet& Set, FName Id, bool bEnabled, FString& Error);
bool ReleasePin(UGarmentProbeBodyPinSet& Set, FName Id, FString& Error);
void ResetPins(UGarmentProbeBodyPinSet& Set);
bool ValidateSavedLifecycle(const UGarmentProbeBodyPinSet& Authored, const UGarmentProbeBodyPinSet& Disabled,
    const UGarmentProbeBodyPinSet& Released, const FBodySurface& Body,
    const TSharedRef<const FManagedArrayCollection>& Collection, FString& Error);
bool WorldTarget(const FResolvedPin& Pin, const FTransform& BodyTransform, FVector& Out, FString& Error);
// Input placement is explicitly body-local. Builds a copy; never edits Input.
UChaosClothAsset* BuildPinAsset(UChaosClothAsset* Input, const UGarmentProbeBodyPinSet& Set,
    const FBodySurface& Body, const FString& PackageName, FString& Error);
bool RunExperiment(const FRunContext& Context, FString& Error);
bool RunCombined(UChaosClothAsset* ReadOnlyInput, const FRunContext& Context, FString& Error);
}
