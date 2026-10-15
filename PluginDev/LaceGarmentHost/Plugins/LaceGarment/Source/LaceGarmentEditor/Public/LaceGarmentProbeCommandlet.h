#pragma once
#include "Commandlets/Commandlet.h"
#include "LaceGarmentProbeCommandlet.generated.h"

UCLASS()
class ULaceGarmentProbeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    ULaceGarmentProbeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
