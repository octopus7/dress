#pragma once
#include "Commandlets/Commandlet.h"
#include "LaceGarmentBodyCommandlet.generated.h"

UCLASS()
class ULaceGarmentBodyCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    ULaceGarmentBodyCommandlet();
    virtual int32 Main(const FString& Params) override;
};
