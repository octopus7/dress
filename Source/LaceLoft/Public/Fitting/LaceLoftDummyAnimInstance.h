#pragma once
#include "Animation/AnimInstance.h"
#include "LaceLoftDummyAnimInstance.generated.h"
class UAnimSequence;
UCLASS(Blueprintable)
class LACELOFT_API ULaceLoftDummyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> SelectedPose;
    UPROPERTY(Transient) TMap<FName,FRotator> JointOffsets;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
