#pragma once
#include "Commandlets/Commandlet.h"
#include "LaceGarmentFitCommandlet.generated.h"
UCLASS()
class ULaceGarmentFitCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    ULaceGarmentFitCommandlet();
    virtual int32 Main(const FString& Params) override;
};
