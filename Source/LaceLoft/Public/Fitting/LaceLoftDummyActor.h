#pragma once
#include "GameFramework/Actor.h"
#include "LaceLoftDummyActor.generated.h"
class USkeletalMeshComponent;class UChaosClothComponent;class UCameraComponent;class UAnimSequence;class ULaceLoftWardrobeSubsystem;
class UMaterialInterface;

UCLASS()
class LACELOFT_API ALaceLoftDummyActor : public AActor
{
    GENERATED_BODY()
public:
    ALaceLoftDummyActor();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UChaosClothComponent> Top;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UChaosClothComponent> Bottom;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(EditAnywhere,Category="Poses") TMap<FName,TObjectPtr<UAnimSequence>> Poses;
    void ApplySession(ULaceLoftWardrobeSubsystem& Session);
    void Orbit(FVector2D Delta);
    void Zoom(float Delta);
    void ResetCamera();
    bool SetWireframeVisible(bool Visible);
    bool IsWireframeVisible() const {return bWireframeVisible;}
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> WireframeMaterial;
    bool bWireframeVisible=false;
    void UpdateCamera();
    float Yaw=0,Pitch=0,Distance=42;
    FName CurrentPose;
    TMap<FName,FRotator> CurrentJoints;
};
