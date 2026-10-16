#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GarmentFitPreviewActor.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UGarmentFitInputWidget;

UENUM(BlueprintType)
enum class EGarmentFitInputState : uint8
{
    Missing,
    LoadFailed,
    UnsupportedType,
    EmptyMesh,
    Ready
};

/** Input belongs to the level; no character asset is assumed by this plugin. */
UCLASS()
class GARMENTFITLAB_API AGarmentFitPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    AGarmentFitPreviewActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garment Fit Lab|Input",
        meta=(AllowedClasses="/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
    TSoftObjectPtr<UObject> InputMesh;

    /** Assign the saved WBP in the level. Missing UI is reported, never generated. */
    UPROPERTY(EditAnywhere, Category="Garment Fit Lab|Preview")
    TSubclassOf<UGarmentFitInputWidget> InputWidgetClass;

    UFUNCTION(BlueprintCallable, CallInEditor, Category="Garment Fit Lab|Input")
    void RefreshInput();

    UFUNCTION(BlueprintPure, Category="Garment Fit Lab|Input")
    EGarmentFitInputState GetInputState() const { return InputState; }

    UFUNCTION(BlueprintPure, Category="Garment Fit Lab|Input")
    FText GetInputMessage() const { return InputMessage; }

    UStaticMeshComponent* GetStaticPreview() const { return StaticPreview; }
    USkeletalMeshComponent* GetSkeletalPreview() const { return SkeletalPreview; }

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UGarmentFitInputWidget> InputWidget;

    UPROPERTY(VisibleAnywhere, Category="Garment Fit Lab|Preview")
    TObjectPtr<UStaticMeshComponent> StaticPreview;

    UPROPERTY(VisibleAnywhere, Category="Garment Fit Lab|Preview")
    TObjectPtr<USkeletalMeshComponent> SkeletalPreview;

    UPROPERTY(VisibleInstanceOnly, Category="Garment Fit Lab|Input")
    EGarmentFitInputState InputState = EGarmentFitInputState::Missing;

    UPROPERTY(VisibleInstanceOnly, Category="Garment Fit Lab|Input")
    FText InputMessage;
};
