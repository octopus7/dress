#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "Session/LaceLoftSaveGame.h"
#include "Garments/LaceLoftSewingModel.h"
#include "LaceLoftWardrobeSubsystem.generated.h"
class ULaceLoftPatternDefinition;
class ULaceLoftJointLimits;

UCLASS()
class LACELOFT_API ULaceLoftWardrobeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    FGuid CreateProject(FName PatternId);
    bool SelectProject(FGuid Id);
    const TArray<FLaceLoftGarmentProject>& GetProjects() const;
    const FLaceLoftGarmentProject* GetProject(FGuid Id) const;
    const FLaceLoftGarmentProject* GetCurrentProject() const;
    ULaceLoftPatternDefinition* GetPattern(FName Id);
    ULaceLoftJointLimits* GetJointLimits();
    FLaceLoftSewingModel* GetModel(FGuid Id);
    bool BeginCut(FName PieceId,FVector2D PiecePointMm);
    bool BeginSeam(FName SeamId,int32 SideAIndex,FVector2D PiecePointMm);
    bool UpdateStroke(FVector2D PiecePointMm);
    bool EndStroke();
    bool Undo();
    bool Redo();
    bool CompleteCurrentProjectCheat();
    bool MovePiece(FName PieceId,FVector2D OffsetMm);
    bool CanEquip(FGuid ProjectId);
    bool Equip(FGuid ProjectId);
    void Unequip(ELaceLoftGarmentSlot Slot);
    FGuid GetEquipped(ELaceLoftGarmentSlot Slot) const;
    bool SaveSession();
    bool LoadSession();
    FName GetPose() const;
    void SetPose(FName Pose);
    const TMap<FName,FRotator>& GetJointRotations() const;
    void SetJointRotation(FName Joint,FRotator Rotation,bool bSave=true);
    void ResetJoints();
    const FText& GetStatusMessage() const {return StatusMessage;}
    const FString& GetRecoverySlot() const {return RecoverySlot;}
    void SetSaveSlotForTesting(const FString& Slot) {SaveSlot=Slot;}
#if WITH_DEV_AUTOMATION_TESTS
    void SetFailSavePromotionForTesting(bool Fail) {bFailSavePromotion=Fail;}
#endif
    FSimpleMulticastDelegate OnChanged;
private:
    void EnsureState();
    FLaceLoftGarmentProject* MutableProject(FGuid Id);
    bool RebuildModel(FLaceLoftGarmentProject& Project);
    void RefreshEquipment();
    void Commit(FLaceLoftGarmentProject& Project,const FLaceLoftEditCommand& Command);
    bool PreserveRecovery(const TArray<uint8>& Bytes);
    UPROPERTY() TObjectPtr<ULaceLoftSaveGame> State;
    UPROPERTY() TMap<FName,TObjectPtr<ULaceLoftPatternDefinition>> Patterns;
    UPROPERTY() TObjectPtr<ULaceLoftJointLimits> JointLimits;
#if WITH_DEV_AUTOMATION_TESTS
    bool bFailSavePromotion=false;
#endif
    TMap<FGuid,FLaceLoftSewingModel> Models;
    FString SaveSlot=TEXT("LaceLoftWardrobe_v1");
    FString RecoverySlot;
    FText StatusMessage;
    FGuid StrokeProjectId;
    bool bStorageBlocked=false;
};
