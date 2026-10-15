#include "LaceGarmentInspection.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "AssetCompilingManager.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"

bool LaceGarment::CaptureInspectionViews(UWorld* World,const FString& Directory,FVector Center,float OrthoWidth,const TArray<FVector>& Cameras)
{
    auto* Light=World->SpawnActor<ADirectionalLight>();
    Light->SetActorRotation(FRotator(-35,-45,0)); Light->GetLightComponent()->SetIntensity(6);
    auto* Fill=World->SpawnActor<ADirectionalLight>();
    Fill->SetActorRotation(FRotator(-15,135,0)); Fill->GetLightComponent()->SetIntensity(2);
    auto* Camera=World->SpawnActor<ASceneCapture2D>();
    UTextureRenderTarget2D* Target=NewObject<UTextureRenderTarget2D>();
    Target->RenderTargetFormat=RTF_RGBA8; Target->ClearColor=FLinearColor(0.025f,0.025f,0.025f);
    Target->InitAutoFormat(1024,1024); Target->UpdateResourceImmediate(true);
    auto* Capture=Camera->GetCaptureComponent2D(); Capture->TextureTarget=Target;
    Capture->ProjectionType=ECameraProjectionMode::Orthographic; Capture->OrthoWidth=OrthoWidth;
    Capture->CaptureSource=SCS_FinalColorLDR; Capture->bCaptureEveryFrame=false;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureBias=true; Capture->PostProcessSettings.AutoExposureBias=1;
    World->UpdateWorldComponents(true,false); World->SendAllEndOfFrameUpdates();
    FAssetCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
    for(int32 I=0; I<Cameras.Num(); ++I)
    {
        Camera->SetActorLocation(Cameras[I]); Camera->SetActorRotation((Center-Cameras[I]).Rotation());
        Capture->CaptureScene(); FlushRenderingCommands();
        FBufferArchive Png;
        if(!FImageUtils::ExportRenderTarget2DAsPNG(Target,Png) || !FFileHelper::SaveArrayToFile(Png,*(Directory/FString::Printf(TEXT("view-%d.png"),I)))) return false;
    }
    return true;
}
