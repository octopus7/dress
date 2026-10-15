#pragma once
#include "Commandlets/Commandlet.h"
#include "LaceLoftAuthoringCommandlet.generated.h"

// Explicit offline authoring only. Never runs when playing or opening a map.
UCLASS()
class ULaceLoftAuthoringCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    ULaceLoftAuthoringCommandlet();
    virtual int32 Main(const FString& Params) override;
};
