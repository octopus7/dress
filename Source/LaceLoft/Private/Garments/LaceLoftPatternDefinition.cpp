#include "Garments/LaceLoftPatternDefinition.h"
#include "Algo/Reverse.h"

double FLaceLoftPatternPath::LengthMm() const
{
    double Length=0;
    for(int32 I=1;I<PointsMm.Num();++I)Length+=FVector2D::Distance(PointsMm[I-1],PointsMm[I]);
    return Length;
}
const FLaceLoftPatternEdge* FLaceLoftPatternPiece::FindEdge(FName Id) const
{ return Edges.FindByPredicate([Id](const auto& Edge){return Edge.EdgeId==Id;}); }
const FLaceLoftPatternPiece* ULaceLoftPatternDefinition::FindPiece(FName Id) const
{ return Pieces.FindByPredicate([Id](const auto& Piece){return Piece.PieceId==Id;}); }
const FLaceLoftSeamOperation* ULaceLoftPatternDefinition::FindSeam(FName Id) const
{ return Seams.FindByPredicate([Id](const auto& Seam){return Seam.SeamId==Id;}); }

bool BuildPatternStitches(const ULaceLoftPatternDefinition& Pattern,TArray<FIntPoint>& Out,TArray<FString>& Errors)
{
    TMap<FName,int32> Offsets;TArray<FVector> Positions;
    for(const auto& Piece:Pattern.Pieces){Offsets.Add(Piece.PieceId,Positions.Num());Positions.Append(Piece.AssembledPositionsMm);}
    Out.Reset();
    const auto Chain=[&](const TArray<FLaceLoftEdgeReference>& Refs,TArray<int32>& Result)
    {
        for(const auto& Ref:Refs)
        {
            const auto* Piece=Pattern.FindPiece(Ref.PieceId);const auto* Edge=Piece?Piece->FindEdge(Ref.EdgeId):nullptr;
            if(!Edge)return false;
            TArray<int32> Indices=Edge->StitchVertices;
            if(Ref.Direction<0)Algo::Reverse(Indices);
            for(int32 I=0;I<Indices.Num();++I)
            {
                if(!Piece->AssembledPositionsMm.IsValidIndex(Indices[I]))return false;
                const int32 Global=Offsets[Ref.PieceId]+Indices[I];
                if(I==0 && !Result.IsEmpty())
                {
                    if(!Positions[Result.Last()].Equals(Positions[Global],.001))return false;
                    continue;
                }
                Result.Add(Global);
            }
        }
        return true;
    };
    for(const auto& Seam:Pattern.Seams)
    {
        if(Seam.Kind==ELaceLoftSeamKind::Hem)continue;
        TArray<int32> A,B;
        if(!Chain(Seam.SideA,A) || !Chain(Seam.SideB,B) || A.Num()!=B.Num() || A.Num()<2)
        {Errors.Add(TEXT("Broken seam chain/count: ")+Seam.SeamId.ToString());continue;}
        bool bSameEdge=false;
        for(const auto& Left:Seam.SideA)for(const auto& Right:Seam.SideB)
            bSameEdge|=Left.PieceId==Right.PieceId && Left.EdgeId==Right.EdgeId;
        if(bSameEdge){Errors.Add(TEXT("Self edge join: ")+Seam.SeamId.ToString());continue;}
        for(int32 I=0;I<A.Num();++I)
        {
            if(!Positions[A[I]].Equals(Positions[B[I]],.001))
                Errors.Add(TEXT("Assembled seam endpoints diverge: ")+Seam.SeamId.ToString());
            Out.Emplace(A[I],B[I]);
        }
    }
    return Errors.IsEmpty();
}

TArray<FString> ValidatePattern(const ULaceLoftPatternDefinition& Pattern)
{
    TArray<FString> Errors;
    const auto Error=[&](const FString& Message){Errors.Add(Message);};
    if(Pattern.PatternId.IsNone() || Pattern.PatternVersion<1 || Pattern.SourceHash.IsEmpty()
        || Pattern.DummyMeasurementsVersion.IsEmpty()) Error(TEXT("Missing pattern identity/version/source."));
    if(Pattern.Pieces.IsEmpty() || Pattern.Seams.IsEmpty()) Error(TEXT("Pattern needs pieces and operations."));
    TSet<FName> PieceIds,PathIds,SeamIds;
    const auto CheckPath=[&](const FLaceLoftPatternPath& Path)
    {
        const FString Id=Path.PathId.ToString();
        if(Path.PathId.IsNone() || PathIds.Contains(Path.PathId)) Error(TEXT("Duplicate/missing PathId: ")+Id);
        PathIds.Add(Path.PathId);
        if(Path.PointsMm.Num()<2 || !FMath::IsFinite(Path.LengthMm()) || Path.LengthMm()<.001)
            Error(TEXT("Empty/invalid path: ")+Id);
        for(int32 I=0;I<Path.PointsMm.Num();++I)
        {
            if(Path.PointsMm[I].ContainsNaN()) Error(TEXT("Nonfinite path: ")+Id);
            if(I && FVector2D::Distance(Path.PointsMm[I-1],Path.PointsMm[I])<.00001)
                Error(TEXT("Degenerate path segment: ")+Id);
        }
        if(Path.bClosed && (Path.PointsMm.Num()<4 ||
            !Path.PointsMm[0].Equals(Path.PointsMm.Last(),.001))) Error(TEXT("Unclosed cut boundary: ")+Id);
    };
    for(const auto& Piece:Pattern.Pieces)
    {
        const FString Id=Piece.PieceId.ToString();
        if(Piece.PieceId.IsNone() || PieceIds.Contains(Piece.PieceId)) Error(TEXT("Duplicate/missing PieceId: ")+Id);
        PieceIds.Add(Piece.PieceId);
        CheckPath(Piece.CutOutline);
        for(const auto& Fold:Piece.FoldLines)CheckPath(Fold);
        if(!Piece.CutOutline.bClosed) Error(TEXT("Cut outline must be closed: ")+Id);
        if(Piece.LayoutMm.ContainsNaN() || Piece.GrainDirection.ContainsNaN() || Piece.GrainDirection.IsNearlyZero())
            Error(TEXT("Invalid piece layout/grain: ")+Id);
        const int32 Count=Piece.SimPositionsMm.Num();
        const bool bArraysValid=Count>=3 && Piece.AssembledPositionsMm.Num()==Count
            && Piece.MaxDistanceMm.Num()==Count && Piece.AnchorBones.Num()==Count;
        if(!bArraysValid) Error(TEXT("Panel vertex arrays disagree: ")+Id);
        else for(int32 I=0;I<Count;++I)
        {
            if(Piece.SimPositionsMm[I].ContainsNaN() || Piece.AssembledPositionsMm[I].ContainsNaN()
                || !FMath::IsFinite(Piece.MaxDistanceMm[I]) || Piece.MaxDistanceMm[I]<0)
                Error(TEXT("Invalid panel vertex: ")+Id);
        }
        if(Piece.TriangleIndices.IsEmpty() || Piece.TriangleIndices.Num()%3) Error(TEXT("Invalid triangle list: ")+Id);
        for(int32 I=0;I+2<Piece.TriangleIndices.Num();I+=3)
        {
            const int32 A=Piece.TriangleIndices[I],B=Piece.TriangleIndices[I+1],C=Piece.TriangleIndices[I+2];
            if(!Piece.SimPositionsMm.IsValidIndex(A) || !Piece.SimPositionsMm.IsValidIndex(B)
                || !Piece.SimPositionsMm.IsValidIndex(C)) { Error(TEXT("Triangle index out of bounds: ")+Id);continue; }
            if(bArraysValid && (FMath::Abs(FVector2D::CrossProduct(Piece.SimPositionsMm[B]-Piece.SimPositionsMm[A],
                Piece.SimPositionsMm[C]-Piece.SimPositionsMm[A]))<.00001 || FVector::CrossProduct(
                Piece.AssembledPositionsMm[B]-Piece.AssembledPositionsMm[A],
                Piece.AssembledPositionsMm[C]-Piece.AssembledPositionsMm[A]).SizeSquared()<.00000001))
                Error(TEXT("Degenerate panel triangle: ")+Id);
        }
        TSet<FName> EdgeIds;
        for(const auto& Edge:Piece.Edges)
        {
            if(Edge.EdgeId.IsNone() || EdgeIds.Contains(Edge.EdgeId)) Error(TEXT("Duplicate/missing EdgeId: ")+Id);
            EdgeIds.Add(Edge.EdgeId);CheckPath(Edge.Path);
            if(FMath::Abs(Edge.SewingDirection)!=1 || !FMath::IsFinite(Edge.AllowanceMm) || Edge.AllowanceMm<0)
                Error(TEXT("Invalid edge direction/allowance: ")+Id);
            if(Edge.StitchVertices.Num()<2) Error(TEXT("Missing edge-to-panel mapping: ")+Id);
            if(Edge.StitchVertices.Num()!=Edge.Path.PointsMm.Num())Error(TEXT("Edge path/mapping count mismatch: ")+Id);
            for(int32 J=0;J<Edge.StitchVertices.Num();++J)
            {
                const int32 Vertex=Edge.StitchVertices[J];
                if(!Piece.SimPositionsMm.IsValidIndex(Vertex)) Error(TEXT("Stitch index out of bounds: ")+Id);
                else if(Edge.Path.PointsMm.IsValidIndex(J) && !Piece.SimPositionsMm[Vertex].Equals(Edge.Path.PointsMm[J],.001))
                    Error(TEXT("UI edge does not follow mapped panel vertices: ")+Id);
            }
        }
    }
    for(const auto& Seam:Pattern.Seams)
    {
        if(Seam.SeamId.IsNone() || SeamIds.Contains(Seam.SeamId)) Error(TEXT("Duplicate/missing SeamId: ")+Seam.SeamId.ToString());
        SeamIds.Add(Seam.SeamId);
    }
    for(const auto& Seam:Pattern.Seams)
    {
        const FString Id=Seam.SeamId.ToString();
        const auto SideLength=[&](const TArray<FLaceLoftEdgeReference>& Side)
        {
            double Length=0;
            TSet<FString> Seen;
            for(const auto& Ref:Side)
            {
                const auto* Piece=Pattern.FindPiece(Ref.PieceId);
                const auto* Edge=Piece?Piece->FindEdge(Ref.EdgeId):nullptr;
                const FString Key=Ref.PieceId.ToString()+TEXT("/")+Ref.EdgeId.ToString();
                if(!Edge) {Error(TEXT("Missing seam edge: ")+Id+TEXT(" ")+Key);continue;}
                if(Seen.Contains(Key)) Error(TEXT("Repeated seam edge: ")+Id);
                Seen.Add(Key);
                if(FMath::Abs(Ref.Direction)!=1 || Ref.Direction!=Edge->SewingDirection)
                    Error(TEXT("Incorrect sewing direction: ")+Id);
                if(Seam.Kind==ELaceLoftSeamKind::Join && Edge->bOpening)
                    Error(TEXT("Join closes an opening: ")+Id);
                Length+=Edge->Path.LengthMm();
            }
            return Length;
        };
        const double A=SideLength(Seam.SideA),B=SideLength(Seam.SideB);
        if(A<.001) Error(TEXT("Empty operation path: ")+Id);
        if(Seam.Kind==ELaceLoftSeamKind::Hem)
        {
            if(!Seam.SideB.IsEmpty()) Error(TEXT("Hem must have only one side: ")+Id);
        }
        else if(B<.001 || !FMath::IsFinite(Seam.EaseRatio) || Seam.EaseRatio<.5 || Seam.EaseRatio>1.25
            || FMath::Abs(B-A*Seam.EaseRatio)>FMath::Max(.25,A*.03))
            Error(TEXT("Undeclared/inconsistent seam length ratio: ")+Id);
        if(Seam.Kind!=ELaceLoftSeamKind::ElasticBand && !FMath::IsNearlyEqual(Seam.EaseRatio,1.,.0001))
            Error(TEXT("Only elastic bands may declare easing: ")+Id);
        for(FName Prerequisite:Seam.Prerequisites)
            if(!SeamIds.Contains(Prerequisite)) Error(TEXT("Missing prerequisite: ")+Id);
    }
    TMap<FName,uint8> Marks;
    TFunction<void(FName)> Visit=[&](FName Id)
    {
        const uint8 Mark=Marks.FindRef(Id);
        if(Mark==2)return;
        if(Mark==1){Error(TEXT("Cyclic seam prerequisite: ")+Id.ToString());return;}
        Marks.Add(Id,1);
        if(const auto* Seam=Pattern.FindSeam(Id))for(FName Before:Seam->Prerequisites)Visit(Before);
        Marks[Id]=2;
    };
    for(const auto& Seam:Pattern.Seams)Visit(Seam.SeamId);
    if(Errors.IsEmpty()){TArray<FIntPoint> Stitches;BuildPatternStitches(Pattern,Stitches,Errors);}
    return Errors;
}
