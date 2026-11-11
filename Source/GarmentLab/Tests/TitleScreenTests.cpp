#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "../TitleScreenWidget.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTitleScreenAssetTest, "GarmentLab.TitleScreen.SavedAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTitleScreenAssetTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Startup map"), GetDefault<UGameMapsSettings>()->EditorStartupMap.ToString(), FString(TEXT("/Game/Maps/MainMap.MainMap")));
    TestEqual(TEXT("Game map"), UGameMapsSettings::GetGameDefaultMap(), FString(TEXT("/Game/Maps/MainMap")));
    TestNotNull(TEXT("Saved MainMap"), LoadObject<UWorld>(nullptr, TEXT("/Game/Maps/MainMap.MainMap")));
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/UI/WBP_TitleScreen.WBP_TitleScreen"));
    if (!TestNotNull(TEXT("Saved editable Widget Blueprint"), Blueprint)) return false;
    TestNotNull(TEXT("Compiled widget class"), Blueprint->GeneratedClass.Get());
    TestTrue(TEXT("Correct native parent"), Blueprint->ParentClass == UTitleScreenWidget::StaticClass());
    TestTrue(TEXT("Blueprint compiles without binding errors"), Blueprint->Status != BS_Error);
    TestNotNull(TEXT("Saved editable root"), Blueprint->WidgetTree->RootWidget.Get());
    TestNotNull(TEXT("Saved exit button"), Cast<UButton>(Blueprint->WidgetTree->FindWidget(TEXT("ExitButton"))));
    UTextBlock* Title = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("Title")));
    if (TestNotNull(TEXT("Editable title text"), Title))
    {
        TestEqual(TEXT("Project title"), Title->GetText().ToString(), FString(TEXT("GarmentLab")));
    }
    UButton* Start = Cast<UButton>(Blueprint->WidgetTree->FindWidget(TEXT("StartButton")));
    if (TestNotNull(TEXT("Placeholder start button"), Start))
    {
        TestFalse(TEXT("Unimplemented start action is disabled"), Start->GetIsEnabled());
    }
    return true;
}
#endif
