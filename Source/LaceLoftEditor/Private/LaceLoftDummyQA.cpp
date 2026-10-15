#include "Editor.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/DirectionalLight.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"

int32 AuthorDummyQA()
{
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if (!Mesh) return 1;
    UWorld* World = GEditor->NewMap();
    ASkeletalMeshActor* Actor = World->SpawnActor<ASkeletalMeshActor>();
    USkeletalMeshComponent* Body = Actor->GetSkeletalMeshComponent();
    Body->SetSkeletalMesh(Mesh);
    Body->SetUpdateAnimationInEditor(true);
    Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    for (const FRotator Angle : {FRotator(-30, 180, 0), FRotator(-20, 0, 0), FRotator(-15, 90, 0), FRotator(-15, -90, 0)})
    {
        ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>();
        Light->SetActorRotation(Angle);
        Light->GetLightComponent()->SetIntensity(Angle.Yaw == 180 ? 4.0f : 1.5f);
    }
    ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>();
    USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
    Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
    Capture->ShowFlags.SetAntiAliasing(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->OrthoWidth = 18.0f;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
    Capture->PostProcessSettings.AutoExposureBias = -1.0f;
    Capture->TextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(World, 900, 900, RTF_RGBA8);
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    TArray<FTransform> ComponentRef;
    for (int32 I=0; I<Ref.GetNum(); ++I)
    {
        const int32 Parent = Ref.GetParentIndex(I);
        ComponentRef.Add(Parent == INDEX_NONE ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * ComponentRef[Parent]);
        UE_LOG(LogTemp, Display, TEXT("Dummy reference %s: %s"), *Ref.GetBoneName(I).ToString(), *ComponentRef[I].GetTranslation().ToString());
    }
    const TCHAR* Poses[] = {TEXT("Neutral"), TEXT("ElbowKnee90"), TEXT("HeadTurn"), TEXT("ArmsRaised"), TEXT("ArmsForward"), TEXT("Sitting")};
    for (int32 Pose=0; Pose<UE_ARRAY_COUNT(Poses); ++Pose)
    {
        const FString Name = FString(TEXT("A_Dummy")) + Poses[Pose];
        UPackage* Package = CreatePackage(*(TEXT("/Game/Characters/Dummy/Animation/") + Name));
        Package->FullyLoad();
        UAnimSequence* Animation = FindObject<UAnimSequence>(Package, *Name);
        if (!Animation) Animation = NewObject<UAnimSequence>(Package, *Name, RF_Public | RF_Standalone);
        Animation->SetSkeleton(Mesh->GetSkeleton());
        IAnimationDataController& Controller = Animation->GetController();
        Controller.InitializeModel();
        Controller.OpenBracket(FText::FromString(TEXT("Authored dummy QA pose")), false);
        Controller.RemoveAllBoneTracks(false);
        Controller.SetFrameRate(FFrameRate(30,1), false);
        Controller.SetNumberOfFrames(30, false);
        for (int32 I=0; I<Ref.GetNum(); ++I)
        {
            FTransform Local = Ref.GetRefBonePose()[I];
            const FString Bone = Ref.GetBoneName(I).ToString();
            FVector Axis = FVector::ZeroVector; double Angle = 0;
            if ((Pose == 1 && (Bone.StartsWith(TEXT("forearm")) || Bone.StartsWith(TEXT("calf"))))
                || (Pose == 5 && Bone.StartsWith(TEXT("calf"))))
            {
                const FString ChildName = Bone.StartsWith(TEXT("forearm"))
                    ? Bone.Replace(TEXT("forearm"),TEXT("hand")) : Bone.Replace(TEXT("calf"),TEXT("foot"));
                const int32 Child = Ref.FindBoneIndex(*ChildName);
                const FVector Down = (ComponentRef[Child].GetTranslation()-ComponentRef[I].GetTranslation()).GetSafeNormal();
                const FVector BendTarget = Bone.StartsWith(TEXT("calf")) ? FVector(-1,0,0) : FVector(1,0,0);
                Axis = FVector::CrossProduct(Down, BendTarget).GetSafeNormal();
                if (Axis.IsNearlyZero()) Axis = FVector(0,1,0);
                Angle = PI/2;
            }
            if (Pose == 2 && Bone == TEXT("head")) { Axis = FVector(0,0,1); Angle = PI/4; }
            if (Pose == 3 && Bone.StartsWith(TEXT("upperarm")))
            { Axis = FVector(1,0,0); Angle = Bone.EndsWith(TEXT("_l")) ? -1.2 : 1.2; }
            if ((Pose == 4 && Bone.StartsWith(TEXT("upperarm"))) || (Pose == 5 && Bone.StartsWith(TEXT("thigh"))))
            {
                const FString ChildName=Pose==4?Bone.Replace(TEXT("upperarm"),TEXT("forearm")):Bone.Replace(TEXT("thigh"),TEXT("calf"));
                const FVector Direction=(ComponentRef[Ref.FindBoneIndex(*ChildName)].GetTranslation()-ComponentRef[I].GetTranslation()).GetSafeNormal();
                Axis=FVector::CrossProduct(Direction,FVector::ForwardVector).GetSafeNormal();
                Angle=FMath::Acos(FVector::DotProduct(Direction,FVector::ForwardVector));
            }
            if(Pose==5 && Bone==TEXT("root"))Local.SetTranslation(FVector(0,0,-2.72));
            if (Angle != 0)
            {
                const int32 Parent = Ref.GetParentIndex(I);
                const FQuat ParentRotation = ComponentRef[Parent].GetRotation();
                const FQuat LocalDelta = ParentRotation.Inverse() * FQuat(Axis,Angle) * ParentRotation;
                Local.SetRotation(LocalDelta * Local.GetRotation());
            }
            Controller.AddBoneCurve(Ref.GetBoneName(I), false);
            TArray<FVector> Positions, Scales; TArray<FQuat> Rotations;
            Positions.Init(Local.GetTranslation(),31); Scales.Init(FVector::OneVector,31); Rotations.Init(Local.GetRotation(),31);
            Controller.SetBoneTrackKeys(Ref.GetBoneName(I),Positions,Rotations,Scales,false);
        }
        Controller.NotifyPopulated(); Controller.CloseBracket(false);
        Animation->PostEditChange();
        FAssetRegistryModule::AssetCreated(Animation);
        FAssetCompilingManager::Get().FinishAllCompilation();
        FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Package,Animation,*Filename,SaveArgs)) return 2;
        Body->PlayAnimation(Animation,true);
        ++GFrameCounter; ++GFrameNumber;
        Body->TickAnimation(1.0f/30.0f,false); Body->RefreshBoneTransforms(); Body->UpdateComponentToWorld();
        Body->MarkRenderDynamicDataDirty();
        const FVector Views[] = {FVector(32,0,7.5),FVector(0,32,7.5),FVector(-32,0,7.5)};
        const TCHAR* Names[] = {TEXT("Front"),TEXT("Side"),TEXT("Back")};
        for(const FString Surface:{TEXT("White"),TEXT("Checker_2048"),TEXT("Checker_1024"),TEXT("Checker_512"),
                                  TEXT("Asymmetric_2048"),TEXT("Asymmetric_1024"),TEXT("Asymmetric_512")})
        {
        const FString MatName=TEXT("MI_Dummy")+Surface;
        auto* Material=LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/Characters/Dummy/Materials/")+MatName+TEXT(".")+MatName));
        if(!Material)return 3;
        Body->SetMaterial(0,Material);
        FAssetCompilingManager::Get().FinishAllCompilation();
        for (int32 View=0;View<3;++View)
        {
            ++GFrameCounter; ++GFrameNumber;
            ENQUEUE_RENDER_COMMAND(AdvanceQAFrame)([](FRHICommandListImmediate&) { ++GFrameNumberRenderThread; });
            CaptureActor->SetActorLocation(Views[View]);
            CaptureActor->SetActorRotation((FVector(0,0,7.5)-Views[View]).Rotation());
            World->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
            Capture->CaptureScene(); FlushRenderingCommands();
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
            ++GFrameCounter; ++GFrameNumber;
            ENQUEUE_RENDER_COMMAND(AdvanceQACaptureFrame)([](FRHICommandListImmediate&) { ++GFrameNumberRenderThread; });
            Capture->CaptureScene(); FlushRenderingCommands();
            UKismetRenderingLibrary::ExportRenderTarget(World,Capture->TextureTarget,
                FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("ArtSource/Dummy/QA")),
                FString::Printf(TEXT("S3_UE_%s_%s_%s.png"),*Surface,Poses[Pose],Names[View]));
        }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Dummy QA: saved 6 UE animations and 126 material/pose/view renders."));
    return 0;
}
