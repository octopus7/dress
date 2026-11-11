#include "TitleScreenWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"

void UTitleScreenWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (ensureMsgf(ExitButton, TEXT("WBP_TitleScreen must contain ExitButton.")))
    {
        ExitButton->OnClicked.AddUniqueDynamic(this, &UTitleScreenWidget::HandleExit);
    }
}

void UTitleScreenWidget::HandleExit()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
