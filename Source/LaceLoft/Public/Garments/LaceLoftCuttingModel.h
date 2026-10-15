#pragma once
#include "CoreMinimal.h"
#include "Garments/LaceLoftGarmentTypes.h"
#include "LaceLoftCuttingModel.generated.h"

USTRUCT()
struct LACELOFT_API FLaceLoftInterval
{
    GENERATED_BODY()
    UPROPERTY() double StartMm=0;
    UPROPERTY() double EndMm=0;
    FLaceLoftInterval()=default;
    FLaceLoftInterval(double A,double B):StartMm(A),EndMm(B){}
};

USTRUCT()
struct LACELOFT_API FLaceLoftPathProgress
{
    GENERATED_BODY()
    UPROPERTY() TArray<FLaceLoftInterval> Intervals;
    double CoveredMm() const;
};

struct LACELOFT_API FLaceLoftStrokeChange
{
    FName PathId;
    FLaceLoftPathProgress Before,After;
};

// Pure input geometry, shared by cutting and directed sewing. No UObject/UI state.
class LACELOFT_API FLaceLoftCuttingModel
{
public:
    void SetPaths(const TArray<FLaceLoftPatternPath>& InPaths);
    bool BeginStroke(FName PathId,FVector2D PointMm,bool bForwardOnly=false);
    bool UpdateStroke(FVector2D PointMm);
    TOptional<FLaceLoftStrokeChange> EndStroke();
    bool IsTracking() const { return bTracking; }
    const FLaceLoftPathProgress& Progress(FName PathId) const;
    bool IsComplete(FName PathId) const;
    void ApplyChange(const FLaceLoftStrokeChange& Change,bool bUndo);
    bool RestoreProgress(FName PathId,const FLaceLoftPathProgress& Value);
    static FVector2D ScreenToMm(FVector2D Screen,FVector2D Origin,double Dpi,double Zoom,FVector2D Pan,double PixelsPerMm);
    static FVector2D LocalToMm(FVector2D Local,double Zoom,FVector2D Pan,double PixelsPerMm);
private:
    TMap<FName,FLaceLoftPatternPath> Paths;
    TMap<FName,FLaceLoftPathProgress> Coverage;
    FName ActivePath;
    FLaceLoftPathProgress Before;
    FVector2D PreviousPoint=FVector2D::ZeroVector;
    double PreviousArc=0;
    bool bTracking=false,bForward=false;
};
