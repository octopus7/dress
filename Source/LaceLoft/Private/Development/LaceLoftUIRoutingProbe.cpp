#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "UI/LaceLoftPatternWorkspaceWidget.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Algo/Reverse.h"
#include "Fitting/LaceLoftDummyActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"

bool LaceLoftProbeRoutedCut(UUserWidget* Screen,const FString& CaptureFile)
{
    auto* Workspace=Cast<ULaceLoftPatternWorkspaceWidget>(Screen->GetWidgetFromName(TEXT("Workspace")));
    auto* Session=Screen->GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
    auto& App=FSlateApplication::Get();auto Window=App.FindWidgetWindow(Screen->TakeWidget());if(!Window)return false;
    const auto Hit=[&](FVector2D P){return FWidgetPath(Window->GetHittestGrid().GetBubblePath(P,0,false,0));};
    const auto Event=[](FVector2D P,FVector2D Last,bool Down,FKey Key)
    {return FPointerEvent(0,FSlateApplication::CursorPointerIndex,P,Last,Down?TSet<FKey>{EKeys::LeftMouseButton}:TSet<FKey>{},Key,0,FModifierKeysState());};
    const auto Click=[&](UWidget* Widget)
    {
        if(!Widget)return false;const auto G=Widget->GetCachedGeometry();const auto P=G.LocalToAbsolute(G.GetLocalSize()*.5);
        App.RoutePointerMoveEvent(Hit(P),Event(P,P,false,EKeys::Invalid),false);
        App.RoutePointerDownEvent(Hit(P),Event(P,P,true,EKeys::LeftMouseButton));
        App.RoutePointerUpEvent(Hit(P),Event(P,P,false,EKeys::LeftMouseButton));return true;
    };
    if(!Workspace)
    {
        auto* Area=Screen->GetWidgetFromName(TEXT("CameraInput"));if(!Area)return true;
        ALaceLoftDummyActor* Dummy=nullptr;for(TActorIterator<ALaceLoftDummyActor> It(Screen->GetWorld());It;++It){Dummy=*It;break;}if(!Dummy)return false;
        const auto Before=Dummy->Camera->GetRelativeLocation();const auto G=Area->GetCachedGeometry();const auto Start=G.LocalToAbsolute(G.GetLocalSize()*.5),End=Start+FVector2D(80,30);
        App.RoutePointerDownEvent(Hit(Start),Event(Start,Start,true,EKeys::LeftMouseButton));
        App.RoutePointerMoveEvent(Hit(End),Event(End,Start,true,EKeys::Invalid),false);
        App.RoutePointerUpEvent(Hit(End),Event(End,End,false,EKeys::LeftMouseButton));
        const bool Orbit=!Before.Equals(Dummy->Camera->GetRelativeLocation(),.01);
        const auto AfterOrbit=Dummy->Camera->GetRelativeLocation();
        App.RouteMouseWheelOrGestureEvent(Hit(End),FPointerEvent(0,FSlateApplication::CursorPointerIndex,End,End,{},EKeys::Invalid,1,FModifierKeysState()));
        const bool Zoom=!AfterOrbit.Equals(Dummy->Camera->GetRelativeLocation(),.01);
        auto* Controls=Cast<UUserWidget>(Screen->GetWidgetFromName(TEXT("PoseControls")));const FName OriginalPose=Session->GetPose();bool Poses=Controls!=nullptr;
        const auto Camera=Dummy->Camera->GetRelativeLocation();
        if(Controls)for(const auto& Pair:TMap<FName,FName>{{TEXT("NeutralButton"),TEXT("Neutral")},{TEXT("RaisedButton"),TEXT("ArmsRaised")},{TEXT("ForwardButton"),TEXT("ArmsForward")},{TEXT("SittingButton"),TEXT("Sitting")}})
        {Click(Controls->GetWidgetFromName(Pair.Key));Poses &= Session->GetPose()==Pair.Value && Dummy->Camera->GetRelativeLocation().Equals(Camera,.001);}
        Click(Screen->GetWidgetFromName(TEXT("ResetCameraButton")));const bool Reset=Before.Equals(Dummy->Camera->GetRelativeLocation(),.001);Session->SetPose(OriginalPose);
        bool Wireframe=true;
        if(FParse::Param(FCommandLine::Get(),TEXT("UIProbeWireframe")))
        {
            auto* Toggle=Screen->GetWidgetFromName(TEXT("WireframeButton"));
            Click(Toggle);Wireframe=Dummy->IsWireframeVisible();
            Click(Toggle);Wireframe &= !Dummy->IsWireframeVisible();
            Click(Toggle);Wireframe &= Dummy->IsWireframeVisible();
        }
        const bool Passed=Orbit && Zoom && Reset && Poses && Wireframe;
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"passed\":%s,\"orbit\":%s,\"zoom\":%s,\"reset\":%s,\"pose_buttons_without_camera_motion\":%s}"),Passed?TEXT("true"):TEXT("false"),Orbit?TEXT("true"):TEXT("false"),Zoom?TEXT("true"):TEXT("false"),Reset?TEXT("true"):TEXT("false"),Poses?TEXT("true"):TEXT("false")),*(FPaths::GetPath(CaptureFile)/TEXT("routed-input.json")));
        return Passed;
    }
    const auto* Project=Session->GetCurrentProject();if(!Project)return false;
    const FGuid Id=Project->ProjectId;auto* Pattern=Session->GetPattern(Project->PatternId);
    const auto& Piece=Pattern->Pieces[0];Workspace->SelectPiece(Piece.PieceId);Workspace->ResetView();
    const auto Geometry=Workspace->GetCachedGeometry();
    const auto Position=[&](FVector2D P){return Geometry.LocalToAbsolute(FVector2D(28,35)+(Piece.LayoutMm+P)*Workspace->PixelsPerMm);};
    // RenderOffscreen deliberately has no visible OS window. Query its actual
    // painted hit grid, then let Slate route/bubble and process capture normally.
    const double Before=Session->GetModel(Id)->Progress(Piece.CutOutline.PathId).CoveredMm();
    const auto Start=Piece.CutOutline.PointsMm[0];const auto End=Piece.CutOutline.PointsMm[1];
    FVector2D Last=Position(Start);const auto InitialPath=Hit(Last);
    UE_LOG(LogTemp,Display,TEXT("Offscreen painted hit path: %d widgets; window visible %d; point %s"),InitialPath.Widgets.Num(),Window->IsVisible(),*Last.ToString());
    App.RoutePointerMoveEvent(InitialPath,Event(Last,Last,false,EKeys::Invalid),false);
    App.RoutePointerDownEvent(InitialPath,Event(Last,Last,true,EKeys::LeftMouseButton));
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Start,End)));
    for(int32 I=1;I<=Steps;++I)
    {
        const auto P=Position(FMath::Lerp(Start,End,double(I)/Steps));App.RoutePointerMoveEvent(Hit(P),Event(P,Last,true,EKeys::Invalid),false);Last=P;
    }
    App.RoutePointerUpEvent(Hit(Last),Event(Last,Last,false,EKeys::LeftMouseButton));
    const double After=Session->GetModel(Id)->Progress(Piece.CutOutline.PathId).CoveredMm();
    bool Passed=After>Before+.1;
    if(FParse::Param(FCommandLine::Get(),TEXT("UIProbeComplete")))
    {
        const auto Trace=[&](const FLaceLoftPatternPath& Path,FVector2D Offset)
        {
            const auto Pos=[&](FVector2D P){return Geometry.LocalToAbsolute(FVector2D(28,35)+(Offset+P)*Workspace->PixelsPerMm);};
            FVector2D Last=Pos(Path.PointsMm[0]);App.RoutePointerDownEvent(Hit(Last),Event(Last,Last,true,EKeys::LeftMouseButton));
            for(int32 I=1;I<Path.PointsMm.Num();++I)
            {
                const int32 Count=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Path.PointsMm[I-1],Path.PointsMm[I])));
                for(int32 J=1;J<=Count;++J){const auto P=Pos(FMath::Lerp(Path.PointsMm[I-1],Path.PointsMm[I],double(J)/Count));App.RoutePointerMoveEvent(Hit(P),Event(P,Last,true,EKeys::Invalid),false);Last=P;}
            }
            App.RoutePointerUpEvent(Hit(Last),Event(Last,Last,false,EKeys::LeftMouseButton));
        };
        for(int32 Garment=0;Garment<2;++Garment)
        {
            if(Garment==1)Click(Screen->GetWidgetFromName(TEXT("NewBottomButton")));
            const auto Current=Session->GetCurrentProject()->ProjectId;auto* Definition=Session->GetPattern(Session->GetCurrentProject()->PatternId);
            for(const auto& P:Definition->Pieces)
            {
                Workspace->SelectPiece(P.PieceId);Trace(P.CutOutline,P.LayoutMm);const bool Cut=Session->GetModel(Current)->IsPieceCut(P.PieceId);Passed &= Cut;
                UE_LOG(LogTemp,Display,TEXT("Full routed piece %s %s: %.4f / %.4f"),*Definition->PatternId.ToString(),*P.PieceId.ToString(),Session->GetModel(Current)->Progress(P.CutOutline.PathId).CoveredMm(),P.CutOutline.LengthMm());
            }
            for(int32 Pass=0;Pass<Definition->Seams.Num();++Pass)for(const auto& Seam:Definition->Seams)
            {
                if(!Session->GetModel(Current)->CanStartSeam(Seam.SeamId))continue;Workspace->SelectSeam(Seam.SeamId);
                for(const auto& Ref:Seam.SideA){const auto* P=Definition->FindPiece(Ref.PieceId);auto Path=P->FindEdge(Ref.EdgeId)->Path;if(Ref.Direction<0)Algo::Reverse(Path.PointsMm);Trace(Path,P->LayoutMm);}
            }
            Passed &= Session->CanEquip(Current);Passed &= Session->Equip(Current);
            for(const auto& Seam:Definition->Seams)UE_LOG(LogTemp,Display,TEXT("Full routed seam %s: %d"),*Seam.SeamId.ToString(),Session->GetModel(Current)->IsSeamComplete(Seam.SeamId));
        }
        Passed &= Session->GetProjects().Num()==2 && Session->SaveSession();
    }
    const FString Json=FString::Printf(TEXT("{\"passed\":%s,\"before_mm\":%.6f,\"after_mm\":%.6f,\"workspace_width\":%.3f,\"workspace_height\":%.3f}"),
        Passed?TEXT("true"):TEXT("false"),Before,After,Geometry.GetLocalSize().X,Geometry.GetLocalSize().Y);
    FFileHelper::SaveStringToFile(Json,*(FPaths::GetPath(CaptureFile)/TEXT("routed-input.json")));
    UE_LOG(LogTemp,Display,TEXT("Routed Slate cut %s: %.4f -> %.4f mm"),Passed?TEXT("PASS"):TEXT("FAIL"),Before,After);
    return Passed;
}
#endif
