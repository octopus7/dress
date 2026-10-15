#pragma once
#include "Engine/DataAsset.h"
#include "Garments/LaceLoftGarmentTypes.h"
#include "LaceLoftPatternDefinition.generated.h"
class UChaosClothAsset;

UCLASS(BlueprintType)
class LACELOFT_API ULaceLoftPatternDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName PatternId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 PatternVersion=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FString DummyMeasurementsVersion;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FString SourceHash;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) ELaceLoftGarmentSlot Slot=ELaceLoftGarmentSlot::Top;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FLaceLoftPatternPiece> Pieces;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FLaceLoftSeamOperation> Seams;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TSoftObjectPtr<UChaosClothAsset> ClothAsset;
    const FLaceLoftPatternPiece* FindPiece(FName Id) const;
    const FLaceLoftSeamOperation* FindSeam(FName Id) const;
};

LACELOFT_API TArray<FString> ValidatePattern(const ULaceLoftPatternDefinition& Pattern);
LACELOFT_API bool BuildPatternStitches(const ULaceLoftPatternDefinition& Pattern,TArray<FIntPoint>& OutStitches,TArray<FString>& Errors);
