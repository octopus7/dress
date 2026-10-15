#include "Development/LaceLoftClothProbeActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "Animation/AnimSequence.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ClothingSystemRuntimeTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ALaceLoftClothProbeActor::ALaceLoftClothProbeActor()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Dummy"));
    SetRootComponent(Body);
    Cloth=CreateDefaultSubobject<UChaosClothComponent>(TEXT("Cloth"));
    Cloth->SetupAttachment(Body);
    OtherCloth=CreateDefaultSubobject<UChaosClothComponent>(TEXT("OtherCloth"));OtherCloth->SetupAttachment(Body);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("ProbeCamera"));
    Camera->SetupAttachment(Body);
    Camera->ProjectionMode=ECameraProjectionMode::Orthographic;
    Camera->OrthoWidth=30;
    Camera->bAutoCalculateOrthoPlanes=false;
    Camera->OrthoNearClipPlane=.1f;
    Camera->OrthoFarClipPlane=1000.f;
    Camera->SetRelativeLocation(FVector(35,0,7.5));
    Camera->SetRelativeRotation(FRotator(0,180,0));
    Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Camera->PostProcessSettings.AutoExposureMethod=AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    Camera->PostProcessSettings.bOverride_AutoExposureBias=true;
    Camera->PostProcessSettings.AutoExposureBias=1;
    Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;
    Camera->PostProcessSettings.MotionBlurAmount=0;
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ALaceLoftClothProbeActor::BeginPlay()
{
    Super::BeginPlay();
    if(FParse::Value(FCommandLine::Get(),TEXT("GarmentProbe="),GarmentProbe))
    {
        const FString PrimaryGarment=GarmentProbe==TEXT("Both")?TEXT("TShirt"):GarmentProbe;
        const FString Path=TEXT("/Game/Garments/Cloth/CA_")+PrimaryGarment+TEXT(".CA_")+PrimaryGarment;
        Cloth->EmptyOverrideMaterials();
        Cloth->SetAsset(LoadObject<UChaosClothAsset>(nullptr,*Path));
        if(GarmentProbe==TEXT("Both"))OtherCloth->SetAsset(LoadObject<UChaosClothAsset>(nullptr,TEXT("/Game/Garments/Cloth/CA_DolphinShorts.CA_DolphinShorts")));
        PoseCount=4;Camera->OrthoWidth=20;Camera->PostProcessSettings.AutoExposureBias=0;
    }
    Body->SetForcedLOD(1); Cloth->SetForcedLOD(1);
    Body->PlayAnimation(NeutralPose,true);
    AddTickPrerequisiteComponent(Cloth);
    AddTickPrerequisiteComponent(OtherCloth);
    if(APlayerController* PC=GetWorld()->GetFirstPlayerController())
    {
        PC->bAutoManageActiveCameraTarget=false;
        PC->SetViewTarget(this);
    }
    bProbe=FParse::Param(FCommandLine::Get(),TEXT("LaceLoftClothProbe"));
    ReportDirectory=FPaths::ProjectSavedDir()/TEXT("ClothProbeRun");
    FParse::Value(FCommandLine::Get(),TEXT("ProbeReportDir="),ReportDirectory);
    ReportDirectory=FPaths::ConvertRelativePathToFull(ReportDirectory);
    if(bProbe)IFileManager::Get().MakeDirectory(*ReportDirectory,true);
}

void ALaceLoftClothProbeActor::Sample(float DeltaSeconds)
{
    TArray<FVector> Current;DynamicParticles=0;
    UPhysicsAsset* Physics=Body->GetPhysicsAsset();
    for(auto* Component:{Cloth.Get(),OtherCloth.Get()})
    {
    if(!Component->GetAsset())continue;
    Component->WaitForExistingParallelClothSimulation_GameThread();
    const auto* Proxy=Component->GetClothSimulationProxy();
    if(!Proxy || !Component->IsSimulationEnabled() || Component->IsSimulationSuspended()
        || Proxy->GetNumDynamicParticles()==0 || Proxy->GetNumIterations()==0 || Proxy->GetNumSubsteps()==0
        || Proxy->GetCurrentSimulationData_AnyThread().IsEmpty())
    {
        if(Elapsed>1)Errors.AddUnique(TEXT("Active cloth simulation/readback missing after initialization"));
        return;
    }
    const auto& Data=Proxy->GetCurrentSimulationData_AnyThread();
    const auto& Lod=CastChecked<UChaosClothAsset>(Component->GetAsset())->GetClothSimulationModel()->ClothSimulationLodModels[0];
    const auto* MaxDistances=Lod.WeightMaps.Find(TEXT("MaxDistance"));int32 ParticleIndex=0;
    DynamicParticles+=Proxy->GetNumDynamicParticles();
    for(const auto& Entry:Data)
        for(const auto& LocalPosition:Entry.Value.Positions)
        {
            const FVector P=Entry.Value.Transform.TransformPosition(FVector(LocalPosition));
            if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||!FMath::IsFinite(P.Z)||P.Size()>30)
                Errors.AddUnique(TEXT("Non-finite or exploded particle"));
            Current.Add(P);
            // Kinematic anchors follow authored skinning and cannot be pushed by collision.
            // The exported surface validator still checks every fixed and dynamic vertex.
            const bool Dynamic=!MaxDistances || !MaxDistances->IsValidIndex(ParticleIndex) || (*MaxDistances)[ParticleIndex]>0;
            ++ParticleIndex;
            if(!Dynamic)continue;
            if(Elapsed<4 || !Physics)continue;
            for(const USkeletalBodySetup* Setup:Physics->SkeletalBodySetups)
            {
                const FTransform Bone=Body->GetBoneTransform(Body->GetBoneIndex(Setup->BoneName));
                for(const FKSphylElem& Capsule:Setup->AggGeom.SphylElems)
                {
                    const FTransform Shape=Capsule.GetTransform()*Bone;
                    const FVector A=Shape.TransformPosition(FVector(0,0,-Capsule.Length*.5));
                    const FVector B=Shape.TransformPosition(FVector(0,0,Capsule.Length*.5));
                    const double Depth=Capsule.Radius-FVector::Distance(P,FMath::ClosestPointOnSegment(P,A,B));
                    if(Depth>MaximumPenetration){MaximumPenetration=Depth;WorstBone=Setup->BoneName.ToString();WorstParticle=Current.Num()-1;WorstPosition=P;}
                }
                for(const FKSphereElem& Sphere:Setup->AggGeom.SphereElems)
                {
                    const double Depth=Sphere.Radius-FVector::Distance(P,Bone.TransformPosition(Sphere.Center));
                    if(Depth>MaximumPenetration){MaximumPenetration=Depth;WorstBone=Setup->BoneName.ToString();}
                }
            }
        }
    }
    if(!Previous.IsEmpty() && Current.Num()!=Previous.Num())Errors.AddUnique(TEXT("Particle count changed"));
    if(Current.Num()==Previous.Num())
        for(int32 I=0;I<Current.Num();++I)
        {
            const double Motion=FVector::Distance(Current[I],Previous[I]);
            MaximumMotion=FMath::Max(MaximumMotion,Motion);
            if(Elapsed>4)MaximumSettledStep=FMath::Max(MaximumSettledStep,Motion);
        }
    Previous=MoveTemp(Current); ++SampleCount;
}

void ALaceLoftClothProbeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bProbe || bFinished)return;
    if(!PendingScreenshot.IsEmpty())
    {
        if(const APlayerController* PC=GetWorld()->GetFirstPlayerController())
        {
            const auto& View=PC->PlayerCameraManager->GetCameraCacheView();
            UE_LOG(LogTemp,Display,TEXT("Captured camera cache location %s rotation %s ortho %.2f"),
                *View.Location.ToString(),*View.Rotation.ToString(),View.OrthoWidth);
        }
        FScreenshotRequest::RequestScreenshot(PendingScreenshot,false,false);
        PendingScreenshot.Reset();
    }
    Elapsed+=DeltaSeconds;
    Sample(DeltaSeconds);
    const int32 View=FMath::FloorToInt(Elapsed-6.0);
    if(View>=0 && View<3 && View!=LastView)
    {
        LastView=View;
        const FVector Locations[]={FVector(35,0,7.5),FVector(0,35,7.5),FVector(-35,0,7.5)};
        Camera->SetRelativeLocation(Locations[View]);
        Camera->SetRelativeRotation((FVector(0,0,7.5)-Locations[View]).Rotation());
        if(APlayerController* PC=GetWorld()->GetFirstPlayerController())PC->SetViewTarget(this);
        UE_LOG(LogTemp,Display,TEXT("Probe camera %s, direction %s; body %s"),
            *Camera->GetComponentLocation().ToString(),*Camera->GetForwardVector().ToString(),*Body->Bounds.ToString());
        if(PoseIndex==0 && View==0)
            for(const USkeletalBodySetup* Setup:Body->GetPhysicsAsset()->SkeletalBodySetups)
                for(const FKSphylElem& Capsule:Setup->AggGeom.SphylElems)
                {
                    const FTransform Shape=Capsule.GetTransform()*Body->GetBoneTransform(Body->GetBoneIndex(Setup->BoneName));
                    UE_LOG(LogTemp,Display,TEXT("Probe collider %s center %s radius %.5f length %.5f axis %s"),
                        *Setup->BoneName.ToString(),*Shape.GetLocation().ToString(),Capsule.Radius,Capsule.Length,
                        *Shape.GetUnitAxis(EAxis::Z).ToString());
                }
        PendingScreenshot=ReportDirectory/FString::Printf(TEXT("Pose%d_View%d.png"),PoseIndex,View);
    }
    if(Elapsed>=10)
    {
        auto Result=MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("pose"),PoseIndex);
        Result->SetNumberField(TEXT("samples"),SampleCount);
        Result->SetNumberField(TEXT("particles"),Previous.Num());
        Result->SetNumberField(TEXT("dynamic_particles"),DynamicParticles);
        Result->SetNumberField(TEXT("max_motion_cm"),MaximumMotion);
        Result->SetNumberField(TEXT("max_settled_step_cm"),MaximumSettledStep);
        Result->SetNumberField(TEXT("max_penetration_cm"),MaximumPenetration);
        Result->SetStringField(TEXT("worst_collision_bone"),WorstBone);
        Result->SetNumberField(TEXT("worst_particle"),WorstParticle);
        Result->SetStringField(TEXT("worst_position_cm"),WorstPosition.ToString());
        PoseResults.Add(MakeShared<FJsonValueObject>(Result));
        ExportSurfacePose();
        if(SampleCount<100 || MaximumMotion<.0001)Errors.AddUnique(TEXT("Insufficient evidence of moving simulated fabric"));
        if(MaximumSettledStep>.1)Errors.AddUnique(TEXT("Settled frame displacement exceeds 0.1cm"));
        if(MaximumPenetration>.12)Errors.AddUnique(TEXT("Body capsule penetration exceeds 0.12cm"));
        UE_LOG(LogTemp,Display,TEXT("Cloth probe pose %d: samples %d, dynamic %d, motion %.5f, settled %.5f, penetration %.5f (%s)"),
            PoseIndex,SampleCount,DynamicParticles,MaximumMotion,MaximumSettledStep,MaximumPenetration,*WorstBone);
        if(PoseIndex+1<PoseCount)
        {
            ++PoseIndex;Elapsed=0;LastView=-1;SampleCount=0;MaximumMotion=MaximumSettledStep=MaximumPenetration=0;
            Previous.Reset();WorstBone.Reset();
            WorstParticle=INDEX_NONE;WorstPosition=FVector::ZeroVector;
            UAnimSequence* Next=RaisedPose;
            if(PoseIndex>=2)
            {
                const FString Name=PoseIndex==2?TEXT("A_DummyArmsForward"):TEXT("A_DummySitting");
                Next=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Dummy/Animation/")+Name+TEXT(".")+Name));
            }
            Body->PlayAnimation(Next,true);Cloth->ForceNextUpdateTeleportAndReset();
            OtherCloth->ForceNextUpdateTeleportAndReset();
        }
        else FinishProbe();
    }
}

void ALaceLoftClothProbeActor::ExportSurfacePose()
{
#if WITH_EDITOR
    if(GarmentProbe.IsEmpty())return;
    // Snapshot at the measurement boundary; this diagnostic CPU skinning pass
    // is excluded from simulation timing and is not used by product gameplay.
    TArray<FFinalSkinVertex> Skin;Body->GetCPUSkinnedVertices(Skin,0);
    const auto* RenderData=Body->GetSkeletalMeshAsset()->GetResourceForRendering();
    if(!RenderData || Skin.IsEmpty()){Errors.AddUnique(TEXT("Body surface snapshot unavailable"));return;}
    FString BodyObj;
    for(const auto& Vertex:Skin)
    {
        const FVector P=Body->GetComponentTransform().TransformPosition(FVector(Vertex.Position));
        BodyObj+=FString::Printf(TEXT("v %.7f %.7f %.7f\n"),P.X,P.Y,P.Z);
    }
    const auto* Indices=RenderData->LODRenderData[0].MultiSizeIndexContainer.GetIndexBuffer();
    for(int32 I=0;I+2<Indices->Num();I+=3)
    {
        uint32 A=Indices->Get(I),B=Indices->Get(I+1),C=Indices->Get(I+2);
        // Preserve imported topology winding. Skinned shading normals do not
        // define a deformed face's geometric orientation; parity QA is independent of winding.
        BodyObj+=FString::Printf(TEXT("f %u %u %u\n"),A+1,B+1,C+1);
    }
    FString ClothObj;
    for(const FVector& P:Previous)ClothObj+=FString::Printf(TEXT("v %.7f %.7f %.7f\n"),P.X,P.Y,P.Z);
    const auto Faces=CastChecked<UChaosClothAsset>(Cloth->GetAsset())->GetClothSimulationModel()->GetIndices(0);
    for(int32 I=0;I+2<Faces.Num();I+=3)ClothObj+=FString::Printf(TEXT("f %u %u %u\n"),Faces[I]+1,Faces[I+1]+1,Faces[I+2]+1);
    if(OtherCloth->GetAsset())
    {
        const int32 PrimaryCount=CastChecked<UChaosClothAsset>(Cloth->GetAsset())->GetClothSimulationModel()->GetNumVertices(0);
        const auto OtherFaces=CastChecked<UChaosClothAsset>(OtherCloth->GetAsset())->GetClothSimulationModel()->GetIndices(0);
        for(int32 I=0;I+2<OtherFaces.Num();I+=3)ClothObj+=FString::Printf(TEXT("f %u %u %u\n"),OtherFaces[I]+PrimaryCount+1,OtherFaces[I+1]+PrimaryCount+1,OtherFaces[I+2]+PrimaryCount+1);
        const auto ExportPart=[&](const TCHAR* Name,int32 Start,int32 Count,TConstArrayView<uint32> PartFaces)
        {
            FString Obj;for(int32 I=Start;I<Start+Count;++I)Obj+=FString::Printf(TEXT("v %.7f %.7f %.7f\n"),Previous[I].X,Previous[I].Y,Previous[I].Z);
            for(int32 I=0;I+2<PartFaces.Num();I+=3)Obj+=FString::Printf(TEXT("f %u %u %u\n"),PartFaces[I]+1,PartFaces[I+1]+1,PartFaces[I+2]+1);
            FFileHelper::SaveStringToFile(Obj,*(ReportDirectory/FString::Printf(TEXT("Pose%d_%s.obj"),PoseIndex,Name)));
        };
        ExportPart(TEXT("Top"),0,PrimaryCount,Faces);ExportPart(TEXT("Bottom"),PrimaryCount,Previous.Num()-PrimaryCount,OtherFaces);
    }
    if(!FFileHelper::SaveStringToFile(BodyObj,*(ReportDirectory/FString::Printf(TEXT("Pose%d_Body.obj"),PoseIndex)))
        || !FFileHelper::SaveStringToFile(ClothObj,*(ReportDirectory/FString::Printf(TEXT("Pose%d_Cloth.obj"),PoseIndex))))
        Errors.AddUnique(TEXT("Body/cloth surface snapshot could not be saved"));
#endif
}

void ALaceLoftClothProbeActor::FinishProbe()
{
    bFinished=true;
    for(int32 Pose=0;Pose<PoseCount;++Pose)for(int32 View=0;View<3;++View)
        if(IFileManager::Get().FileSize(*(ReportDirectory/FString::Printf(TEXT("Pose%d_View%d.png"),Pose,View)))<=0)
            Errors.AddUnique(TEXT("A required GPU screenshot was not saved"));
    auto Result=MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("passed"),Errors.IsEmpty());Result->SetArrayField(TEXT("poses"),PoseResults);
    Result->SetStringField(TEXT("coordinate_unit"),TEXT("cm; actor scale 1"));
    Result->SetStringField(TEXT("capsule_metric_scope"),TEXT("dynamic particles only; full body surface validation includes kinematic anchors"));
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    for(const FString& Error:Errors)ErrorValues.Add(MakeShared<FJsonValueString>(Error));
    Result->SetArrayField(TEXT("errors"),ErrorValues);
    FString Text;FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
    const bool Saved=FFileHelper::SaveStringToFile(Text,*(ReportDirectory/TEXT("metrics.json")));
    UE_LOG(LogTemp,Display,TEXT("Cloth probe result %s; report %s"),Errors.IsEmpty()&&Saved?TEXT("PASS"):TEXT("FAIL"),*ReportDirectory);
    FPlatformMisc::RequestExitWithStatus(false,Errors.IsEmpty()&&Saved?0:1);
}
