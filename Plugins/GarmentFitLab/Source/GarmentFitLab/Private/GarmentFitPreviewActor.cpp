#include "GarmentFitPreviewActor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GarmentFitInputWidget.h"
#include "Rendering/SkeletalMeshRenderData.h"

#define LOCTEXT_NAMESPACE "GarmentFitLab"

namespace
{
bool HasDrawableSurface(const USkeletalMesh* Mesh)
{
    const FSkeletalMeshRenderData* Data = Mesh->GetResourceForRendering();
    if (Mesh->GetRefSkeleton().GetNum() == 0 || !Data || Data->LODRenderData.IsEmpty())
    {
        return false;
    }
    const FSkeletalMeshLODRenderData& LOD = Data->LODRenderData[0];
    return LOD.GetNumVertices() > 0 && LOD.RenderSections.ContainsByPredicate(
        [](const FSkelMeshRenderSection& Section) { return Section.NumTriangles > 0; });
}
}

AGarmentFitPreviewActor::AGarmentFitPreviewActor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
    StaticPreview = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticPreview"));
    StaticPreview->SetupAttachment(RootComponent);
    StaticPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalPreview = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalPreview"));
    SkeletalPreview->SetupAttachment(RootComponent);
    SkeletalPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGarmentFitPreviewActor::RefreshInput()
{
    // Always clear the last input first, including on rejection or removal.
    StaticPreview->SetStaticMesh(nullptr);
    SkeletalPreview->SetDisablePostProcessBlueprint(true);
    SkeletalPreview->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    SkeletalPreview->SetAnimation(nullptr);
    SkeletalPreview->SetSkeletalMesh(nullptr);
    if (InputMesh.IsNull())
    {
        InputState = EGarmentFitInputState::Missing;
        InputMessage = LOCTEXT("Missing", "캐릭터 메시 대기 중\n레벨의 GarmentFitPreview 액터에서 Input Mesh를 지정하세요.");
    }
    else if (UObject* Input = InputMesh.LoadSynchronous())
    {
        UStaticMesh* StaticMesh = Cast<UStaticMesh>(Input);
        USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Input);
        if (!StaticMesh && !SkeletalMesh)
        {
            InputState = EGarmentFitInputState::UnsupportedType;
            InputMessage = LOCTEXT("Unsupported", "지원하지 않는 애셋입니다.\nStatic Mesh 또는 Skeletal Mesh를 지정하세요.");
        }
        else if ((StaticMesh && (StaticMesh->GetNumLODs() == 0 || StaticMesh->GetNumVertices(0) == 0 || StaticMesh->GetNumTriangles(0) == 0)) ||
            (SkeletalMesh && !HasDrawableSurface(SkeletalMesh)))
        {
            InputState = EGarmentFitInputState::EmptyMesh;
            InputMessage = LOCTEXT("Empty", "표시할 메시 데이터가 없습니다.\n메시 애셋의 LOD와 임포트 결과를 확인하세요.");
        }
        else
        {
            InputState = EGarmentFitInputState::Ready;
            if (StaticMesh)
            {
                StaticPreview->SetStaticMesh(StaticMesh);
                InputMessage = LOCTEXT("StaticReady", "Static Mesh 표시 준비됨\n크기·방향·몸 표면을 확인하세요. 체형 분석은 아직 없습니다.");
            }
            else
            {
                SkeletalPreview->SetSkeletalMesh(SkeletalMesh);
                InputMessage = LOCTEXT("SkeletalReady", "Skeletal Mesh 참조 포즈 표시 준비됨\n크기·방향·포즈를 확인하세요. 체형 분석은 아직 없습니다.");
            }
        }
    }
    else
    {
        InputState = EGarmentFitInputState::LoadFailed;
        InputMessage = LOCTEXT("LoadFailed", "메시를 불러올 수 없습니다.\n입력 애셋 경로와 파일을 확인하세요.");
    }
    if (InputWidget)
    {
        InputWidget->RefreshDisplay();
    }
}

void AGarmentFitPreviewActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshInput();
}

void AGarmentFitPreviewActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshInput();
    APlayerController* Player = GetWorld()->GetFirstPlayerController();
    if (!Player || !Player->IsLocalController())
    {
        return;
    }
    if (!InputWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GarmentFitLab: InputWidgetClass is missing. Assign the saved WBP_GarmentFitLab asset in the level."));
        return;
    }
    InputWidget = CreateWidget<UGarmentFitInputWidget>(Player, InputWidgetClass);
    if (InputWidget)
    {
        InputWidget->SetPreviewActor(this);
        InputWidget->AddToViewport();
    }
}

void AGarmentFitPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (InputWidget)
    {
        InputWidget->RemoveFromParent();
        InputWidget = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
