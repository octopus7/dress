#pragma once
#include "Garments/LaceLoftCuttingModel.h"
class ULaceLoftPatternDefinition;

class LACELOFT_API FLaceLoftSewingModel
{
public:
    bool Configure(const ULaceLoftPatternDefinition& Pattern,const TMap<FName,FLaceLoftPathProgress>& Progress);
    bool CanStartSeam(FName SeamId) const;
    bool BeginCut(FName PieceId,FVector2D PiecePointMm);
    bool BeginSeam(FName SeamId,int32 SideAIndex,FVector2D PiecePointMm);
    bool UpdateStroke(FVector2D PiecePointMm) { return Input.UpdateStroke(PiecePointMm); }
    TOptional<FLaceLoftStrokeChange> EndStroke() { return Input.EndStroke(); }
    bool IsPieceCut(FName PieceId) const;
    bool IsSeamComplete(FName SeamId) const;
    bool IsCompleted() const;
    const FLaceLoftPathProgress& Progress(FName Id) const { return Input.Progress(Id); }
    static FName SeamPathId(FName SeamId,int32 SideAIndex);
    void ApplyChange(const FLaceLoftStrokeChange& Change,bool bUndo) { Input.ApplyChange(Change,bUndo); }
private:
    const ULaceLoftPatternDefinition* Definition=nullptr;
    FLaceLoftCuttingModel Input;
};
