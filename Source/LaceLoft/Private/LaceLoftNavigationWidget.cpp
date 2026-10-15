#include "LaceLoftNavigationWidget.h"

#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Engine/GameInstance.h"

void ULaceLoftNavigationWidget::NativeConstruct()
{
    Super::NativeConstruct();

    check(TitleButton && CreationButton && FittingButton);
    TitleButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTitleClicked);
    CreationButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreationClicked);
    FittingButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFittingClicked);

    bTravelPending = false;
}

void ULaceLoftNavigationWidget::NativeDestruct()
{
    if (TitleButton)
    {
        TitleButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleTitleClicked);
    }
    if (CreationButton)
    {
        CreationButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCreationClicked);
    }
    if (FittingButton)
    {
        FittingButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleFittingClicked);
    }

    Super::NativeDestruct();
}

void ULaceLoftNavigationWidget::FocusInitialButton()
{
    const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(this, true);
    UButton* PreferredButton = CurrentLevel == TEXT("L_Title") ? CreationButton.Get() : TitleButton.Get();
    UButton* Candidates[] = { PreferredButton, TitleButton.Get(), CreationButton.Get(), FittingButton.Get() };
    UButton* InitialButton = nullptr;
    for (UButton* Candidate : Candidates)
    {
        if (Candidate && Candidate->GetIsEnabled())
        {
            InitialButton = Candidate;
            break;
        }
    }

    if (APlayerController* Controller = GetOwningPlayer(); Controller && InitialButton)
    {
        InitialButton->SetUserFocus(Controller);
    }
}

void ULaceLoftNavigationWidget::HandleTitleClicked()
{
    TravelToLevel(TEXT("/Game/Maps/L_Title"));
}

void ULaceLoftNavigationWidget::HandleCreationClicked()
{
    TravelToLevel(TEXT("/Game/Maps/L_GarmentCreation"));
}

void ULaceLoftNavigationWidget::HandleFittingClicked()
{
    TravelToLevel(TEXT("/Game/Maps/L_FittingRoom"));
}

void ULaceLoftNavigationWidget::TravelToLevel(const FName LevelPackage)
{
    if (bTravelPending || !GetWorld())
    {
        return;
    }

    const FString DestinationName = FPackageName::GetShortName(LevelPackage.ToString());
    if (UGameplayStatics::GetCurrentLevelName(this, true) == DestinationName)
    {
        return;
    }

    if(auto* Session=GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>())
    {
        Session->EndStroke();
        if(!Session->SaveSession()){Session->OnChanged.Broadcast();return;}
    }
    bTravelPending = true;
    SetNavigationEnabled(false);
    UGameplayStatics::OpenLevel(this, LevelPackage);
}

void ULaceLoftNavigationWidget::SetNavigationEnabled(const bool bEnabled)
{
    TitleButton->SetIsEnabled(bEnabled);
    CreationButton->SetIsEnabled(bEnabled);
    FittingButton->SetIsEnabled(bEnabled);
}
