#include "UI/LaceLoftFittingWidget.h"
#include "UI/LaceLoftCreationWidget.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "Fitting/LaceLoftFittingController.h"
#include "Fitting/LaceLoftJointLimits.h"
#include "Fitting/LaceLoftDummyActor.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/Slider.h"
#include "Engine/GameInstance.h"

namespace
{
const TCHAR* JointIds[]={TEXT("head"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("forearm_l"),TEXT("forearm_r"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r")};
const TCHAR* JointLabels[]={TEXT("고개"),TEXT("왼쪽 어깨"),TEXT("오른쪽 어깨"),TEXT("왼쪽 팔꿈치"),TEXT("오른쪽 팔꿈치"),TEXT("왼쪽 고관절"),TEXT("오른쪽 고관절"),TEXT("왼쪽 무릎"),TEXT("오른쪽 무릎")};
}
void ULaceLoftPoseControlsWidget::NativeConstruct()
{
    Super::NativeConstruct();Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
    NeutralButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Neutral);RaisedButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Raised);
    ForwardButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Forward);SittingButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Sitting);
    ResetJointsButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ResetJoints);
    JointChoice->ClearOptions();for(const TCHAR* Label:JointLabels)JointChoice->AddOption(Label);
    JointChoice->SetSelectedIndex(0);JointChoice->OnSelectionChanged.AddUniqueDynamic(this,&ThisClass::JointSelected);
    for(auto* Slider:{PitchSlider.Get(),YawSlider.Get(),RollSlider.Get()})
    {Slider->OnValueChanged.AddUniqueDynamic(this,&ThisClass::JointChanged);Slider->OnMouseCaptureEnd.AddUniqueDynamic(this,&ThisClass::CommitJoint);Slider->OnControllerCaptureEnd.AddUniqueDynamic(this,&ThisClass::CommitJoint);}
    Session->OnChanged.AddUObject(this,&ThisClass::Refresh);Refresh();
}
void ULaceLoftPoseControlsWidget::NativeDestruct(){if(Session.IsValid()){Session->SaveSession();Session->OnChanged.RemoveAll(this);}Super::NativeDestruct();}
void ULaceLoftPoseControlsWidget::Neutral(){Session->SetPose(TEXT("Neutral"));}
void ULaceLoftPoseControlsWidget::Raised(){Session->SetPose(TEXT("ArmsRaised"));}
void ULaceLoftPoseControlsWidget::Forward(){Session->SetPose(TEXT("ArmsForward"));}
void ULaceLoftPoseControlsWidget::Sitting(){Session->SetPose(TEXT("Sitting"));}
void ULaceLoftPoseControlsWidget::ResetJoints(){Session->ResetJoints();}
void ULaceLoftPoseControlsWidget::JointSelected(FString,ESelectInfo::Type){Refresh();}
void ULaceLoftPoseControlsWidget::JointChanged(float)
{
    const int32 I=JointChoice->GetSelectedIndex();if(bRefreshing || I<0 || I>=UE_ARRAY_COUNT(JointIds))return;
    Session->SetJointRotation(JointIds[I],FRotator(PitchSlider->GetValue(),YawSlider->GetValue(),RollSlider->GetValue()),false);
}
void ULaceLoftPoseControlsWidget::CommitJoint(){if(Session.IsValid()){Session->SaveSession();Session->OnChanged.Broadcast();}}
void ULaceLoftPoseControlsWidget::Refresh()
{
    const int32 I=JointChoice->GetSelectedIndex();if(!Session.IsValid() || I<0 || I>=UE_ARRAY_COUNT(JointIds))return;
    bRefreshing=true;const auto R=Session->GetJointRotations().FindRef(JointIds[I]);
    const auto* Limits=Session->GetJointLimits();const auto* Range=Limits?Limits->Joints.Find(JointIds[I]):nullptr;
    if(Range && Range->IsValid())
    {
        PitchSlider->SetMinValue(Range->Minimum.Pitch);PitchSlider->SetMaxValue(Range->Maximum.Pitch);
        YawSlider->SetMinValue(Range->Minimum.Yaw);YawSlider->SetMaxValue(Range->Maximum.Yaw);
        RollSlider->SetMinValue(Range->Minimum.Roll);RollSlider->SetMaxValue(Range->Maximum.Roll);
    }
    PitchSlider->SetValue(R.Pitch);YawSlider->SetValue(R.Yaw);RollSlider->SetValue(R.Roll);
    JointValueLabel->SetText(FText::FromString(FString::Printf(TEXT("앞뒤 %.0f° · 좌우 %.0f° · 비틀기 %.0f°"),R.Pitch,R.Yaw,R.Roll)));bRefreshing=false;
}
void ULaceLoftFittingWidget::NativeConstruct()
{
    Super::NativeConstruct();Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
    Fitting=NewObject<ULaceLoftFittingController>(this); // Screen controller only; never a widget hierarchy.
    if(!Fitting->Initialize(GetOwningPlayer()))FittingStatus->SetText(FText::FromString(TEXT("인형 애셋을 불러오지 못했습니다.")));
    RemoveTopButton->OnClicked.AddUniqueDynamic(this,&ThisClass::RemoveTop);RemoveBottomButton->OnClicked.AddUniqueDynamic(this,&ThisClass::RemoveBottom);
    WireframeButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleWireframe);
    ResetCameraButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ResetCamera);Session->OnChanged.AddUObject(this,&ThisClass::Refresh);Refresh();
}
void ULaceLoftFittingWidget::NativeDestruct()
{bOrbiting=false;if(Fitting)Fitting->Shutdown();Fitting=nullptr;if(Session.IsValid())Session->OnChanged.RemoveAll(this);Super::NativeDestruct();}
void ULaceLoftFittingWidget::RemoveTop(){Session->Unequip(ELaceLoftGarmentSlot::Top);}
void ULaceLoftFittingWidget::RemoveBottom(){Session->Unequip(ELaceLoftGarmentSlot::Bottom);}
void ULaceLoftFittingWidget::ResetCamera(){Fitting->ResetCamera();}
void ULaceLoftFittingWidget::ToggleWireframe()
{
    auto* Dummy=Fitting?Fitting->GetDummy():nullptr;if(!Dummy)return;
    if(!Dummy->SetWireframeVisible(!Dummy->IsWireframeVisible()))
    {FittingStatus->SetText(FText::FromString(TEXT("와이어프레임 재질을 불러오지 못했습니다.")));return;}
    Refresh();
}
void ULaceLoftFittingWidget::Equip(FName,FGuid Id){Session->Equip(Id);}
void ULaceLoftFittingWidget::Refresh()
{
    if(!Session.IsValid())return;GarmentList->ClearChildren();int32 Count=0;
    auto* Dummy=Fitting?Fitting->GetDummy():nullptr;
    WireframeButton->SetIsEnabled(Dummy!=nullptr);
    WireframeButtonCaption->SetText(FText::FromString(Dummy && Dummy->IsWireframeVisible()?TEXT("와이어프레임: 켜짐"):TEXT("와이어프레임: 꺼짐")));
    for(const auto& Project:Session->GetProjects())if(Session->CanEquip(Project.ProjectId) && GarmentEntryClass)
    {
        ++Count;auto* Entry=CreateWidget<ULaceLoftSelectionEntryWidget>(GetOwningPlayer(),GarmentEntryClass);GarmentList->AddChild(Entry);
        auto* Pattern=Session->GetPattern(Project.PatternId);const bool Worn=Session->GetEquipped(Pattern->Slot)==Project.ProjectId;
        Entry->Configure(FText::FromString((Worn?TEXT("착용 중 · "):TEXT(""))+Pattern->DisplayName.ToString()+TEXT(" ")+Project.ProjectId.ToString(EGuidFormats::Digits).Left(4)),NAME_None,Project.ProjectId,true);
        Entry->OnSelected.BindUObject(this,&ThisClass::Equip);
    }
    RemoveTopButton->SetIsEnabled(Session->GetEquipped(ELaceLoftGarmentSlot::Top).IsValid());RemoveBottomButton->SetIsEnabled(Session->GetEquipped(ELaceLoftGarmentSlot::Bottom).IsValid());
    const FString Guide=Count?TEXT("가운데에서 드래그: 회전 · 휠: 확대/축소"):TEXT("완성한 의상이 없습니다. 제작 화면에서 재단과 봉제를 마쳐주세요.");
    FittingStatus->SetText(FText::FromString(Guide+TEXT("\n")+Session->GetStatusMessage().ToString()));
}
FReply ULaceLoftFittingWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    if(E.GetEffectingButton()==EKeys::LeftMouseButton && CameraInput->GetCachedGeometry().IsUnderLocation(E.GetScreenSpacePosition()))
    {LastPointer=G.AbsoluteToLocal(E.GetScreenSpacePosition());bOrbiting=true;return FReply::Handled().CaptureMouse(TakeWidget());}
    return FReply::Unhandled();
}
FReply ULaceLoftFittingWidget::NativeOnMouseButtonUp(const FGeometry&,const FPointerEvent&)
{if(!bOrbiting)return FReply::Unhandled();bOrbiting=false;return FReply::Handled().ReleaseMouseCapture();}
FReply ULaceLoftFittingWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{if(!bOrbiting)return FReply::Unhandled();const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());Fitting->Orbit(P-LastPointer);LastPointer=P;return FReply::Handled();}
FReply ULaceLoftFittingWidget::NativeOnMouseWheel(const FGeometry&,const FPointerEvent& E)
{if(!CameraInput->GetCachedGeometry().IsUnderLocation(E.GetScreenSpacePosition()))return FReply::Unhandled();Fitting->Zoom(E.GetWheelDelta());return FReply::Handled();}
void ULaceLoftFittingWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& E){bOrbiting=false;Super::NativeOnMouseCaptureLost(E);}
