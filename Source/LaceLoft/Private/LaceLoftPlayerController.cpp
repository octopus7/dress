#include "LaceLoftPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "LaceLoftNavigationWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformMemory.h"
#include "RHI.h"
#if WITH_DEV_AUTOMATION_TESTS
bool LaceLoftProbeRoutedCut(UUserWidget* Screen,const FString& CaptureFile);
#endif

ALaceLoftPlayerController::ALaceLoftPlayerController()
{
    static ConstructorHelpers::FClassFinder<ULaceLoftNavigationWidget> TitleClass(
        TEXT("/Game/UI/WBP_Title.WBP_Title_C"));
    static ConstructorHelpers::FClassFinder<ULaceLoftNavigationWidget> CreationClass(
        TEXT("/Game/UI/WBP_GarmentCreation.WBP_GarmentCreation_C"));
    static ConstructorHelpers::FClassFinder<ULaceLoftNavigationWidget> FittingClass(
        TEXT("/Game/UI/WBP_FittingRoom.WBP_FittingRoom_C"));

    TitleWidgetClass = TitleClass.Class;
    CreationWidgetClass = CreationClass.Class;
    FittingWidgetClass = FittingClass.Class;
    bShowMouseCursor = true;
}

void ALaceLoftPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, true);
    TSubclassOf<ULaceLoftNavigationWidget> ScreenClass = TitleWidgetClass;
    if (CurrentLevel == TEXT("L_GarmentCreation"))
    {
        ScreenClass = CreationWidgetClass;
    }
    else if (CurrentLevel == TEXT("L_FittingRoom"))
    {
        ScreenClass = FittingWidgetClass;
    }

    if (!ensureMsgf(ScreenClass, TEXT("LaceLoft screen Widget Blueprint is missing for %s."), *CurrentLevel))
    {
        return;
    }

    NavigationWidget = CreateWidget<ULaceLoftNavigationWidget>(this, ScreenClass);
    if (!ensure(NavigationWidget))
    {
        return;
    }

    NavigationWidget->AddToViewport();
    bShowMouseCursor = true;
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    NavigationWidget->FocusInitialButton();

#if WITH_DEV_AUTOMATION_TESTS
    FString CaptureFile;
    if(FParse::Value(FCommandLine::Get(),TEXT("LaceLoftUICapture="),CaptureFile))
    {
        // Isolated development capture; never read or replace the player's save.
        auto* Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
        const FString CaptureSlot=TEXT("LaceLoft_UICapture_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FString SourceSlot;
        if(FParse::Value(FCommandLine::Get(),TEXT("UICaptureSourceSlot="),SourceSlot) && (SourceSlot.StartsWith(TEXT("LaceLoft_UIAutomation_")) || SourceSlot.StartsWith(TEXT("LaceLoft_UICapture_"))))
        {
            TArray<uint8> Bytes;if(UGameplayStatics::LoadDataFromSlot(Bytes,SourceSlot,0))UGameplayStatics::SaveDataToSlot(Bytes,CaptureSlot,0);
        }
        Session->SetSaveSlotForTesting(CaptureSlot);Session->LoadSession();
        auto CaptureMetadata=MakeShared<FJsonObject>();CaptureMetadata->SetStringField(TEXT("save_slot"),CaptureSlot);
        CaptureMetadata->SetStringField(TEXT("saved_directory"),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()));
        FString MetadataText;FJsonSerializer::Serialize(CaptureMetadata,TJsonWriterFactory<>::Create(&MetadataText));
        FFileHelper::SaveStringToFile(MetadataText,*(FPaths::GetPath(CaptureFile)/TEXT("capture-session.json")));
        CaptureReportDirectory=FPaths::GetPath(CaptureFile);CaptureStarted=FPlatformTime::Seconds();
        FString Garment=TEXT("TShirt");FParse::Value(FCommandLine::Get(),TEXT("UICaptureGarment="),Garment);
        if(Session->GetProjects().IsEmpty())Session->CreateProject(FName(Garment));
        for(const auto& Project:Session->GetProjects())if(Session->CanEquip(Project.ProjectId))Session->Equip(Project.ProjectId);
        FString Pose=TEXT("Neutral");FParse::Value(FCommandLine::Get(),TEXT("UICapturePose="),Pose);Session->SetPose(FName(Pose));
        FTimerHandle InputTimer,CaptureTimer,ExitTimer;
        const TWeakObjectPtr<UUserWidget> ProbeScreen=NavigationWidget;
        GetWorld()->GetTimerManager().SetTimer(InputTimer,[ProbeScreen,CaptureFile]()
        {if(ProbeScreen.IsValid() && !LaceLoftProbeRoutedCut(ProbeScreen.Get(),CaptureFile))UE_LOG(LogTemp,Error,TEXT("UI routed input probe failed"));},3.f,false);
        GetWorld()->GetTimerManager().SetTimer(CaptureTimer,[CaptureFile](){FScreenshotRequest::RequestScreenshot(CaptureFile,true,false);},5.f,false);
        GetWorld()->GetTimerManager().SetTimer(ExitTimer,[this](){WriteCapturePerformance();FPlatformMisc::RequestExit(false);},9.f,false);
    }
#endif
}

void ALaceLoftPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if WITH_DEV_AUTOMATION_TESTS
    const double Now=FPlatformTime::Seconds();
    if(!CaptureReportDirectory.IsEmpty() && LastCaptureFrame>CaptureStarted+5.5)CaptureFrameTimes.Add((Now-LastCaptureFrame)*1000.);
    LastCaptureFrame=Now;
#endif
}
void ALaceLoftPlayerController::WriteCapturePerformance()
{
#if WITH_DEV_AUTOMATION_TESTS
    if(CaptureFrameTimes.IsEmpty())return;CaptureFrameTimes.Sort();double Sum=0;for(double V:CaptureFrameTimes)Sum+=V;
    auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("frame_samples"),CaptureFrameTimes.Num());
    Report->SetNumberField(TEXT("mean_frame_ms"),Sum/CaptureFrameTimes.Num());Report->SetNumberField(TEXT("p95_frame_ms"),CaptureFrameTimes[FMath::FloorToInt((CaptureFrameTimes.Num()-1)*.95)]);
    Report->SetNumberField(TEXT("peak_resident_mb"),FPlatformMemory::GetStats().PeakUsedPhysical/1048576.);
    Report->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand());Report->SetStringField(TEXT("gpu"),GRHIAdapterName);
    Report->SetStringField(TEXT("scope"),TEXT("actual offscreen game frames after warmup; no frame cap; includes cloth and UMG"));
    FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(CaptureReportDirectory/TEXT("performance.json")));
#endif
}
