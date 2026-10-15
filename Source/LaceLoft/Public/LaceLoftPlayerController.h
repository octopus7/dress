#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LaceLoftPlayerController.generated.h"

class ULaceLoftNavigationWidget;

UCLASS()
class LACELOFT_API ALaceLoftPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ALaceLoftPlayerController();
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    void WriteCapturePerformance();
#if WITH_DEV_AUTOMATION_TESTS
    FString CaptureReportDirectory;
    double CaptureStarted=0,LastCaptureFrame=0;
    TArray<double> CaptureFrameTimes;
#endif
    UPROPERTY()
    TSubclassOf<ULaceLoftNavigationWidget> TitleWidgetClass;

    UPROPERTY()
    TSubclassOf<ULaceLoftNavigationWidget> CreationWidgetClass;

    UPROPERTY()
    TSubclassOf<ULaceLoftNavigationWidget> FittingWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<ULaceLoftNavigationWidget> NavigationWidget;
};
