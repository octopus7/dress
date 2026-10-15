#include "Fitting/LaceLoftFittingController.h"
#include "Fitting/LaceLoftDummyActor.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
bool ULaceLoftFittingController::Initialize(APlayerController* Player)
{
    Shutdown();if(!Player)return false;
    for(TActorIterator<ALaceLoftDummyActor> It(Player->GetWorld());It;++It){Dummy=*It;break;}
    if(!Dummy.IsValid())return false;
    Session=Player->GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();Session->OnChanged.AddUObject(this,&ThisClass::Refresh);
    Player->bAutoManageActiveCameraTarget=false;Player->SetViewTarget(Dummy.Get());Refresh();return true;
}
void ULaceLoftFittingController::Shutdown(){if(Session.IsValid())Session->OnChanged.RemoveAll(this);Session.Reset();Dummy.Reset();}
void ULaceLoftFittingController::Refresh(){if(Dummy.IsValid() && Session.IsValid())Dummy->ApplySession(*Session.Get());}
void ULaceLoftFittingController::Orbit(FVector2D Delta){if(Dummy.IsValid())Dummy->Orbit(Delta);}
void ULaceLoftFittingController::Zoom(float Delta){if(Dummy.IsValid())Dummy->Zoom(Delta);}
void ULaceLoftFittingController::ResetCamera(){if(Dummy.IsValid())Dummy->ResetCamera();}
