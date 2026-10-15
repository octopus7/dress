#include "UI/LaceLoftPatternWorkspaceWidget.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Events.h"
#include "Algo/Reverse.h"

void ULaceLoftPatternWorkspaceWidget::NativeConstruct()
{Super::NativeConstruct();if(GetGameInstance())Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();SetClipping(EWidgetClipping::ClipToBounds);}
void ULaceLoftPatternWorkspaceWidget::NativeDestruct(){FinishInput();Session.Reset();Super::NativeDestruct();}
ULaceLoftPatternDefinition* ULaceLoftPatternWorkspaceWidget::Pattern() const
{
    if(Session.IsValid())if(const auto* Project=Session->GetCurrentProject())return Session->GetPattern(Project->PatternId);
    return IsDesignTime()?PreviewPattern.LoadSynchronous():nullptr;
}
FVector2D ULaceLoftPatternWorkspaceWidget::PieceOffset(const FLaceLoftPatternPiece& Piece) const
{
    if(bDragging && Piece.PieceId==InputPiece)return Piece.LayoutMm+DragOffset;
    if(Session.IsValid())if(const auto* Project=Session->GetCurrentProject())return Piece.LayoutMm+Project->PiecePositions.FindRef(Piece.PieceId);
    return Piece.LayoutMm;
}
FVector2D ULaceLoftPatternWorkspaceWidget::ToMm(const FGeometry& Geometry,FVector2D Screen) const
{return FLaceLoftCuttingModel::LocalToMm(Geometry.AbsoluteToLocal(Screen),Zoom,Pan,PixelsPerMm);}
void ULaceLoftPatternWorkspaceWidget::SelectPiece(FName Id){FinishInput();SelectedPiece=Id;SelectedSeam=NAME_None;Invalidate(EInvalidateWidgetReason::Paint);}
void ULaceLoftPatternWorkspaceWidget::SelectSeam(FName Id){FinishInput();SelectedSeam=Id;SelectedPiece=NAME_None;Invalidate(EInvalidateWidgetReason::Paint);}
void ULaceLoftPatternWorkspaceWidget::ResetView(){FinishInput();Pan=FVector2D(28,35);Zoom=1;Invalidate(EInvalidateWidgetReason::Paint);}

int32 ULaceLoftPatternWorkspaceWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Cull,
    FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    Layer=Super::NativePaint(Args,Geometry,Cull,Out,Layer,Style,Enabled)+1;
    auto* Definition=Pattern();if(!Definition)return Layer;
    const auto* Project=Session.IsValid()?Session->GetCurrentProject():nullptr;
    auto* Model=Project?Session->GetModel(Project->ProjectId):nullptr;
    const auto Screen=[&](FVector2D Mm){return Pan+Mm*(PixelsPerMm*Zoom);};
    const auto Line=[&](const TArray<FVector2D>& Points,FVector2D Offset,FLinearColor Color,float Width)
    {
        TArray<FVector2D> Draw;for(auto P:Points)Draw.Add(Screen(P+Offset));
        if(Draw.Num()>1)FSlateDrawElement::MakeLines(Out,Layer,Geometry.ToPaintGeometry(),Draw,ESlateDrawEffect::None,Color,true,Width);
    };
    const auto Progress=[&](const FLaceLoftPatternPath& Path,FName Id,FVector2D Offset)
    {
        if(!Model)return;
        double Arc=0;
        for(int32 I=1;I<Path.PointsMm.Num();++I)
        {
            const auto A=Path.PointsMm[I-1],B=Path.PointsMm[I];const double Length=FVector2D::Distance(A,B);
            for(const auto& Interval:Model->Progress(Id).Intervals)
            {
                const double Start=FMath::Max(Arc,Interval.StartMm),End=FMath::Min(Arc+Length,Interval.EndMm);
                if(End>Start && Length>UE_SMALL_NUMBER)Line({FMath::Lerp(A,B,(Start-Arc)/Length),FMath::Lerp(A,B,(End-Arc)/Length)},Offset,ProgressColor,4);
            }
            Arc+=Length;
        }
    };
    const auto Arrow=[&](FVector2D A,FVector2D B,FVector2D Offset,FLinearColor Color)
    {
        const FVector2D D=(B-A).GetSafeNormal(),N(-D.Y,D.X);
        Line({A,B},Offset,Color,2);Line({B-D*1.4+N*.8,B,B-D*1.4-N*.8},Offset,Color,2);
    };
    const auto Nearby=[&](const TArray<FVector2D>& Points,FVector2D Offset)
    {
        if(!IsHovered())return;
        const FVector2D Cursor=FLaceLoftCuttingModel::LocalToMm(LastPointer,Zoom,Pan,PixelsPerMm)-Offset;
        double Distance=MAX_dbl;FVector2D Closest,Direction;
        for(int32 I=1;I<Points.Num();++I)
        {
            const auto Delta=Points[I]-Points[I-1];const double Length2=Delta.SizeSquared();if(Length2<UE_SMALL_NUMBER)continue;
            const auto P=Points[I-1]+Delta*FMath::Clamp(FVector2D::DotProduct(Cursor-Points[I-1],Delta)/Length2,0.,1.);
            const double D=FVector2D::Distance(Cursor,P);
            if(D<Distance){Distance=D;Closest=P;Direction=Delta.GetSafeNormal();}
        }
        if(Distance<2.5)
        {
            Line({Closest-Direction,Closest+Direction},Offset,ActiveColor,5);
            // A short needle follows the cursor; only the model's tighter tolerance can add progress.
            Line({Cursor-FVector2D(.6,1.2),Cursor+FVector2D(.6,1.2)},Offset,CutColor,2);
        }
    };
    for(const auto& Piece:Definition->Pieces)
    {
        const auto Offset=PieceOffset(Piece);
        if(!Offset.Equals(Piece.LayoutMm))Line(Piece.CutOutline.PointsMm,Piece.LayoutMm,FLinearColor(.3f,.4f,.4f,.2f),1);
        Line(Piece.CutOutline.PointsMm,Offset,SelectedPiece==Piece.PieceId?ActiveColor:CutColor,2);
        Progress(Piece.CutOutline,Piece.CutOutline.PathId,Offset);
        if(SelectedSeam.IsNone() && (SelectedPiece.IsNone() || SelectedPiece==Piece.PieceId))Nearby(Piece.CutOutline.PointsMm,Offset);
        for(const auto& Edge:Piece.Edges)Line(Edge.Path.PointsMm,Offset,FLinearColor(.3f,.4f,.4f,.3f),1);
        for(const auto& Fold:Piece.FoldLines)Line(Fold.PointsMm,Offset,FLinearColor(.6f,.45f,.25f,.5f),1);
        for(auto N:Piece.NotchesMm)Line({N-FVector2D(.7,.7),N+FVector2D(.7,.7)},Offset,ActiveColor,2);
        FBox2D Bounds(ForceInit);for(auto P:Piece.CutOutline.PointsMm)Bounds+=P;
        const auto Center=Bounds.GetCenter();Arrow(Center-Piece.GrainDirection*2,Center+Piece.GrainDirection*2,Offset,FLinearColor(.3f,.4f,.4f,.45f));
        const FString Caption=Piece.DisplayName.ToString()+(Model && Model->IsPieceCut(Piece.PieceId)?TEXT(" ✓"):TEXT(""));
        FSlateDrawElement::MakeText(Out,Layer+1,Geometry.ToPaintGeometry(FVector2D(200,22),FSlateLayoutTransform(Screen(Offset+FVector2D(Bounds.Min.X,Bounds.Min.Y-4)))),Caption,
            FCoreStyle::GetDefaultFontStyle("Regular",12),ESlateDrawEffect::None,CutColor);
    }
    for(const auto& Seam:Definition->Seams)
    {
        const bool Active=Seam.SeamId==SelectedSeam,Complete=Model && Model->IsSeamComplete(Seam.SeamId);
        if(!Active && !Complete)continue;
        for(int32 I=0;I<Seam.SideA.Num();++I)
        {
            const auto& Ref=Seam.SideA[I];const auto* Piece=Definition->FindPiece(Ref.PieceId);
            auto Path=Piece->FindEdge(Ref.EdgeId)->Path;if(Ref.Direction<0)Algo::Reverse(Path.PointsMm);
            const auto Offset=PieceOffset(*Piece);
            Line(Path.PointsMm,Offset,Complete?ProgressColor:ActiveColor,3);
            if(Active && Path.PointsMm.Num()>1)Arrow(Path.PointsMm[0],Path.PointsMm[1],Offset,ActiveColor);
            Progress(Path,FLaceLoftSewingModel::SeamPathId(Seam.SeamId,I),Offset);
            if(Active)Nearby(Path.PointsMm,Offset);
        }
        if(Active)for(const auto& Ref:Seam.SideB)
        {
            const auto* Piece=Definition->FindPiece(Ref.PieceId);auto Points=Piece->FindEdge(Ref.EdgeId)->Path.PointsMm;
            if(Ref.Direction<0)Algo::Reverse(Points);
            Line(Points,PieceOffset(*Piece),FLinearColor(.3f,.4f,.85f,1),3);
            if(Points.Num()>1)Arrow(Points[0],Points[1],PieceOffset(*Piece),FLinearColor(.3f,.4f,.85f,1));
        }
        if((Active || Complete) && !Seam.SideA.IsEmpty() && !Seam.SideB.IsEmpty())
        {
            const auto& A=Seam.SideA[0];const auto& B=Seam.SideB[0];
            const auto* PA=Definition->FindPiece(A.PieceId);const auto* PB=Definition->FindPiece(B.PieceId);
            auto AP=PA->FindEdge(A.EdgeId)->Path.PointsMm;auto BP=PB->FindEdge(B.EdgeId)->Path.PointsMm;
            if(A.Direction<0)Algo::Reverse(AP);if(B.Direction<0)Algo::Reverse(BP);
            Line({AP[0]+PieceOffset(*PA),BP[0]+PieceOffset(*PB)},FVector2D::ZeroVector,Complete?ProgressColor:TargetColor,1);
            if(Active && PA!=PB && AP.Num()>1 && BP.Num()>1)
            {
                const auto AD=(AP.Last()-AP[0]).GetSafeNormal(),BD=(BP.Last()-BP[0]).GetSafeNormal();
                const double Angle=FMath::Atan2(AD.Y,AD.X)-FMath::Atan2(BD.Y,BD.X);
                TArray<FVector2D> Target;
                for(auto P:PB->CutOutline.PointsMm)
                {
                    P-=BP[0];Target.Add(AP[0]+FVector2D(P.X*FMath::Cos(Angle)-P.Y*FMath::Sin(Angle),P.X*FMath::Sin(Angle)+P.Y*FMath::Cos(Angle)));
                }
                Line(Target,PieceOffset(*PA),TargetColor,2);
            }
        }
    }
    return Layer+2;
}

FReply ULaceLoftPatternWorkspaceWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& Event)
{
    LastPointer=G.AbsoluteToLocal(Event.GetScreenSpacePosition());
    if(Event.GetEffectingButton()==EKeys::MiddleMouseButton || Event.GetEffectingButton()==EKeys::RightMouseButton)
    {FinishInput();bPanning=true;return FReply::Handled().CaptureMouse(TakeWidget());}
    if(Event.GetEffectingButton()!=EKeys::LeftMouseButton || !Session.IsValid())return FReply::Unhandled();
    auto* Definition=Pattern();const auto* Project=Session->GetCurrentProject();if(!Definition || !Project)return FReply::Unhandled();
    const auto Point=ToMm(G,Event.GetScreenSpacePosition());auto* Model=Session->GetModel(Project->ProjectId);
    if(const auto* Seam=Definition->FindSeam(SelectedSeam))
    {
        for(int32 I=0;I<Seam->SideA.Num();++I)
        {
            const auto* Piece=Definition->FindPiece(Seam->SideA[I].PieceId);
            if(Session->BeginSeam(SelectedSeam,I,Point-PieceOffset(*Piece)))
            {InputPiece=Piece->PieceId;bDrawing=true;break;}
        }
    }
    else for(const auto& Piece:Definition->Pieces)
    {
        if(!SelectedPiece.IsNone() && SelectedPiece!=Piece.PieceId)continue;
        const auto Offset=PieceOffset(Piece);const auto Local=Point-Offset;
        if(Session->BeginCut(Piece.PieceId,Local)){InputPiece=Piece.PieceId;SelectedPiece=Piece.PieceId;bDrawing=true;break;}
        if(Model && Model->IsPieceCut(Piece.PieceId))
        {
            FBox2D Bounds(ForceInit);for(auto P:Piece.CutOutline.PointsMm)Bounds+=P;
            if(Bounds.IsInside(Local)){InputPiece=Piece.PieceId;DragStartMm=Point;DragOriginal=Offset-Piece.LayoutMm;DragOffset=DragOriginal;bDragging=true;break;}
        }
    }
    return FReply::Handled().CaptureMouse(TakeWidget());
}
FReply ULaceLoftPatternWorkspaceWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& Event)
{
    const auto Local=G.AbsoluteToLocal(Event.GetScreenSpacePosition());
    if(bPanning)Pan+=Local-LastPointer;
    else if(bDragging)DragOffset=DragOriginal+ToMm(G,Event.GetScreenSpacePosition())-DragStartMm;
    else if(bDrawing && Session.IsValid())if(auto* Definition=Pattern())if(const auto* Piece=Definition->FindPiece(InputPiece))
        Session->UpdateStroke(ToMm(G,Event.GetScreenSpacePosition())-PieceOffset(*Piece));
    LastPointer=Local;Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
}
void ULaceLoftPatternWorkspaceWidget::FinishInput()
{
    const bool Move=bDragging;bDragging=false;bDrawing=false;bPanning=false;
    if(Session.IsValid()){Session->EndStroke();if(Move)Session->MovePiece(InputPiece,DragOffset);}
}
FReply ULaceLoftPatternWorkspaceWidget::NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&)
{FinishInput();return FReply::Handled().ReleaseMouseCapture();}
void ULaceLoftPatternWorkspaceWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{FinishInput();Super::NativeOnMouseCaptureLost(Event);}
FReply ULaceLoftPatternWorkspaceWidget::NativeOnMouseWheel(const FGeometry& G,const FPointerEvent& Event)
{
    FinishInput();const auto Local=G.AbsoluteToLocal(Event.GetScreenSpacePosition());const auto Mm=ToMm(G,Event.GetScreenSpacePosition());
    Zoom=FMath::Clamp(Zoom*FMath::Pow(1.15f,Event.GetWheelDelta()),.3f,5.f);Pan=Local-Mm*(PixelsPerMm*Zoom);
    Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
}

