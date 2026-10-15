#include "LaceGarmentDocument.h"

namespace
{
bool ValidPoint(FVector2D P)
{
    return !P.ContainsNaN() && FMath::Abs(P.X)<=10000 && FMath::Abs(P.Y)<=10000;
}
bool ValidHash(const FString& Hash)
{
    if(Hash.Len()!=64) return false;
    for(TCHAR C : Hash) if(!((C>=TEXT('0') && C<=TEXT('9')) || (C>=TEXT('a') && C<=TEXT('f')))) return false;
    return true;
}
const FLGControlPoint* FindPoint(const FLGPiece& Piece,const FGuid& Id)
{
    return Piece.Points.FindByPredicate([&](const FLGControlPoint& Point) { return Point.Id==Id; });
}
const FLGEdge* FindEdge(const FLGPiece& Piece,const FGuid& Id)
{
    return Piece.Edges.FindByPredicate([&](const FLGEdge& Edge) { return Edge.Id==Id; });
}
}

ULaceGarmentDocument::ULaceGarmentDocument()
{
    SetFlags(RF_Transactional);
    GarmentId=FGuid::NewGuid(); PatternId=FGuid::NewGuid();
}

bool ULaceGarmentDocument::ValidateStructure(FString& Error) const
{
    Error.Reset();
    auto Fail=[&](const TCHAR* Reason) { Error=Reason; return false; };
    TSet<FGuid> IDs;
    auto AddID=[&](const FGuid& ID) { if(!ID.IsValid() || IDs.Contains(ID)) return false; IDs.Add(ID); return true; };
    if(!AddID(GarmentId) || !AddID(PatternId)) return Fail(TEXT("Document identifiers are missing or duplicated"));
    if(Pieces.Num()>32 || Seams.Num()>1024 || Anchors.Num()>1024 || Corrections.Num()>65536 || Name.Len()>256
        || TemplateParametersMm.Num()>64 || !FMath::IsFinite(BoundarySpacingMm) || BoundarySpacingMm<0.1 || BoundarySpacingMm>100)
        return Fail(TEXT("Document limits or sampling settings are invalid"));
    if(!FMath::IsFinite(MeshMaxEdgeMm) || MeshMaxEdgeMm<0.1 || MeshMaxEdgeMm>100)
        return Fail(TEXT("Mesh edge length is invalid"));
    for(const auto& Parameter : TemplateParametersMm)
        if(Parameter.Key.IsEmpty() || Parameter.Key.Len()>64 || !FMath::IsFinite(Parameter.Value) || FMath::Abs(Parameter.Value)>10000)
            return Fail(TEXT("Template parameter is invalid"));
    TMap<FGuid,const FLGPiece*> PieceById;
    TMap<FGuid,const FLGNotch*> NotchById;
    for(const auto& Piece : Pieces)
    {
        if(!AddID(Piece.Id)) return Fail(TEXT("Piece identifier is missing or duplicated"));
        PieceById.Add(Piece.Id,&Piece);
        if(Piece.Points.Num()>1024 || Piece.Edges.Num()>2048 || Piece.Holes.Num()>32 || Piece.Notches.Num()>1024 || Piece.Name.Len()>256)
            return Fail(TEXT("Piece exceeds authoring limits"));
        if(!ValidPoint(Piece.Grain) || Piece.Grain.SquaredLength()<1.e-12 || !Piece.InitialPlacementCm.IsValid()
            || Piece.InitialPlacementCm.GetTranslation().GetAbsMax()>10000 || !Piece.InitialPlacementCm.GetScale3D().Equals(FVector::OneVector,1.e-6))
            return Fail(TEXT("Piece grain or initial placement is invalid"));
        for(const auto& Point : Piece.Points)
            if(!AddID(Point.Id) || !ValidPoint(Point.PositionMm)) return Fail(TEXT("Control point identifier or position is invalid"));
        for(const auto& Edge : Piece.Edges)
        {
            if(!AddID(Edge.Id) || !FindPoint(Piece,Edge.Start) || !FindPoint(Piece,Edge.End))
                return Fail(TEXT("Edge identifier or endpoint reference is invalid"));
            if(Edge.Curve==ELGCurveType::Cubic)
            {
                if(!FindPoint(Piece,Edge.ControlA) || !FindPoint(Piece,Edge.ControlB)) return Fail(TEXT("Cubic edge refers to a missing control point"));
            }
            else if(Edge.Curve!=ELGCurveType::Line) return Fail(TEXT("Unsupported curve type"));
            if(uint8(Edge.Kind)>uint8(ELGBoundaryType::Cut) || !FMath::IsFinite(Edge.SeamAllowanceMm) || Edge.SeamAllowanceMm<0 || Edge.SeamAllowanceMm>100)
                return Fail(TEXT("Edge type or seam allowance is invalid"));
        }
        TSet<FGuid> LoopEdges;
        auto CheckLoop=[&](const FLGLoop& Loop)
        {
            for(const FGuid& Edge : Loop.Edges)
            {
                if(!FindEdge(Piece,Edge) || LoopEdges.Contains(Edge)) return false;
                LoopEdges.Add(Edge);
            }
            return true;
        };
        if(!CheckLoop(Piece.Outer)) return Fail(TEXT("Outer loop references a missing or repeated edge"));
        for(const auto& Hole : Piece.Holes) if(!CheckLoop(Hole)) return Fail(TEXT("Inner loop references a missing or repeated edge"));
        // Open and self-intersecting loops remain valid draft structures.
        // Generation performs closure and geometric validity checks separately.
        for(const auto& Notch : Piece.Notches)
        {
            if(!AddID(Notch.Id) || !FindEdge(Piece,Notch.Edge) || !FMath::IsFinite(Notch.Parameter) || Notch.Parameter<0 || Notch.Parameter>1)
                return Fail(TEXT("Notch reference or parameter is invalid"));
            NotchById.Add(Notch.Id,&Notch);
        }
    }
    for(const auto& Piece : Pieces)
    {
        TSet<FGuid> Seen; Seen.Add(Piece.Id);
        FGuid Mirror=Piece.MirrorOf;
        while(Mirror.IsValid())
        {
            if(Seen.Contains(Mirror)) return Fail(TEXT("Piece symmetry contains a cycle"));
            Seen.Add(Mirror);
            const FLGPiece* const* Referenced=PieceById.Find(Mirror);
            if(!Referenced) return Fail(TEXT("Piece symmetry refers to a missing piece"));
            Mirror=(*Referenced)->MirrorOf;
        }
    }
    for(const auto& Anchor : Anchors)
    {
        const auto* const* Piece=PieceById.Find(Anchor.Piece);
        if(!AddID(Anchor.Id) || !Piece || !FindPoint(**Piece,Anchor.Point) || Anchor.TargetCm.ContainsNaN() || Anchor.TargetCm.GetAbsMax()>10000)
            return Fail(TEXT("Anchor identifier or point reference is invalid"));
    }
    for(const auto& Pair : NotchById)
        if(Pair.Value->MatchingNotch.IsValid() && (Pair.Value->MatchingNotch==Pair.Key || !NotchById.Contains(Pair.Value->MatchingNotch)))
            return Fail(TEXT("Notch pairing refers to a missing or identical notch"));
    auto CheckInterval=[&](const FLGEdgeInterval& Interval)
    {
        const FLGPiece* const* Piece=PieceById.Find(Interval.Piece);
        return Piece && FindEdge(**Piece,Interval.Edge) && FMath::IsFinite(Interval.From) && FMath::IsFinite(Interval.To)
            && Interval.From>=0 && Interval.To<=1 && Interval.From<Interval.To;
    };
    TMap<FGuid,const FLGSeam*> SeamById;
    for(const auto& Seam : Seams)
    {
        if(!AddID(Seam.Id) || !CheckInterval(Seam.A)) return Fail(TEXT("Seam identifier or first edge interval is invalid"));
        if(Seam.Type==ELGSeamType::Hem)
        {
            if(Seam.B.Piece.IsValid() || Seam.B.Edge.IsValid() || !FMath::IsFinite(Seam.FoldWidthMm) || Seam.FoldWidthMm<=0 || Seam.FoldWidthMm>100)
                return Fail(TEXT("Hem needs one edge and a positive fold width"));
        }
        else if(Seam.Type==ELGSeamType::Join || Seam.Type==ELGSeamType::Binding)
        {
            if(!CheckInterval(Seam.B)) return Fail(TEXT("Seam second edge interval is invalid"));
            if(Seam.A.Piece==Seam.B.Piece && Seam.A.Edge==Seam.B.Edge && FMath::Max(Seam.A.From,Seam.B.From)<FMath::Min(Seam.A.To,Seam.B.To))
                return Fail(TEXT("A seam cannot join an edge interval to itself"));
        }
        else return Fail(TEXT("Unsupported seam type"));
        if(!FMath::IsFinite(Seam.EaseRatio) || Seam.EaseRatio<=0 || Seam.EaseRatio>10 || !FMath::IsFinite(Seam.LengthToleranceMm)
            || Seam.LengthToleranceMm<0 || Seam.LengthToleranceMm>100 || Seam.Prerequisites.Num()>1024)
            return Fail(TEXT("Seam ease or tolerance is invalid"));
        SeamById.Add(Seam.Id,&Seam);
    }
    TMap<FGuid,uint8> Visit;
    TFunction<bool(const FGuid&)> VisitSeam=[&](const FGuid& Id)
    {
        const auto* const* Seam=SeamById.Find(Id);
        if(!Seam) { Error=TEXT("Sewing prerequisite refers to a missing seam"); return false; }
        const uint8 State=Visit.FindRef(Id);
        if(State==1) { Error=TEXT("Sewing prerequisites contain a cycle"); return false; }
        if(State==2) return true;
        Visit.Add(Id,1);
        for(const auto& Before : (*Seam)->Prerequisites) if(!VisitSeam(Before)) return false;
        Visit.Add(Id,2); return true;
    };
    for(const auto& Seam : Seams) if(!VisitSeam(Seam.Id)) return false;
    for(const auto& Correction : Corrections)
    {
        if(!AddID(Correction.Id) || !Correction.Target.IsValid() || !ValidHash(Correction.TopologyHash)
            || Correction.Domain!=TEXT("simulationVertex") || uint8(Correction.State)>uint8(ELGCorrectionState::Unresolved))
            return Fail(TEXT("Correction identifier, domain or topology hash is invalid"));
        if(!Correction.Skin.IsEmpty())
        {
            if(Correction.Skin.Num()>4 || !Correction.Property.IsEmpty()) return Fail(TEXT("Skin correction needs at most four influences and no scalar property"));
            TSet<FGuid> Joints; double Sum=0;
            for(const auto& Influence : Correction.Skin)
            {
                if(!Influence.Joint.IsValid() || Joints.Contains(Influence.Joint) || !FMath::IsFinite(Influence.Weight) || Influence.Weight<=0)
                    return Fail(TEXT("Skin correction contains an invalid influence"));
                Joints.Add(Influence.Joint); Sum+=Influence.Weight;
            }
            if(!FMath::IsNearlyEqual(Sum,1.0,1.e-6)) return Fail(TEXT("Skin correction is not normalized"));
        }
        else if(Correction.Property.IsEmpty() || Correction.Property.Len()>64 || !FMath::IsFinite(Correction.Value) || Correction.Value<0)
            return Fail(TEXT("Scalar cloth correction is invalid"));
    }
    return true;
}

bool ULaceGarmentDocument::EvaluateEdge(const FGuid& PieceId,const FGuid& EdgeId,double Parameter,FVector2D& Out,FString& Error) const
{
    Error.Reset();
    const auto* Piece=Pieces.FindByPredicate([&](const FLGPiece& P) { return P.Id==PieceId; });
    const auto* Edge=Piece?FindEdge(*Piece,EdgeId):nullptr;
    if(!Edge || !FMath::IsFinite(Parameter) || Parameter<0 || Parameter>1) { Error=TEXT("Invalid edge or curve parameter"); return false; }
    const auto* A=FindPoint(*Piece,Edge->Start); const auto* B=FindPoint(*Piece,Edge->End);
    if(!A || !B) { Error=TEXT("Edge endpoint is missing"); return false; }
    FVector2D Result;
    if(Edge->Curve==ELGCurveType::Line) Result=FMath::Lerp(A->PositionMm,B->PositionMm,Parameter);
    else if(Edge->Curve==ELGCurveType::Cubic)
    {
        const auto* C=FindPoint(*Piece,Edge->ControlA); const auto* D=FindPoint(*Piece,Edge->ControlB);
        if(!C || !D) { Error=TEXT("Cubic control point is missing"); return false; }
        const double U=1-Parameter;
        Result=U*U*U*A->PositionMm+3*U*U*Parameter*C->PositionMm+3*U*Parameter*Parameter*D->PositionMm+Parameter*Parameter*Parameter*B->PositionMm;
    }
    else { Error=TEXT("Unsupported curve type"); return false; }
    if(!ValidPoint(Result)) { Error=TEXT("Evaluated curve is outside the supported mm range"); return false; }
    Out=Result; return true;
}

bool ULaceGarmentDocument::MovePoint(const FGuid& PieceId,const FGuid& PointId,FVector2D PositionMm,FString& Error)
{
    Error.Reset();
    auto* Piece=Pieces.FindByPredicate([&](const FLGPiece& P) { return P.Id==PieceId; });
    auto* Point=Piece?Piece->Points.FindByPredicate([&](const FLGControlPoint& P) { return P.Id==PointId; }):nullptr;
    if(!Point || !ValidPoint(PositionMm)) { Error=TEXT("Point or requested position is invalid"); return false; }
    Modify(); Point->PositionMm=PositionMm; InvalidateDerived(); return true;
}

void ULaceGarmentDocument::InvalidateDerived()
{
    Modify(); bDerivedCurrent=false;
    for(auto& Correction : Corrections) Correction.State=ELGCorrectionState::PendingRegeneration;
}

bool ULaceGarmentDocument::ApplyGeneratedTopology(const FString& Hash,FString& Error)
{
    Error.Reset();
    if(!ValidHash(Hash)) { Error=TEXT("Generated topology requires a lowercase SHA-256 hash"); return false; }
    if(!ValidateStructure(Error)) return false;
    Modify(); GeneratedTopologyHash=Hash;
    for(auto& Correction : Corrections)
        Correction.State=Correction.TopologyHash==Hash?ELGCorrectionState::Resolved:ELGCorrectionState::Unresolved;
    // Current topology is not a validated garment or permission to export.
    bDerivedCurrent=true; return true;
}

bool ULaceGarmentDocument::HasUnresolvedCorrections() const
{
    for(const auto& Correction : Corrections) if(Correction.State!=ELGCorrectionState::Resolved) return true;
    return false;
}

void ULaceGarmentDocument::PostEditUndo()
{
    Super::PostEditUndo();
    bDerivedCurrent=false;
    for(auto& Correction : Corrections) Correction.State=ELGCorrectionState::PendingRegeneration;
}

void ULaceGarmentDocument::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event); InvalidateDerived();
}
