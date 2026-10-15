#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LaceGarmentDocument.generated.h"

UENUM(BlueprintType)
enum class ELGCurveType : uint8 { Line, Cubic };
UENUM(BlueprintType)
enum class ELGBoundaryType : uint8 { Open, Stitch, Fold, Cut };
UENUM(BlueprintType)
enum class ELGSeamType : uint8 { Join, Hem, Binding };
UENUM(BlueprintType)
enum class ELGCorrectionState : uint8 { Resolved, PendingRegeneration, Unresolved };

USTRUCT(BlueprintType)
struct FLGControlPoint
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FVector2D PositionMm=FVector2D::ZeroVector;
};
USTRUCT(BlueprintType)
struct FLGEdge
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Start;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid End;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid ControlA;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid ControlB;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") ELGCurveType Curve=ELGCurveType::Line;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") ELGBoundaryType Kind=ELGBoundaryType::Open;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double SeamAllowanceMm=0;
};
USTRUCT(BlueprintType)
struct FLGLoop
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FGuid> Edges;
};
USTRUCT(BlueprintType)
struct FLGNotch
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Edge;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid MatchingNotch;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double Parameter=0.5;
};
USTRUCT(BlueprintType)
struct FLGPiece
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FString Name;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGControlPoint> Points;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGEdge> Edges;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FLGLoop Outer;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGLoop> Holes;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGNotch> Notches;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FVector2D Grain=FVector2D(0,1);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid MirrorOf;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FTransform InitialPlacementCm=FTransform::Identity;
};
USTRUCT(BlueprintType)
struct FLGEdgeInterval
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Piece;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Edge;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double From=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double To=1;
};
USTRUCT(BlueprintType)
struct FLGSeam
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FLGEdgeInterval A;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FLGEdgeInterval B;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") ELGSeamType Type=ELGSeamType::Join;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") bool ReverseB=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double EaseRatio=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double LengthToleranceMm=0.1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double FoldWidthMm=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FGuid> Prerequisites;
};
USTRUCT(BlueprintType)
struct FLGAnchor
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Piece;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Point;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") bool bUseTarget=false;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FVector TargetCm=FVector::ZeroVector;
};
USTRUCT(BlueprintType)
struct FLGJointWeight
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Joint;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double Weight=0;
};
USTRUCT(BlueprintType)
struct FLGCorrection
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Id;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid Target;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FString Domain=TEXT("simulationVertex");
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FString TopologyHash;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGJointWeight> Skin;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FString Property;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double Value=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="LaceGarment") ELGCorrectionState State=ELGCorrectionState::PendingRegeneration;
};

UCLASS(BlueprintType)
class LACEGARMENTEDITOR_API ULaceGarmentDocument : public UObject
{
    GENERATED_BODY()
public:
    ULaceGarmentDocument();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid GarmentId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FGuid PatternId;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FString Name;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FString TemplateName;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") FString TemplateVersion;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TMap<FString,double> TemplateParametersMm;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGPiece> Pieces;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGSeam> Seams;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGAnchor> Anchors;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") TArray<FLGCorrection> Corrections;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double BoundarySpacingMm=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="LaceGarment") double MeshMaxEdgeMm=3;
    UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="LaceGarment") FString GeneratedTopologyHash;
    UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="LaceGarment") bool bDerivedCurrent=false;

    bool ValidateStructure(FString& Error) const;
    bool EvaluateEdge(const FGuid& Piece,const FGuid& Edge,double Parameter,FVector2D& Out,FString& Error) const;
    bool MovePoint(const FGuid& Piece,const FGuid& Point,FVector2D PositionMm,FString& Error);
    void InvalidateDerived();
    bool ApplyGeneratedTopology(const FString& Hash,FString& Error);
    bool HasUnresolvedCorrections() const;
    virtual void PostEditUndo() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
};
