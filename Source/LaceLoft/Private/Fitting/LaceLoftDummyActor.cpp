#include "Fitting/LaceLoftDummyActor.h"
#include "Fitting/LaceLoftDummyAnimInstance.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInterface.h"

ALaceLoftDummyActor::ALaceLoftDummyActor()
{
    Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Dummy"));SetRootComponent(Body);
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Top=CreateDefaultSubobject<UChaosClothComponent>(TEXT("TopCloth"));Top->SetupAttachment(Body);
    Bottom=CreateDefaultSubobject<UChaosClothComponent>(TEXT("BottomCloth"));Bottom->SetupAttachment(Body);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("FittingCamera"));Camera->SetupAttachment(Body);
    Camera->FieldOfView=50;Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    Camera->PostProcessSettings.bOverride_AutoExposureBias=true;Camera->PostProcessSettings.AutoExposureBias=0;
    Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;Camera->PostProcessSettings.MotionBlurAmount=0;
    UpdateCamera();
}
void ALaceLoftDummyActor::ApplySession(ULaceLoftWardrobeSubsystem& Session)
{
    auto* Anim=Cast<ULaceLoftDummyAnimInstance>(Body->GetAnimInstance());
    if(!ensureMsgf(Anim,TEXT("Saved ABP_Dummy must use LaceLoftDummyAnimInstance")))return;
    const bool PoseChanged=CurrentPose!=Session.GetPose() || !CurrentJoints.OrderIndependentCompareEqual(Session.GetJointRotations());
    if(PoseChanged)
    {
        CurrentPose=Session.GetPose();CurrentJoints=Session.GetJointRotations();Anim->SelectedPose=Poses.FindRef(CurrentPose);Anim->JointOffsets=CurrentJoints;
    }
    for(const auto Slot:{ELaceLoftGarmentSlot::Top,ELaceLoftGarmentSlot::Bottom})
    {
        auto* Component=Slot==ELaceLoftGarmentSlot::Top?Top.Get():Bottom.Get();
        const FGuid Id=Session.GetEquipped(Slot);UChaosClothAsset* Asset=nullptr;
        if(Session.CanEquip(Id))Asset=Session.GetPattern(Session.GetProject(Id)->PatternId)->ClothAsset.LoadSynchronous();
        const bool Changed=Component->GetAsset()!=Asset;
        if(Changed){Component->SetAsset(Asset);Component->SetVisibility(Asset!=nullptr);Component->SetEnableSimulation(Asset!=nullptr);}
        if(Asset && (Changed || PoseChanged))Component->ForceNextUpdateTeleportAndReset();
    }
}
void ALaceLoftDummyActor::UpdateCamera()
{
    const FVector Target(0,0,7.5);const FVector Offset=FRotator(Pitch,Yaw,0).Vector()*Distance;
    Camera->SetRelativeLocation(Target+Offset);Camera->SetRelativeRotation((-Offset).Rotation());
}
void ALaceLoftDummyActor::Orbit(FVector2D Delta)
{Yaw=FMath::UnwindDegrees(Yaw+Delta.X*.3f);Pitch=FMath::Clamp(Pitch+float(Delta.Y)*.3f,-60.f,60.f);UpdateCamera();}
void ALaceLoftDummyActor::Zoom(float Delta){Distance=FMath::Clamp(Distance-Delta*2,20.f,60.f);UpdateCamera();}
void ALaceLoftDummyActor::ResetCamera(){Yaw=Pitch=0;Distance=42;UpdateCamera();}
bool ALaceLoftDummyActor::SetWireframeVisible(bool Visible)
{
    if(Visible && !WireframeMaterial)
        WireframeMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Characters/Dummy/Materials/M_WireframeOverlay.M_WireframeOverlay"));
    if(Visible && !WireframeMaterial)return false;
    // The same posed/simulated render vertices are drawn again; base materials stay intact.
    for(UMeshComponent* Mesh:{static_cast<UMeshComponent*>(Body.Get()),static_cast<UMeshComponent*>(Top.Get()),static_cast<UMeshComponent*>(Bottom.Get())})
        Mesh->SetOverlayMaterial(Visible?WireframeMaterial.Get():nullptr);
    bWireframeVisible=Visible;return true;
}
