#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "LaceLoftClothProbeActor.generated.h"

class USkeletalMeshComponent;
class UChaosClothComponent;
class UCameraComponent;
class UAnimSequence;

UCLASS()
class LACELOFT_API ALaceLoftClothProbeGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ALaceLoftClothProbeGameMode() { DefaultPawnClass=nullptr; HUDClass=nullptr; }
};

// Development scene only. The product maps never spawn this actor.
UCLASS()
class LACELOFT_API ALaceLoftClothProbeActor : public AActor
{
    GENERATED_BODY()
public:
    ALaceLoftClothProbeActor();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UChaosClothComponent> Cloth;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UChaosClothComponent> OtherCloth;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> NeutralPose;
    UPROPERTY(EditAnywhere) TObjectPtr<UAnimSequence> RaisedPose;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void FinishProbe();
    void Sample(float DeltaSeconds);
    void ExportSurfacePose();
    bool bProbe=false;
    bool bFinished=false;
    int32 PoseIndex=0, LastView=-1, SampleCount=0, DynamicParticles=0;
    double Elapsed=0, MaximumMotion=0, MaximumSettledStep=0, MaximumPenetration=0;
    FString ReportDirectory, WorstBone, PendingScreenshot;
    TArray<FString> Errors;
    TArray<FVector> Previous;
    TArray<TSharedPtr<class FJsonValue>> PoseResults;
    FString GarmentProbe;
    int32 PoseCount=2;
    int32 WorstParticle=INDEX_NONE;
    FVector WorstPosition=FVector::ZeroVector;
};
