#pragma once
#include "LaceGarmentGeometry.h"
#include "UObject/SoftObjectPath.h"

class UChaosClothAsset;
class UMaterialInterface;
class USkeletalMesh;
struct FManagedArrayCollection;
struct FKConvexElem;

namespace LaceGarment
{
struct FSeam
{
    int32 PanelA = 0, EdgeA = 0, PanelB = 1, EdgeB = 0;
    bool bReverseB = true;
    // Optional contiguous chains of original boundary edges, in panel winding order.
    TArray<int32> BoundaryEdgesA, BoundaryEdgesB;
    // LengthA = EaseRatio * LengthB, within LengthToleranceMm.
    double EaseRatio = 1;
    double LengthToleranceMm = 0.01;
};
struct FCapsule
{
    FVector3d A = FVector3d::ZeroVector, B = FVector3d(0,0,1);
    double RadiusCm = 1;
};
struct FConvexShapeData;
struct LACEGARMENTEDITOR_API FConvexCollider
{
    bool IsValid() const { return Shape.IsValid(); }
    const TArray<FVector3d>& GetVertices() const { return VerticesCm; }
    const TArray<FPlane4d>& GetPlanes() const { return PlanesCm; }
    const FBox3d& GetBounds() const { return Bounds; }
    double SignedDistance(FVector3d Position,FVector3d& Normal) const;
    bool LineInterval(FVector3d Origin,FVector3d Direction,double Thickness,FVector2d& Interval,FVector3d& EntryNormal) const;
    double TrianglePenetration(FVector3d A,FVector3d B,FVector3d C) const;
private:
    friend LACEGARMENTEDITOR_API bool BuildConvexCollider(TConstArrayView<FVector3d>,FConvexCollider&,FString&);
    friend bool ReadPhysicsConvex(const FKConvexElem&,const FTransform&,FConvexCollider&,FString&);
    TArray<FVector3d> VerticesCm;
    // Unit outward normals; inside satisfies N dot P <= W.
    TArray<FPlane4d> PlanesCm;
    FBox3d Bounds=FBox3d(ForceInit);
    TSharedPtr<const FConvexShapeData> Shape;
};
struct FPin
{
    int32 Panel = 0;
    int32 BoundaryPoint = 0;
    TOptional<FVector3d> TargetCm;
};
enum class EBodySkinTransfer : uint8
{
    SurfaceVertex,
    BarycentricStrict
};
struct FBuildResult;
struct FBuildInput
{
    TArray<FPanel> Panels;
    TArray<FSeam> Seams;
    TArray<FCapsule> Colliders;
    TArray<FConvexCollider> ConvexColliders;
    TArray<FPin> Pins;
    int32 MaxSteps = 4800;
    int32 Iterations = 100;
    double TimeStep = 1.0/480.0;
    double GravityCm = -980;
    double ThicknessCm = 0.03;
    // Opposite-vertex regularization, not a calibrated material stiffness.
    double BendingStrength = 0.05;
    TFunction<void(const FBuildResult&)> Progress;
};
struct FBuildResult
{
    TArray<FPanelMesh> Panels;
    TArray<int32> Offsets;
    TArray<FVector3d> InitialCm, SettledCm;
    TArray<FVector2f> UV;
    TArray<FIntVector2> Stitches;
    TSet<int32> PinnedVertices;
    TArray<TArray<int32>> SkinJoints;
    TArray<TArray<float>> SkinWeights;
    // Optional source-vertex map in cm. Stitched members must agree.
    TArray<float> MaxDistancesCm;
    FSoftObjectPath ReferenceMesh;
    EBodySkinTransfer SkinTransfer=EBodySkinTransfer::BarycentricStrict;
    double MaxSeamGapCm = 0;
    double MaxPenetrationCm = 0;
    double MaxRelativeEdgeError = 0;
    double MaxFacePenetrationCm = 0;
    double MaxInternalFoldDegrees = 0;
    double MinAreaRatio = 1, MaxAreaRatio = 1;
    double MaxSpeedCmPerSec = 0;
    double TexelsPerCm = 0;
    int32 Steps = 0;
    bool bConverged = false;
    bool bSurfaceMeasured = false;
};

// Native editor-only constrained relaxation. No external process or solver.
// Failure retains diagnostics in Out, but must not produce a distributable asset.
LACEGARMENTEDITOR_API bool BuildGarment(const FBuildInput& Input, FBuildResult& Out, FString& Error);
// Reference-pose body collision in mesh component-space centimetres. Atomic on failure.
LACEGARMENTEDITOR_API bool ReadBodyCollision(USkeletalMesh* Body,TArray<FCapsule>& Out,FString& Error);
LACEGARMENTEDITOR_API bool ReadBodyCollision(USkeletalMesh* Body,TArray<FCapsule>& Out,TArray<FConvexCollider>& Convexes,FString& Error);
LACEGARMENTEDITOR_API bool BuildConvexCollider(TConstArrayView<FVector3d> Points,FConvexCollider& Out,FString& Error);
LACEGARMENTEDITOR_API bool TransferBodySkin(FBuildResult& Build, USkeletalMesh* Body, FString& Error, EBodySkinTransfer Method=EBodySkinTransfer::BarycentricStrict);
LACEGARMENTEDITOR_API TSharedPtr<FManagedArrayCollection> MakeClothCollection(const FBuildResult& Build, UMaterialInterface* Material, FString& Error);
LACEGARMENTEDITOR_API UChaosClothAsset* SaveClothAsset(const FBuildResult& Build, UMaterialInterface* Material, const FString& PackageName, FString& Error);
}
