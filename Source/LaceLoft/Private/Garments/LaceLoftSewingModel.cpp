#include "Garments/LaceLoftSewingModel.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "Algo/Reverse.h"

bool FLaceLoftSewingModel::Configure(const ULaceLoftPatternDefinition& Pattern,const TMap<FName,FLaceLoftPathProgress>& Saved)
{
    Definition=nullptr;
    if(!ValidatePattern(Pattern).IsEmpty())return false;
    TArray<FLaceLoftPatternPath> Paths;
    for(const auto& Piece:Pattern.Pieces)Paths.Add(Piece.CutOutline);
    for(const auto& Seam:Pattern.Seams)for(int32 I=0;I<Seam.SideA.Num();++I)
    {
        const auto& Ref=Seam.SideA[I];auto Path=Pattern.FindPiece(Ref.PieceId)->FindEdge(Ref.EdgeId)->Path;
        Path.PathId=SeamPathId(Seam.SeamId,I);
        if(Ref.Direction<0)Algo::Reverse(Path.PointsMm);
        Paths.Add(MoveTemp(Path));
    }
    Input.SetPaths(Paths);
    for(const auto& Entry:Saved)if(!Input.RestoreProgress(Entry.Key,Entry.Value))return false;
    Definition=&Pattern;
    bool bAllCut=true;for(const auto& Piece:Pattern.Pieces)bAllCut&=IsPieceCut(Piece.PieceId);
    for(int32 Pass=0;Pass<Pattern.Seams.Num();++Pass)for(const auto& Seam:Pattern.Seams)
    {
        bool bAllowed=bAllCut;
        for(FName Before:Seam.Prerequisites)bAllowed&=IsSeamComplete(Before);
        if(!bAllowed)for(int32 I=0;I<Seam.SideA.Num();++I)Input.RestoreProgress(SeamPathId(Seam.SeamId,I),{});
    }
    return true;
}
bool FLaceLoftSewingModel::CanStartSeam(FName Id) const
{
    const auto* Seam=Definition?Definition->FindSeam(Id):nullptr;
    if(!Seam || IsSeamComplete(Id))return false;
    for(const auto& Piece:Definition->Pieces)if(!IsPieceCut(Piece.PieceId))return false;
    for(FName Before:Seam->Prerequisites)if(!IsSeamComplete(Before))return false;
    return true;
}
bool FLaceLoftSewingModel::BeginCut(FName Id,FVector2D Point)
{
    const auto* Piece=Definition?Definition->FindPiece(Id):nullptr;
    return Piece && !IsPieceCut(Id) && Input.BeginStroke(Piece->CutOutline.PathId,Point);
}
bool FLaceLoftSewingModel::BeginSeam(FName Id,int32 Index,FVector2D Point)
{
    if(!CanStartSeam(Id) || !Definition->FindSeam(Id)->SideA.IsValidIndex(Index))return false;
    const FName PathId=SeamPathId(Id,Index);
    return !Input.IsComplete(PathId) && Input.BeginStroke(PathId,Point,true);
}
bool FLaceLoftSewingModel::IsPieceCut(FName Id) const
{const auto* Piece=Definition?Definition->FindPiece(Id):nullptr;return Piece && Input.IsComplete(Piece->CutOutline.PathId);}
bool FLaceLoftSewingModel::IsSeamComplete(FName Id) const
{
    const auto* Seam=Definition?Definition->FindSeam(Id):nullptr;if(!Seam)return false;
    for(int32 I=0;I<Seam->SideA.Num();++I)if(!Input.IsComplete(SeamPathId(Id,I)))return false;
    return !Seam->SideA.IsEmpty();
}
bool FLaceLoftSewingModel::IsCompleted() const
{
    if(!Definition)return false;
    for(const auto& Piece:Definition->Pieces)if(!IsPieceCut(Piece.PieceId))return false;
    for(const auto& Seam:Definition->Seams)if(!IsSeamComplete(Seam.SeamId))return false;
    return true;
}
FName FLaceLoftSewingModel::SeamPathId(FName Id,int32 Index)
{return FName(FString::Printf(TEXT("seam.%s.%d"),*Id.ToString(),Index));}
