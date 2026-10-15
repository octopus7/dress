#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LaceLoftFittingController.generated.h"
class ALaceLoftDummyActor;class ULaceLoftWardrobeSubsystem;class APlayerController;
UCLASS()
class LACELOFT_API ULaceLoftFittingController : public UObject
{
    GENERATED_BODY()
public:
    bool Initialize(APlayerController* Player);
    void Shutdown();
    void Orbit(FVector2D Delta);
    void Zoom(float Delta);
    void ResetCamera();
    ALaceLoftDummyActor* GetDummy() const{return Dummy.Get();}
private:
    void Refresh();
    TWeakObjectPtr<ALaceLoftDummyActor> Dummy;
    TWeakObjectPtr<ULaceLoftWardrobeSubsystem> Session;
};
