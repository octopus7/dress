#include "Fitting/LaceLoftDummyActor.h"
#include "Fitting/LaceLoftDummyAnimInstance.h"
#include "Fitting/LaceLoftJointLimits.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

namespace
{
bool SaveFittingAsset(UObject* Asset)
{
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
}
int32 AuthorFittingAssets()
{
    auto* Limits=LoadObject<ULaceLoftJointLimits>(nullptr,TEXT("/Game/Characters/Dummy/Animation/DA_DummyJointLimits.DA_DummyJointLimits"),nullptr,LOAD_NoWarn);
    if(!Limits)
    {
        Limits=NewObject<ULaceLoftJointLimits>(CreatePackage(TEXT("/Game/Characters/Dummy/Animation/DA_DummyJointLimits")),TEXT("DA_DummyJointLimits"),RF_Public|RF_Standalone);
        for(const TCHAR* Joint:{TEXT("head"),TEXT("upperarm_l"),TEXT("upperarm_r"),TEXT("forearm_l"),TEXT("forearm_r"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r")})Limits->Joints.Add(Joint,FLaceLoftJointRange());
        FAssetRegistryModule::AssetCreated(Limits);
    }
    if(!SaveFittingAsset(Limits))return 8;
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));if(!Mesh)return 1;
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Dummy/Animation/ABP_Dummy.ABP_Dummy"),nullptr,LOAD_NoWarn);
    if(!BP)
    {
        auto* Factory=NewObject<UAnimBlueprintFactory>();Factory->ParentClass=ULaceLoftDummyAnimInstance::StaticClass();
        Factory->TargetSkeleton=Mesh->GetSkeleton();Factory->PreviewSkeletalMesh=Mesh;
        BP=Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),CreatePackage(TEXT("/Game/Characters/Dummy/Animation/ABP_Dummy")),TEXT("ABP_Dummy"),RF_Public|RF_Standalone,nullptr,GWarn));
        if(!BP)return 2;FAssetRegistryModule::AssetCreated(BP);
    }
    FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);
    if(Results.NumErrors || !SaveFittingAsset(BP))return 3;
    const FString MapFile=FPaths::ProjectContentDir()/TEXT("Maps/L_FittingRoom.umap");
    if(!FEditorFileUtils::LoadMap(MapFile,false,true))return 4;
    UWorld* World=GEditor->GetEditorWorldContext().World();ALaceLoftDummyActor* Actor=nullptr;
    for(TActorIterator<ALaceLoftDummyActor> It(World);It;++It){Actor=*It;break;}
    const bool IsNew=!Actor;if(!Actor)Actor=World->SpawnActor<ALaceLoftDummyActor>();
    Actor->SetActorLabel(TEXT("LaceLoft Fitting Dummy"));Actor->Body->SetSkeletalMesh(Mesh);
    Actor->Camera->FieldOfView=50;Actor->ResetCamera();
    auto* PoseAsset=LoadObject<UPoseAsset>(nullptr,TEXT("/Game/Characters/Dummy/Animation/PA_DummyPoses.PA_DummyPoses"),nullptr,LOAD_NoWarn);
    if(!PoseAsset)
    {PoseAsset=NewObject<UPoseAsset>(CreatePackage(TEXT("/Game/Characters/Dummy/Animation/PA_DummyPoses")),TEXT("PA_DummyPoses"),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(PoseAsset);}
    PoseAsset->SetSkeleton(Mesh->GetSkeleton());
    for(const TCHAR* Name:{TEXT("Neutral"),TEXT("ArmsRaised"),TEXT("ArmsForward"),TEXT("Sitting")})
    {
        const FString AssetName=TEXT("A_Dummy")+FString(Name);
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Dummy/Animation/")+AssetName+TEXT(".")+AssetName));if(!Sequence)return 5;
        Actor->Poses.Add(Name,Sequence);Actor->Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);Actor->Body->SetAnimation(Sequence);
        Actor->Body->SetPosition(0,false);Actor->Body->TickAnimation(0,false);Actor->Body->RefreshBoneTransforms();
        PoseAsset->AddOrUpdatePose(Name,Actor->Body,false);
    }
    if(PoseAsset->GetNumPoses()!=4 || !SaveFittingAsset(PoseAsset))return 6;
    Actor->Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);Actor->Body->SetAnimInstanceClass(BP->GeneratedClass);
    if(IsNew)for(const FRotator Angle:{FRotator(-30,180,0),FRotator(-20,0,0),FRotator(-15,90,0),FRotator(-15,-90,0)})
    {
        auto* Light=World->SpawnActor<ADirectionalLight>();Light->SetActorLabel(TEXT("LaceLoft Studio Light"));
        Light->SetActorRotation(Angle);Light->GetLightComponent()->SetIntensity(Angle.Yaw==180?4.f:1.5f);Light->SetMobility(EComponentMobility::Movable);
    }
    int32 LightIndex=0;for(TActorIterator<ADirectionalLight> It(World);It;++It)
        CastChecked<UDirectionalLightComponent>(It->GetLightComponent())->ForwardShadingPriority=LightIndex++==0?1:0;
    return FEditorFileUtils::SaveMap(World,MapFile)?0:7;
}
