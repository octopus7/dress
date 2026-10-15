#include "LaceGarmentTemplates.h"
#include "LaceGarmentSampling.h"
namespace LaceGarment
{
bool ApplyTemplate(ULaceGarmentDocument& D,ETemplate Template,const TMap<FString,double>& Parameters,FString& Error)
{
    Error.Reset();
    auto Fail=[&](const TCHAR* Why){Error=Why; return false;};
    if(Template!=ETemplate::TShirt && Template!=ETemplate::DolphinShorts) return Fail(TEXT("Unknown garment template"));
    if(!D.GarmentId.IsValid() || !D.PatternId.IsValid()) return Fail(TEXT("Template document requires stable garment and pattern identifiers"));
    const bool Shirt=Template==ETemplate::TShirt;
    const FString Name=Shirt?TEXT("OB11 T-Shirt"):TEXT("OB11 Dolphin Shorts");
    TMap<FString,double> P=Shirt?TMap<FString,double>{{TEXT("sleeveLength"),18},{TEXT("shirtLength"),29},{TEXT("neckWidth"),32},{TEXT("cuffWidth"),24},{TEXT("armholeDepth"),17}}:TMap<FString,double>{{TEXT("shortsLength"),22}};
    // Unspecified fields retain their prior value on updates to this same template.
    if(D.TemplateName==Name)
        for(auto& Pair : P) if(const double* V=D.TemplateParametersMm.Find(Pair.Key)) Pair.Value=*V;
    for(const auto& Pair : Parameters)
    {
        if(!P.Contains(Pair.Key)||!FMath::IsFinite(Pair.Value)) return Fail(TEXT("Unknown or non-finite template parameter"));
        P[Pair.Key]=Pair.Value;
    }
    const double Sleeve=Shirt?P[TEXT("sleeveLength")]:0;
    const double NeckHalf=Shirt?P[TEXT("neckWidth")]*.5:0;
    const double CuffWidth=Shirt?P[TEXT("cuffWidth")]:0;
    const double ArmholeDepth=Shirt?P[TEXT("armholeDepth")]:0;
    const double Length=P[Shirt?TEXT("shirtLength"):TEXT("shortsLength")];
    if((Shirt && (Sleeve<10 || Sleeve>25 || Length<24 || Length>38 || NeckHalf<8 || NeckHalf>18 || CuffWidth<12 || CuffWidth>30 || ArmholeDepth<10 || ArmholeDepth>22)) || (!Shirt && (Length<15 || Length>32)))
        return Fail(TEXT("Template length is outside the supported doll-scale range"));
    auto ID=[&](const FString& Role){return FGuid::NewDeterministicGuid(D.PatternId.ToString()+TEXT("/")+Name+TEXT("/")+Role);};
    auto* Draft=NewObject<ULaceGarmentDocument>();
    Draft->GarmentId=D.GarmentId; Draft->PatternId=D.PatternId;
    Draft->Name=Name; Draft->TemplateName=Name; Draft->TemplateVersion=TEXT("0.3.3-draft"); Draft->TemplateParametersMm=P;
    Draft->BoundarySpacingMm=D.BoundarySpacingMm; Draft->MeshMaxEdgeMm=D.MeshMaxEdgeMm;
    TArray<int32> Sewn=Shirt?TArray<int32>{1,2,4,5,8,9,11,12}:TArray<int32>{2,4,5,7};
    for(int32 Side=0;Side<2;++Side)
    {
        const FString Role=Side==0?TEXT("front"):TEXT("back"); const double Sign=Side==0?1:-1;
        FLGPiece Piece; Piece.Id=ID(Role); Piece.Name=Side==0?TEXT("Front"):TEXT("Back");
        const double Height=Shirt?95:68;
        Piece.InitialPlacementCm=FTransform(FRotator(0,Side==0?90:-90,-90),FVector(Side==0?1.8:-1.8,0,(Height-Length)/10));
        TArray<FVector2d> Boundary;
        if(Shirt)
        {
            const FVector2d Cuff=FVector2d(15,Length-4)+FVector2d(.65,-.76)*Sleeve;
            const FVector2d Under=Cuff-FVector2d(.76,.65)*6,Over=Under+FVector2d(.76,.65)*CuffWidth;
            // A shallow 1.5mm back neckline cannot span the fixed neck guides
            // around the saved shoulder collision within the 10% strain limit.
            const double NeckDepth=4;
            Boundary={{-22,0},{22,0},{18,Length-ArmholeDepth},Under,Over,{20,Length},{NeckHalf,Length},{0,Length-NeckDepth},{-NeckHalf,Length},{-20,Length},{-Over.X,Over.Y},{-Under.X,Under.Y},{-18,Length-ArmholeDepth}};
        }
        else Boundary={{-22,Length},{0,Length},{22,Length},{24,3},{4,0},{0,Length-FMath::Min(15.0,Length-3)},{-4,0},{-24,3}};
        auto AddPoint=[&](const FString& PointRole,FVector2d V)
        {
            FLGControlPoint C; C.Id=ID(Role+TEXT("/")+PointRole); C.PositionMm=FVector2D(Sign*V.X,V.Y); Piece.Points.Add(C); return C.Id;
        };
        for(int32 I=0;I<Boundary.Num();++I) AddPoint(FString::Printf(TEXT("point/%d"),I),Boundary[I]);
        for(int32 I=0;I<Boundary.Num();++I)
        {
            FLGEdge E; E.Id=ID(Role+FString::Printf(TEXT("/edge/%d"),I)); E.Start=Piece.Points[I].Id; E.End=Piece.Points[(I+1)%Boundary.Num()].Id;
            E.Kind=Sewn.Contains(I)?ELGBoundaryType::Stitch:ELGBoundaryType::Open; E.SeamAllowanceMm=E.Kind==ELGBoundaryType::Stitch?2:0;
            Piece.Edges.Add(E); Piece.Outer.Edges.Add(E.Id);
        }
        auto Curve=[&](int32 Edge,FVector2d A,FVector2d B)
        {
            auto& E=Piece.Edges[Edge]; E.Curve=ELGCurveType::Cubic;
            E.ControlA=AddPoint(FString::Printf(TEXT("handle/%d/a"),Edge),A);
            E.ControlB=AddPoint(FString::Printf(TEXT("handle/%d/b"),Edge),B);
        };
        if(Shirt)
        {
            const double Depth=4;
            Curve(6,{NeckHalf-1,Length-Depth},{NeckHalf*.45,Length-Depth}); Curve(7,{-NeckHalf*.45,Length-Depth},{-NeckHalf+1,Length-Depth});
        }
        else
        {
            Curve(3,{19,-1},{10,-1}); Curve(6,{-10,-1},{-19,-1});
        }
        FLGAnchor A; A.Id=ID(Role+TEXT("/anchor")); A.Piece=Piece.Id; A.Point=Piece.Points[Shirt?7:1].Id;
        if(Shirt) { A.bUseTarget=true; A.TargetCm=FVector(Side==0?1.05:-1.05,0,9.5); }
        else { A.bUseTarget=true; A.TargetCm=FVector(Side==0?1.1:-1.1,0,6.4); }
        Draft->Anchors.Add(A);
        if(Shirt) for(int32 Point : {6,8})
        {
            FLGAnchor Shoulder; Shoulder.Id=ID(Role+FString::Printf(TEXT("/shoulder-anchor/%d"),Point));
            Shoulder.Piece=Piece.Id; Shoulder.Point=Piece.Points[Point].Id; Shoulder.bUseTarget=true;
            Shoulder.TargetCm=FVector(0,Point==6?1.4:-1.4,9.75);
            Draft->Anchors.Add(Shoulder);
        }
        Draft->Pieces.Add(MoveTemp(Piece));
    }
    for(int32 Edge : Sewn)
    {
        FLGSeam S; S.Id=ID(FString::Printf(TEXT("seam/%d"),Edge));
        S.A.Piece=Draft->Pieces[0].Id; S.A.Edge=Draft->Pieces[0].Edges[Edge].Id;
        S.B.Piece=Draft->Pieces[1].Id; S.B.Edge=Draft->Pieces[1].Edges[Edge].Id;
        S.ReverseB=false; S.LengthToleranceMm=.05; Draft->Seams.Add(S);
    }
    FSampledDocument Check;
    if(!SampleDocument(*Draft,Check,Error)) return false;
    D.Modify();
    D.Name=Draft->Name; D.TemplateName=Draft->TemplateName; D.TemplateVersion=Draft->TemplateVersion; D.TemplateParametersMm=MoveTemp(Draft->TemplateParametersMm);
    D.Pieces=MoveTemp(Draft->Pieces); D.Seams=MoveTemp(Draft->Seams); D.Anchors=MoveTemp(Draft->Anchors);
    D.InvalidateDerived(); return true;
}
}
