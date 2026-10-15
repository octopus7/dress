#pragma once
#include "GameFramework/SaveGame.h"
#include "Garments/LaceLoftCuttingModel.h"
#include "LaceLoftSaveGame.generated.h"

USTRUCT()
struct LACELOFT_API FLaceLoftEditCommand
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TMap<FName,FLaceLoftPathProgress> Before;
    UPROPERTY(SaveGame) TMap<FName,FLaceLoftPathProgress> After;
    UPROPERTY(SaveGame) TMap<FName,FVector2D> PositionsBefore;
    UPROPERTY(SaveGame) TMap<FName,FVector2D> PositionsAfter;
};

USTRUCT(BlueprintType)
struct LACELOFT_API FLaceLoftGarmentProject
{
    GENERATED_BODY()
    UPROPERTY(SaveGame,BlueprintReadOnly) FGuid ProjectId;
    UPROPERTY(SaveGame,BlueprintReadOnly) FName PatternId;
    UPROPERTY(SaveGame) int32 PatternVersion=0;
    UPROPERTY(SaveGame) FString SourceHash;
    UPROPERTY(SaveGame) FString DummyMeasurementsVersion;
    UPROPERTY(SaveGame) TMap<FName,FLaceLoftPathProgress> Progress;
    UPROPERTY(SaveGame) TMap<FName,FVector2D> PiecePositions;
    UPROPERTY(SaveGame) TArray<FLaceLoftEditCommand> UndoCommands;
    UPROPERTY(SaveGame) TArray<FLaceLoftEditCommand> RedoCommands;
};

UCLASS()
class LACELOFT_API ULaceLoftSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 SchemaVersion=1;
    UPROPERTY(SaveGame) TArray<FLaceLoftGarmentProject> Projects;
    UPROPERTY(SaveGame) FGuid ActiveProjectId;
    UPROPERTY(SaveGame) FGuid EquippedTop;
    UPROPERTY(SaveGame) FGuid EquippedBottom;
    UPROPERTY(SaveGame) FName PoseId=TEXT("Neutral");
    UPROPERTY(SaveGame) TMap<FName,FRotator> JointRotations;
};
