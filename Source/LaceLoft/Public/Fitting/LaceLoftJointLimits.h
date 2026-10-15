#pragma once
#include "Engine/DataAsset.h"
#include "LaceLoftJointLimits.generated.h"

USTRUCT(BlueprintType)
struct FLaceLoftJointRange
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FRotator Minimum=FRotator(-90,-90,-90);
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FRotator Maximum=FRotator(90,90,90);
    bool IsValid() const {return !Minimum.ContainsNaN() && !Maximum.ContainsNaN() && Minimum.Pitch<=Maximum.Pitch && Minimum.Yaw<=Maximum.Yaw && Minimum.Roll<=Maximum.Roll;}
    FRotator Clamp(FRotator R) const {return FRotator(FMath::Clamp(R.Pitch,Minimum.Pitch,Maximum.Pitch),FMath::Clamp(R.Yaw,Minimum.Yaw,Maximum.Yaw),FMath::Clamp(R.Roll,Minimum.Roll,Maximum.Roll));}
};

UCLASS(BlueprintType)
class LACELOFT_API ULaceLoftJointLimits : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TMap<FName,FLaceLoftJointRange> Joints;
};
