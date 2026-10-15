#pragma once
#include "CoreMinimal.h"
#include "LaceLoftGarmentTypes.generated.h"

UENUM(BlueprintType)
enum class ELaceLoftGarmentSlot : uint8 { Top, Bottom };
UENUM(BlueprintType)
enum class ELaceLoftSeamKind : uint8 { Join, Hem, Binding, ElasticBand };

USTRUCT(BlueprintType)
struct LACELOFT_API FLaceLoftPatternPath
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName PathId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FVector2D> PointsMm;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) bool bClosed=false;
    double LengthMm() const;
};

USTRUCT(BlueprintType)
struct LACELOFT_API FLaceLoftPatternEdge
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName EdgeId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FLaceLoftPatternPath Path;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) bool bOpening=false;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 SewingDirection=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) double AllowanceMm=3;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<int32> StitchVertices;
};

USTRUCT(BlueprintType)
struct LACELOFT_API FLaceLoftPatternPiece
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName PieceId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D LayoutMm=FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector2D GrainDirection=FVector2D(0,1);
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FLaceLoftPatternPath CutOutline;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FLaceLoftPatternEdge> Edges;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FVector2D> NotchesMm;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FLaceLoftPatternPath> FoldLines;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FVector2D> SimPositionsMm;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FVector> AssembledPositionsMm;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<int32> TriangleIndices;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<float> MaxDistanceMm;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FName> AnchorBones;
    const FLaceLoftPatternEdge* FindEdge(FName Id) const;
};

USTRUCT(BlueprintType)
struct LACELOFT_API FLaceLoftEdgeReference
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName PieceId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName EdgeId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 Direction=1;
};

USTRUCT(BlueprintType)
struct LACELOFT_API FLaceLoftSeamOperation
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName SeamId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) ELaceLoftSeamKind Kind=ELaceLoftSeamKind::Join;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FLaceLoftEdgeReference> SideA;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FLaceLoftEdgeReference> SideB;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) double EaseRatio=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FName> Prerequisites;
};

namespace LaceLoftUnits
{
    inline FVector ToCm(const FVector& Mm) { return Mm/10.; }
    inline FVector2D ToCm(const FVector2D& Mm) { return Mm/10.; }
    inline double ToCm(double Mm) { return Mm/10.; }
}
