#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "PlayInEditorDataTypes.h"
#include "Tests/AutomationEditorCommon.h"
#include "WidgetBlueprint.h"
#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "Algo/Reverse.h"

namespace LaceLoftTests
{
struct FScreen
{
    const TCHAR* Map;
    const TCHAR* Widget;
    const TCHAR* Button;
};

static const FScreen Screens[] =
{
    { TEXT("L_Title"), TEXT("WBP_Title"), TEXT("TitleButton") },
    { TEXT("L_GarmentCreation"), TEXT("WBP_GarmentCreation"), TEXT("CreationButton") },
    { TEXT("L_FittingRoom"), TEXT("WBP_FittingRoom"), TEXT("FittingButton") }
};

// This circuit covers every directed route between distinct screens and revisits
// each screen. It invokes the same multicast delegate as a Slate button click.
static const int32 Route[] = { 0, 1, 2, 0, 2, 1, 0 };

static void Trace(ULaceLoftWardrobeSubsystem* Session,const FLaceLoftPatternPath& Path,bool End=true)
{
    for(int32 I=1;I<Path.PointsMm.Num();++I)
    {
        const int32 Count=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Path.PointsMm[I-1],Path.PointsMm[I])));
        for(int32 J=1;J<=Count;++J)Session->UpdateStroke(FMath::Lerp(Path.PointsMm[I-1],Path.PointsMm[I],double(J)/Count));
    }
    if(End)Session->EndStroke();
}
static void FinishCuts(ULaceLoftWardrobeSubsystem* Session,ULaceLoftPatternDefinition* Pattern)
{
    for(const auto& Piece:Pattern->Pieces)
        if(Session->BeginCut(Piece.PieceId,Piece.CutOutline.PointsMm[0]))Trace(Session,Piece.CutOutline);
}
static void FinishSeams(ULaceLoftWardrobeSubsystem* Session,ULaceLoftPatternDefinition* Pattern,FGuid Id)
{
    for(int32 Pass=0;Pass<Pattern->Seams.Num();++Pass)for(const auto& Seam:Pattern->Seams)
        if(Session->GetModel(Id)->CanStartSeam(Seam.SeamId))for(int32 I=0;I<Seam.SideA.Num();++I)
        {
            const auto& Ref=Seam.SideA[I];auto Path=Pattern->FindPiece(Ref.PieceId)->FindEdge(Ref.EdgeId)->Path;
            if(Ref.Direction<0)Algo::Reverse(Path.PointsMm);
            if(Session->BeginSeam(Seam.SeamId,I,Path.PointsMm[0]))Trace(Session,Path);
        }
}

class FNavigateScreens final : public IAutomationLatentCommand
{
public:
    explicit FNavigateScreens(FAutomationTestBase* InTest)
        : Test(InTest), StepStartedAt(FPlatformTime::Seconds())
    {
    }

    virtual bool Update() override
    {
        UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
        const FScreen& Expected = Screens[Route[Step]];
        if (!World || !World->HasBegunPlay()
            || UGameplayStatics::GetCurrentLevelName(World, true) != Expected.Map)
        {
            return WaitOrFail(FString::Printf(TEXT("PIE did not reach %s"), Expected.Map));
        }

        UClass* ExpectedClass = LoadClass<UUserWidget>(nullptr,
            *FString::Printf(TEXT("/Game/UI/%s.%s_C"), Expected.Widget, Expected.Widget));
        if (!ExpectedClass)
        {
            Test->AddError(FString::Printf(TEXT("Missing generated widget class %s"), Expected.Widget));
            return true;
        }

        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), true);
        UUserWidget* ActiveWidget = nullptr;
        int32 NavigationWidgetCount = 0;
        for (UUserWidget* Widget : Widgets)
        {
            if (Widget && Widget->GetWorld() == World && Widget->IsInViewport())
            {
                ++NavigationWidgetCount;
                if (Widget->IsA(ExpectedClass))
                {
                    ActiveWidget = Widget;
                }
            }
        }
        if (!ActiveWidget)
        {
            return WaitOrFail(FString::Printf(TEXT("%s did not display its authored widget"), Expected.Map));
        }
        if(!bIsolatedSave)
        {
            auto* Session=World->GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
            SaveSlot=TEXT("LaceLoft_NavigationAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
            Session->SetSaveSlotForTesting(SaveSlot);
            Session->LoadSession();bIsolatedSave=true;
        }

        Test->TestEqual(FString::Printf(TEXT("%s has exactly one viewport widget"), Expected.Map),
            NavigationWidgetCount, 1);
        APlayerController* Controller = World->GetFirstPlayerController();
        Test->TestNotNull(TEXT("Navigation has a player controller"), Controller);
        if (Controller)
        {
            Test->TestTrue(TEXT("Navigation displays the mouse cursor"), Controller->bShowMouseCursor);
            Test->TestNull(TEXT("Empty navigation levels do not spawn a pawn"), Controller->GetPawn());
            Test->TestNull(TEXT("Navigation uses UMG without a HUD actor"), Controller->GetHUD());
            const FName FocusButtonName = Route[Step] == 0 ? TEXT("CreationButton") : TEXT("TitleButton");
            UButton* FocusButton = Cast<UButton>(ActiveWidget->GetWidgetFromName(FocusButtonName));
            if (Test->TestNotNull(TEXT("Initial navigation focus button exists"), FocusButton))
            {
                Test->TestTrue(TEXT("An enabled destination button receives keyboard focus"),
                    FocusButton->GetIsEnabled() && FocusButton->HasUserFocus(Controller));
            }
        }
        UButton* CurrentButton = Cast<UButton>(ActiveWidget->GetWidgetFromName(Expected.Button));
        if (Test->TestNotNull(TEXT("Current screen button exists"), CurrentButton))
        {
            Test->TestFalse(TEXT("Current screen button keeps its disabled Designer default"), CurrentButton->GetIsEnabled());
        }

        auto* Session=World->GetGameInstance()->GetSubsystem<ULaceLoftWardrobeSubsystem>();
        if(Step==1)
        {
            if(!Test->TestNotNull(TEXT("Title New created a project"),Session->GetCurrentProject()))return true;
            Top=Session->GetCurrentProject()->ProjectId;auto* Pattern=Session->GetPattern(TEXT("TShirt"));
            const auto& Piece=Pattern->Pieces[0];CutId=Piece.CutOutline.PathId;
            auto Partial=Piece.CutOutline;Partial.PointsMm.SetNum(2);
            Session->BeginCut(Piece.PieceId,Partial.PointsMm[0]);Trace(Session,Partial,false);
            ExpectedCoverage=Session->GetModel(Top)->Progress(CutId).CoveredMm();
        }
        if(Step==2 || Step==4 || Step==6)
        {
            // Fresh subsystem reads disk, rather than merely inspecting the persistent GameInstance.
            auto* Fresh=NewObject<ULaceLoftWardrobeSubsystem>(World->GetGameInstance());Fresh->SetSaveSlotForTesting(SaveSlot);
            Test->TestTrue(TEXT("Travel saved a reloadable command boundary"),Fresh->LoadSession());
            if(Step==2)Test->TestEqual(TEXT("Open cut stroke survives travel and disk reload"),Fresh->GetModel(Top)->Progress(CutId).CoveredMm(),ExpectedCoverage);
            if(Step==4)Test->TestEqual(TEXT("Open seam stroke survives travel and disk reload"),Fresh->GetModel(Top)->Progress(SeamPathId).CoveredMm(),ExpectedCoverage);
            if(Step==6)
            {
                Test->TestFalse(TEXT("Completed then undone top remains unqualified after restart"),Fresh->CanEquip(Top));
                Test->TestFalse(TEXT("Undone top stays unequipped after restart"),Fresh->GetEquipped(ELaceLoftGarmentSlot::Top).IsValid());
                Test->TestTrue(TEXT("Other completed garment survives restart"),Fresh->CanEquip(Bottom));
                Test->TestEqual(TEXT("Other garment remains equipped"),Fresh->GetEquipped(ELaceLoftGarmentSlot::Bottom),Bottom);
            }
        }
        if(Step==3)
        {
            auto* Pattern=Session->GetPattern(TEXT("TShirt"));FinishCuts(Session,Pattern);
            for(const auto& Seam:Pattern->Seams)if(Session->GetModel(Top)->CanStartSeam(Seam.SeamId))
            {
                const auto& Ref=Seam.SideA[0];auto Partial=Pattern->FindPiece(Ref.PieceId)->FindEdge(Ref.EdgeId)->Path;
                if(Ref.Direction<0)Algo::Reverse(Partial.PointsMm);Partial.PointsMm.SetNum(2);
                Session->BeginSeam(Seam.SeamId,0,Partial.PointsMm[0]);Trace(Session,Partial,false);
                SeamPathId=FLaceLoftSewingModel::SeamPathId(Seam.SeamId,0);ExpectedCoverage=Session->GetModel(Top)->Progress(SeamPathId).CoveredMm();break;
            }
        }
        if(Step==4)
        {
            auto* Pattern=Session->GetPattern(TEXT("TShirt"));FinishSeams(Session,Pattern,Top);
            Test->TestTrue(TEXT("Travelled top can finish and equip"),Session->Equip(Top));
            Bottom=Session->CreateProject(TEXT("DolphinShorts"));Pattern=Session->GetPattern(TEXT("DolphinShorts"));
            FinishCuts(Session,Pattern);FinishSeams(Session,Pattern,Bottom);
            Test->TestTrue(TEXT("Second garment can finish and equip independently"),Session->Equip(Bottom));
            Session->SelectProject(Top);
        }
        if(Step==5)
        {
            Test->TestTrue(TEXT("Completed top survives creation revisit"),Session->CanEquip(Top));
            auto* Undo=Cast<UButton>(ActiveWidget->GetWidgetFromName(TEXT("UndoButton")));
            if(Test->TestNotNull(TEXT("Creation revisit exposes saved Undo"),Undo))Undo->OnClicked.Broadcast();
        }
        if (Step == UE_ARRAY_COUNT(Route) - 1)
        {
            Test->AddInfo(TEXT("All six navigation routes and repeated visits succeeded in PIE."));
            return true;
        }

        const FScreen& Destination = Screens[Route[Step + 1]];
        UButton* Button = Cast<UButton>(ActiveWidget->GetWidgetFromName(Destination.Button));
        if (!Test->TestNotNull(TEXT("Destination button exists in the serialized widget"), Button))
        {
            return true;
        }
        if (!Test->TestTrue(TEXT("Destination button is enabled"), Button->GetIsEnabled())
            || !Test->TestTrue(TEXT("Destination button has a click handler"), Button->OnClicked.IsBound()))
        {
            return true;
        }
        Test->AddInfo(FString::Printf(TEXT("Click route: %s -> %s"), Expected.Map, Destination.Map));
        Button->OnClicked.Broadcast();
        ++Step;
        StepStartedAt = FPlatformTime::Seconds();
        return false;
    }

private:
    bool WaitOrFail(const FString& Message) const
    {
        if (FPlatformTime::Seconds() - StepStartedAt > 30.0)
        {
            Test->AddError(Message);
            return true;
        }
        return false;
    }

    FAutomationTestBase* Test;
    int32 Step = 0;
    bool bIsolatedSave=false;
    FString SaveSlot;FGuid Top,Bottom;FName CutId,SeamPathId;double ExpectedCoverage=0;
    double StepStartedAt;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftAuthoredAssetsTest,
    "LaceLoft.Content.AuthoredAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaceLoftAuthoredAssetsTest::RunTest(const FString& Parameters)
{
    UClass* NativeParent = LoadClass<UUserWidget>(nullptr, TEXT("/Script/LaceLoft.LaceLoftNavigationWidget"));
    if (!TestNotNull(TEXT("Native navigation widget class loads"), NativeParent))
    {
        return false;
    }

    for (const LaceLoftTests::FScreen& Screen : LaceLoftTests::Screens)
    {
        const FString AssetPath = FString::Printf(TEXT("/Game/UI/%s.%s"), Screen.Widget, Screen.Widget);
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
        if (!TestNotNull(AssetPath, Blueprint))
        {
            continue;
        }
        TestTrue(TEXT("Widget Blueprint retains the navigation parent contract"), Blueprint->ParentClass->IsChildOf(NativeParent));
        if (TestNotNull(TEXT("Widget Blueprint has a saved designer tree"), Blueprint->WidgetTree.Get()))
        {
            TestNotNull(TEXT("Widget Blueprint has a saved designer root"), Blueprint->WidgetTree->RootWidget.Get());
            for (const LaceLoftTests::FScreen& Target : LaceLoftTests::Screens)
            {
                UButton* Button = Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(Target.Button)));
                TestNotNull(FString::Printf(TEXT("%s contains UButton %s"), Screen.Widget, Target.Button), Button);
                if (Button)
                {
                    TestTrue(TEXT("Navigation button is exposed as a designer variable"), Button->bIsVariable != 0);
                    TestEqual(TEXT("Designer disables only the current screen button"),
                        Button->GetIsEnabled(), FCString::Strcmp(Screen.Map, Target.Map) != 0);
                }
            }
        }

        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        TestEqual(FString::Printf(TEXT("%s compiles without errors"), Screen.Widget), Results.NumErrors, 0);
        TestNotNull(TEXT("Widget Blueprint has a generated class"), Blueprint->GeneratedClass.Get());

        const FString MapPath = FString::Printf(TEXT("/Game/Maps/%s.%s"), Screen.Map, Screen.Map);
        UWorld* Map = LoadObject<UWorld>(nullptr, *MapPath);
        if (TestNotNull(MapPath, Map))
        {
            TestNotNull(TEXT("Map contains a persistent level"), Map->PersistentLevel.Get());
            TestNull(TEXT("Map does not contain World Partition"), Map->GetWorldPartition());
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftNavigationPIETest,
    "LaceLoft.Navigation.AllRoutesPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaceLoftNavigationPIETest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Editor is available"), GEditor))
    {
        return false;
    }
    FRequestPlaySessionParams Request;
    Request.GlobalMapOverride = TEXT("/Game/Maps/L_Title");
    Request.StartLocation = FVector::ZeroVector;
    Request.StartRotation = FRotator::ZeroRotator;
    Request.bAllowOnlineSubsystem = false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIEForAutomationCommand(Request));
    ADD_LATENT_AUTOMATION_COMMAND(LaceLoftTests::FNavigateScreens(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
