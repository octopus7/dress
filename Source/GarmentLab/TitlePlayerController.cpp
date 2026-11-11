#include "TitlePlayerController.h"
#include "TitleScreenWidget.h"
#include "UObject/ConstructorHelpers.h"

ATitlePlayerController::ATitlePlayerController()
{
    static ConstructorHelpers::FClassFinder<UTitleScreenWidget> WidgetClass(TEXT("/Game/UI/WBP_TitleScreen"));
    TitleScreenClass = WidgetClass.Class;
}

void ATitlePlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    if (!TitleScreenClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing /Game/UI/WBP_TitleScreen. Restore the saved Widget Blueprint."));
        return;
    }
    TitleScreen = CreateWidget<UTitleScreenWidget>(this, TitleScreenClass);
    if (!TitleScreen) return;
    TitleScreen->AddToViewport();
    bShowMouseCursor = true;
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(TitleScreen->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
}
