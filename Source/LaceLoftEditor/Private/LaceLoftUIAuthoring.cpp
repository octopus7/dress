#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "UI/LaceLoftCreationWidget.h"
#include "UI/LaceLoftPatternWorkspaceWidget.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Styling/CoreStyle.h"
#include "UI/LaceLoftFittingWidget.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "UI/LaceLoftTitleWidget.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "MaterialEditingLibrary.h"

// Editor-only authoring: all resulting hierarchy/style is serialized in real WBPs.
// Existing screens are extended in place; this tool never replaces their trees.
namespace
{
UWidgetBlueprint* NewWidget(const TCHAR* Name,UClass* Parent)
{
    const FString Path=TEXT("/Game/UI/")+FString(Name);
    if(auto* Existing=LoadObject<UWidgetBlueprint>(nullptr,*(Path+TEXT(".")+Name),nullptr,LOAD_NoWarn))return Existing;
    auto* BP=CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UUserWidget::StaticClass(),CreatePackage(*Path),Name,BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
    BP->ParentClass=Parent;
    FAssetRegistryModule::AssetCreated(BP);return BP;
}
template<typename T>T* Widget(UWidgetBlueprint* BP,const TCHAR* Name)
{auto* Result=BP->WidgetTree->ConstructWidget<T>(T::StaticClass(),Name);Result->bIsVariable=true;return Result;}
UTextBlock* Text(UWidgetBlueprint* BP,const TCHAR* Name,const TCHAR* Caption,int32 Size=16)
{
    auto* Result=Widget<UTextBlock>(BP,Name);Result->SetText(FText::FromString(Caption));
    auto Font=Result->GetFont();Font.Size=Size;Result->SetFont(Font);Result->SetColorAndOpacity(FSlateColor(FLinearColor(.13f,.23f,.22f)));
    Result->SetAutoWrapText(true);return Result;
}
UButton* Button(UWidgetBlueprint* BP,const TCHAR* Name,const TCHAR* Caption)
{
    auto* Result=Widget<UButton>(BP,Name);Result->AddChild(Text(BP,*(FString(Name)+TEXT("Caption")),Caption));
    Result->SetBackgroundColor(FLinearColor(.83f,.89f,.84f));return Result;
}
void Row(UVerticalBox* Box,UWidget* Child,float Padding=5)
{auto* Slot=Box->AddChildToVerticalBox(Child);Slot->SetPadding(FMargin(0,Padding));}
void AuthoredFont(UTextBlock* Label,int32 Size)
{
    // The UFont object serializes its composite face data. FCoreStyle's transient
    // file-backed font cannot be persisted as a Designer font reference.
    auto Font=GetDefault<UTextBlock>()->GetFont();Font.Size=Size;Font.TypefaceFontName=TEXT("Regular");Label->SetFont(Font);
}
bool SaveWidget(UWidgetBlueprint* BP)
{
    BP->WidgetTree->ForEachWidget([](UWidget* W)
    {if(auto* Label=Cast<UTextBlock>(W))if(Cast<UButton>(Label->GetParent()) && Label->GetFName()!=TEXT("Label"))Label->SetAutoWrapText(false);});
    if(auto* Label=Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("ButtonLabel2"))))Label->SetText(FText::FromString(TEXT("입어보기")));
    for(const TCHAR* Name:{TEXT("ButtonLabel0"),TEXT("ButtonLabel1"),TEXT("ButtonLabel2")})
        if(auto* Label=Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name)))Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    BP->WidgetTree->ForEachWidget([BP](UWidget* W)
    {if(!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName()))BP->WidgetVariableNameToGuidMap.Add(W->GetFName(),FGuid::NewGuid());});
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);
    if(Results.NumErrors || !BP->GeneratedClass)return false;
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
bool Entry(const TCHAR* Name)
{
    auto* BP=NewWidget(Name,ULaceLoftSelectionEntryWidget::StaticClass());
    if(!BP->WidgetTree->RootWidget)
    {
        auto* ButtonWidget=Widget<UButton>(BP,TEXT("SelectButton"));BP->WidgetTree->RootWidget=ButtonWidget;
        ButtonWidget->SetBackgroundColor(FLinearColor(.88f,.91f,.87f));
        ButtonWidget->AddChild(Text(BP,TEXT("Label"),TEXT("옷본 조각 · 진행 상태"),14));
    }
    return SaveWidget(BP);
}
}

int32 AuthorCreationUI()
{
    if(!Entry(TEXT("WBP_PatternPieceEntry")) || !Entry(TEXT("WBP_SewingStepEntry")))return 1;
    auto* Workspace=NewWidget(TEXT("WBP_PatternWorkspace"),ULaceLoftPatternWorkspaceWidget::StaticClass());
    if(!Workspace->WidgetTree->RootWidget)
    {
        auto* Frame=Widget<UBorder>(Workspace,TEXT("WorkspaceFrame"));Frame->SetBrushColor(FLinearColor(.97f,.96f,.91f));
        Workspace->WidgetTree->RootWidget=Frame;
    }
    Workspace->WidgetTree->RootWidget->SetVisibility(ESlateVisibility::Visible);
    if(!SaveWidget(Workspace))return 2;
    auto* Preview=CastChecked<ULaceLoftPatternWorkspaceWidget>(Workspace->GeneratedClass->GetDefaultObject());
    Preview->PreviewPattern=TSoftObjectPtr<ULaceLoftPatternDefinition>(FSoftObjectPath(TEXT("/Game/Garments/Patterns/DA_TShirt.DA_TShirt")));
    // Save CDO defaults without a second compile resetting freshly assigned defaults.
    {
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(Workspace->GetOutermost(),Workspace,*FPackageName::LongPackageNameToFilename(Workspace->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args))return 3;
    }
    auto* Confirm=NewWidget(TEXT("WBP_ConfirmDialog"),UUserWidget::StaticClass());
    if(!Confirm->WidgetTree->RootWidget)
    {
        auto* Frame=Widget<UBorder>(Confirm,TEXT("DialogFrame"));Confirm->WidgetTree->RootWidget=Frame;
        Frame->SetPadding(FMargin(24));Frame->SetBrushColor(FLinearColor(.97f,.96f,.91f));
        auto* Layout=Widget<UVerticalBox>(Confirm,TEXT("DialogLayout"));Frame->AddChild(Layout);
        Row(Layout,Text(Confirm,TEXT("MessageLabel"),TEXT("현재 작업을 저장했습니다. 계속할까요?"),18));
        Row(Layout,Button(Confirm,TEXT("ConfirmButton"),TEXT("계속")));Row(Layout,Button(Confirm,TEXT("CancelButton"),TEXT("돌아가기")));
    }
    if(!SaveWidget(Confirm))return 4;
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/UI/WBP_GarmentCreation.WBP_GarmentCreation"));
    if(!BP || !BP->WidgetTree)return 5;
    if(!BP->WidgetTree->FindWidget(TEXT("WorkflowLayout"))) // Preserve later Designer edits.
    {
    auto* Background=Cast<UBorder>(BP->WidgetTree->RootWidget);
    auto* Navigation=Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("NavigationMenu")));
    if(!Background || !Navigation || Background->GetContent()!=Navigation)return 6;
    BP->ParentClass=ULaceLoftCreationWidget::StaticClass();
    Background->SetPadding(FMargin(18));Background->SetBrushColor(FLinearColor(.95f,.94f,.90f));
    Background->SetHorizontalAlignment(HAlign_Fill);Background->SetVerticalAlignment(VAlign_Fill);
    Navigation->RemoveFromParent();
    auto* Layout=Widget<UHorizontalBox>(BP,TEXT("WorkflowLayout"));Background->AddChild(Layout);
    auto* Sidebar=Widget<USizeBox>(BP,TEXT("WorkflowSidebar"));Sidebar->SetWidthOverride(190);Sidebar->AddChild(Navigation);
    Layout->AddChildToHorizontalBox(Sidebar)->SetPadding(FMargin(0,0,16,0));
    for(int32 I=0;I<3;++I)if(auto* Size=Cast<USizeBox>(BP->WidgetTree->FindWidget(FName(FString::Printf(TEXT("ButtonSize%d"),I)))))
    {Size->SetWidthOverride(180);Size->SetHeightOverride(40);}
    for(const TCHAR* Name:{TEXT("ProjectTitle"),TEXT("ScreenTitle"),TEXT("ButtonLabel0"),TEXT("ButtonLabel1"),TEXT("ButtonLabel2")})
        if(auto* Label=Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name)))
        {auto Font=Label->GetFont();Font.Size=Name==FString(TEXT("ProjectTitle"))?25:16;Label->SetFont(Font);Label->SetAutoWrapText(true);Label->SetColorAndOpacity(FSlateColor(FLinearColor(.13f,.23f,.22f)));}
    Row(Navigation,Text(BP,TEXT("NewProjectHeading"),TEXT("새 작업"),18),12);
    Row(Navigation,Button(BP,TEXT("NewTopButton"),TEXT("새 티셔츠")));
    Row(Navigation,Button(BP,TEXT("NewBottomButton"),TEXT("새 돌핀쇼츠")));
    Row(Navigation,Text(BP,TEXT("ProjectHeading"),TEXT("내 작업"),18),12);
    auto* Projects=Widget<UScrollBox>(BP,TEXT("ProjectList"));Navigation->AddChildToVerticalBox(Projects)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Main=Widget<UVerticalBox>(BP,TEXT("CreationMain"));Layout->AddChildToHorizontalBox(Main)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Toolbar=Widget<UHorizontalBox>(BP,TEXT("CreationToolbar"));Row(Main,Toolbar);
    const TCHAR* ButtonNames[]={TEXT("UndoButton"),TEXT("RedoButton"),TEXT("SaveButton"),TEXT("ResetViewButton")};
    const TCHAR* Captions[]={TEXT("되돌리기"),TEXT("다시 하기"),TEXT("저장"),TEXT("보기 초기화")};
    for(int32 I=0;I<4;++I)Toolbar->AddChildToHorizontalBox(Button(BP,ButtonNames[I],Captions[I]))->SetPadding(FMargin(0,0,8,0));
    Row(Main,Text(BP,TEXT("InstructionLabel"),TEXT("조각을 선택하여 재단을 시작하세요. 휠로 확대하고 오른쪽 버튼으로 이동합니다."),15));
    auto* WorkRow=Widget<UHorizontalBox>(BP,TEXT("WorkRow"));Main->AddChildToVerticalBox(WorkRow)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* StepsWidth=Widget<USizeBox>(BP,TEXT("StepsWidth"));StepsWidth->SetWidthOverride(210);
    auto* Steps=Widget<UVerticalBox>(BP,TEXT("Steps"));StepsWidth->AddChild(Steps);WorkRow->AddChildToHorizontalBox(StepsWidth)->SetPadding(FMargin(0,0,12,0));
    Row(Steps,Text(BP,TEXT("PieceHeading"),TEXT("1 · 재단"),18));
    Steps->AddChildToVerticalBox(Widget<UScrollBox>(BP,TEXT("PieceList")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Row(Steps,Text(BP,TEXT("SeamHeading"),TEXT("2 · 봉제와 마감"),18));
    Steps->AddChildToVerticalBox(Widget<UScrollBox>(BP,TEXT("SeamList")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* View=BP->WidgetTree->ConstructWidget<ULaceLoftPatternWorkspaceWidget>(Workspace->GeneratedClass.Get(),TEXT("Workspace"));View->bIsVariable=true;
    WorkRow->AddChildToHorizontalBox(View)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Row(Main,Text(BP,TEXT("StatusLabel"),TEXT("입력 한 번을 마칠 때마다 자동 저장합니다."),14));
    }
    if(!BP->WidgetTree->FindWidget(TEXT("CompleteCheatButton")))
    {
        auto* Toolbar=CastChecked<UHorizontalBox>(BP->WidgetTree->FindWidget(TEXT("CreationToolbar")));
        auto* Cheat=Button(BP,TEXT("CompleteCheatButton"),TEXT("치트: 즉시 완성"));
        Cheat->SetBackgroundColor(FLinearColor::White);
        auto Style=Cheat->GetStyle();
        Style.Normal.TintColor=FSlateColor(FLinearColor(.55f,.025f,.035f));
        Style.Hovered.TintColor=FSlateColor(FLinearColor(.8f,.045f,.06f));
        Style.Pressed.TintColor=FSlateColor(FLinearColor(.3f,.008f,.015f));
        Style.Disabled.TintColor=FSlateColor(FLinearColor(.2f,.055f,.06f));
        Cheat->SetStyle(Style);Cheat->SetToolTipText(FText::FromString(TEXT("현재 작업의 재단·봉제를 즉시 완료하고 저장합니다. 되돌리기로 취소할 수 있습니다.")));
        CastChecked<UTextBlock>(Cheat->GetChildAt(0))->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Toolbar->AddChildToHorizontalBox(Cheat)->SetPadding(FMargin(24,0,0,0));
    }
    // Reparent existing widgets to the planned right guidance / bottom tools.
    if(!BP->WidgetTree->FindWidget(TEXT("InstructionPanel")))
    {
        auto* Main=CastChecked<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("CreationMain")));
        auto* WorkRow=CastChecked<UHorizontalBox>(BP->WidgetTree->FindWidget(TEXT("WorkRow")));
        auto* Guidance=CastChecked<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("InstructionLabel")));
        auto* Toolbar=BP->WidgetTree->FindWidget(TEXT("CreationToolbar"));
        Guidance->RemoveFromParent();Toolbar->RemoveFromParent();
        auto* Width=Widget<USizeBox>(BP,TEXT("InstructionPanel"));Width->SetWidthOverride(230);
        auto* Guide=Widget<UVerticalBox>(BP,TEXT("InstructionLayout"));Width->AddChild(Guide);
        Row(Guide,Text(BP,TEXT("InstructionHeading"),TEXT("작업 안내"),18));Row(Guide,Guidance);
        WorkRow->AddChildToHorizontalBox(Width)->SetPadding(FMargin(16,0,0,0));
        CastChecked<UVerticalBoxSlot>(Main->InsertChildAt(1,Toolbar))->SetPadding(FMargin(0,8));
    }
    for(const TCHAR* Name:{TEXT("ProjectTitle"),TEXT("ScreenTitle"),TEXT("ButtonLabel0"),TEXT("ButtonLabel1"),TEXT("ButtonLabel2")})
        if(auto* Label=Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name)))
            AuthoredFont(Label,FCString::Strcmp(Name,TEXT("ProjectTitle"))==0?25:16);
    if(!SaveWidget(BP))return 7;
    for(const auto& Pair:TMap<FName,FString>{{TEXT("PieceEntryClass"),TEXT("WBP_PatternPieceEntry")},{TEXT("SeamEntryClass"),TEXT("WBP_SewingStepEntry")}})
    {
        auto* Property=FindFProperty<FClassProperty>(BP->GeneratedClass,Pair.Key);
        auto* Class=LoadClass<UUserWidget>(nullptr,*FString::Printf(TEXT("/Game/UI/%s.%s_C"),*Pair.Value,*Pair.Value));
        if(!Property || !Class)return 8;Property->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Class);
    }
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args)?0:9;
}

int32 AuthorFittingUI()
{
    const TCHAR* MaterialPath=TEXT("/Game/Characters/Dummy/Materials/M_WireframeOverlay");
    auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/Characters/Dummy/Materials/M_WireframeOverlay.M_WireframeOverlay"),nullptr,LOAD_NoWarn);
    if(!Material)
    {
        Material=NewObject<UMaterial>(CreatePackage(MaterialPath),TEXT("M_WireframeOverlay"),RF_Public|RF_Standalone);
        Material->BlendMode=BLEND_Translucent;Material->SetShadingModel(MSM_Unlit);Material->Wireframe=true;
        Material->TwoSided=true;Material->bDisableDepthTest=false;
        auto* Color=CastChecked<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionVectorParameter::StaticClass()));
        Color->ParameterName=TEXT("WireColor");Color->DefaultValue=FLinearColor(.005f,.08f,.12f);
        auto* Opacity=CastChecked<UMaterialExpressionScalarParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionScalarParameter::StaticClass()));
        Opacity->ParameterName=TEXT("WireOpacity");Opacity->DefaultValue=.55f;
        UMaterialEditingLibrary::ConnectMaterialProperty(Color,TEXT(""),MP_EmissiveColor);
        UMaterialEditingLibrary::ConnectMaterialProperty(Opacity,TEXT(""),MP_Opacity);
        Material->SetMaterialUsage(MATUSAGE_SkeletalMesh);Material->SetMaterialUsage(MATUSAGE_Clothing);
        UMaterialEditingLibrary::RecompileMaterial(Material);FAssetRegistryModule::AssetCreated(Material);
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(Material->GetOutermost(),Material,*FPackageName::LongPackageNameToFilename(MaterialPath,FPackageName::GetAssetPackageExtension()),Args))return 10;
    }
    if(!Material->TwoSided)
    {
        Material->TwoSided=true;
        for(UMaterialExpression* Expression:Material->GetExpressions())
        {
            if(auto* Color=Cast<UMaterialExpressionVectorParameter>(Expression))Color->DefaultValue=FLinearColor(.005f,.08f,.12f);
            if(auto* Opacity=Cast<UMaterialExpressionScalarParameter>(Expression))Opacity->DefaultValue=.55f;
        }
        UMaterialEditingLibrary::RecompileMaterial(Material);
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(Material->GetOutermost(),Material,*FPackageName::LongPackageNameToFilename(MaterialPath,FPackageName::GetAssetPackageExtension()),Args))return 10;
    }
    if(!Entry(TEXT("WBP_GarmentEntry")))return 1;
    auto* Pose=NewWidget(TEXT("WBP_PoseControls"),ULaceLoftPoseControlsWidget::StaticClass());
    if(!Pose->WidgetTree->RootWidget)
    {
        auto* Frame=Widget<UBorder>(Pose,TEXT("PoseFrame"));Frame->SetBrushColor(FLinearColor(.95f,.94f,.90f));Frame->SetPadding(FMargin(18));Pose->WidgetTree->RootWidget=Frame;
        auto* Panel=Widget<UVerticalBox>(Pose,TEXT("PoseLayout"));Frame->AddChild(Panel);
        Row(Panel,Text(Pose,TEXT("PoseHeading"),TEXT("포즈"),22));
        const TCHAR* Names[]={TEXT("NeutralButton"),TEXT("RaisedButton"),TEXT("ForwardButton"),TEXT("SittingButton")};
        const TCHAR* Labels[]={TEXT("중립"),TEXT("팔 들기"),TEXT("팔 앞으로"),TEXT("앉기")};
        for(int32 I=0;I<4;++I)Row(Panel,Button(Pose,Names[I],Labels[I]));
        Row(Panel,Text(Pose,TEXT("JointHeading"),TEXT("관절 조절"),20),18);
        Row(Panel,Widget<UComboBoxString>(Pose,TEXT("JointChoice")));
        const TCHAR* SliderNames[]={TEXT("PitchSlider"),TEXT("YawSlider"),TEXT("RollSlider")};
        const TCHAR* AxisLabels[]={TEXT("앞뒤"),TEXT("좌우"),TEXT("비틀기")};
        for(int32 I=0;I<3;++I)
        {
            Row(Panel,Text(Pose,*(FString(SliderNames[I])+TEXT("Label")),AxisLabels[I],14));
            auto* Slider=Widget<USlider>(Pose,SliderNames[I]);Slider->SetMinValue(-90);Slider->SetMaxValue(90);Slider->SetValue(0);Slider->SetStepSize(1);Row(Panel,Slider);
        }
        Row(Panel,Text(Pose,TEXT("JointValueLabel"),TEXT("회전 범위: −90° ~ 90°"),14));
        Row(Panel,Button(Pose,TEXT("ResetJointsButton"),TEXT("관절 초기화")),16);
    }
    if(!SaveWidget(Pose))return 2;
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/UI/WBP_FittingRoom.WBP_FittingRoom"));if(!BP || !BP->WidgetTree)return 3;
    if(!BP->WidgetTree->FindWidget(TEXT("FittingLayout")))
    {
        auto* Background=Cast<UBorder>(BP->WidgetTree->RootWidget);auto* Navigation=Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("NavigationMenu")));
        if(!Background || !Navigation || Background->GetContent()!=Navigation)return 4;
        BP->ParentClass=ULaceLoftFittingWidget::StaticClass();Background->SetBrushColor(FLinearColor(0,0,0,0));Background->SetPadding(FMargin(16));
        Background->SetHorizontalAlignment(HAlign_Fill);Background->SetVerticalAlignment(VAlign_Fill);Navigation->RemoveFromParent();
        auto* Layout=Widget<UHorizontalBox>(BP,TEXT("FittingLayout"));Background->AddChild(Layout);
        auto* Sidebar=Widget<UBorder>(BP,TEXT("FittingSidebar"));Sidebar->SetBrushColor(FLinearColor(.95f,.94f,.90f));Sidebar->SetPadding(FMargin(18));Sidebar->AddChild(Navigation);
        auto* Width=Widget<USizeBox>(BP,TEXT("FittingSidebarWidth"));Width->SetWidthOverride(230);Width->AddChild(Sidebar);Layout->AddChildToHorizontalBox(Width);
        for(int32 I=0;I<3;++I)if(auto* Size=Cast<USizeBox>(BP->WidgetTree->FindWidget(FName(FString::Printf(TEXT("ButtonSize%d"),I)))))
        {Size->SetWidthOverride(190);Size->SetHeightOverride(40);}
        Row(Navigation,Text(BP,TEXT("GarmentHeading"),TEXT("완성 의상"),20),18);
        Navigation->AddChildToVerticalBox(Widget<UScrollBox>(BP,TEXT("GarmentList")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Row(Navigation,Button(BP,TEXT("RemoveTopButton"),TEXT("상의 벗기")));Row(Navigation,Button(BP,TEXT("RemoveBottomButton"),TEXT("하의 벗기")));
        auto* Center=Widget<UVerticalBox>(BP,TEXT("FittingCenter"));Layout->AddChildToHorizontalBox(Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        auto* CameraArea=Widget<UBorder>(BP,TEXT("CameraInput"));CameraArea->SetBrushColor(FLinearColor(0,0,0,0));Center->AddChildToVerticalBox(CameraArea)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        auto* Status=Text(BP,TEXT("FittingStatus"),TEXT("가운데에서 드래그: 회전 · 휠: 확대"),16);Status->SetColorAndOpacity(FSlateColor(FLinearColor::White));Row(Center,Status);
        Row(Center,Button(BP,TEXT("ResetCameraButton"),TEXT("카메라 초기화")));
        auto* PoseWidth=Widget<USizeBox>(BP,TEXT("PoseControlsWidth"));PoseWidth->SetWidthOverride(250);
        PoseWidth->AddChild(BP->WidgetTree->ConstructWidget<ULaceLoftPoseControlsWidget>(Pose->GeneratedClass.Get(),TEXT("PoseControls")));Layout->AddChildToHorizontalBox(PoseWidth);
    }
    for(const TCHAR* Name:{TEXT("ProjectTitle"),TEXT("ScreenTitle"),TEXT("ButtonLabel0"),TEXT("ButtonLabel1"),TEXT("ButtonLabel2")})
        if(auto* Label=Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name)))
        {AuthoredFont(Label,FCString::Strcmp(Name,TEXT("ProjectTitle"))==0?25:16);Label->SetAutoWrapText(true);Label->SetColorAndOpacity(FSlateColor(FLinearColor(.13f,.23f,.22f)));}
    if(!BP->WidgetTree->FindWidget(TEXT("WireframeButton")))
    {
        auto* Navigation=CastChecked<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("NavigationMenu")));
        auto* Toggle=Button(BP,TEXT("WireframeButton"),TEXT("와이어프레임: 꺼짐"));
        Toggle->SetToolTipText(FText::FromString(TEXT("재질과 조명을 유지한 채 인형과 의상 위에 메시 선을 겹쳐 표시합니다.")));
        Navigation->InsertChildAt(Navigation->GetChildIndex(BP->WidgetTree->FindWidget(TEXT("RemoveTopButton"))),Toggle);
    }
    if(!SaveWidget(BP))return 5;
    auto* Property=FindFProperty<FClassProperty>(BP->GeneratedClass,TEXT("GarmentEntryClass"));
    auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/UI/WBP_GarmentEntry.WBP_GarmentEntry_C"));if(!Property || !Class)return 6;
    Property->SetObjectPropertyValue_InContainer(BP->GeneratedClass->GetDefaultObject(),Class);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    return UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args)?0:7;
}

int32 AuthorTitleUI()
{
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/UI/WBP_Title.WBP_Title"));if(!BP || !BP->WidgetTree)return 1;
    auto* Menu=Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("NavigationMenu")));if(!Menu)return 2;
    if(!BP->WidgetTree->FindWidget(TEXT("ContinueButton")))
    {
        BP->ParentClass=ULaceLoftTitleWidget::StaticClass();
        auto* Choice=Widget<UComboBoxString>(BP,TEXT("PatternChoice"));Choice->AddOption(TEXT("반팔 티셔츠"));Choice->AddOption(TEXT("돌핀쇼츠"));Choice->SetSelectedIndex(0);
        Menu->InsertChildAt(2,Choice);Menu->InsertChildAt(5,Button(BP,TEXT("ContinueButton"),TEXT("이어 하기")));
        Row(Menu,Text(BP,TEXT("TitleStatus"),TEXT("옷본을 고르고 재단과 봉제를 시작하세요."),14));
        if(auto* Label=Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("ButtonLabel1"))))Label->SetText(FText::FromString(TEXT("새로 제작")));
    }
    for(const TCHAR* Name:{TEXT("ProjectTitle"),TEXT("ScreenTitle"),TEXT("ButtonLabel0"),TEXT("ButtonLabel1"),TEXT("ButtonLabel2")})
        if(auto* Label=Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name)))
            AuthoredFont(Label,FCString::Strcmp(Name,TEXT("ProjectTitle"))==0?38:20);
    auto* Choice=CastChecked<UComboBoxString>(BP->WidgetTree->FindWidget(TEXT("PatternChoice")));
    // AddOption only populates transient Slate data. Persist the Designer defaults.
    auto* Options=FindFProperty<FArrayProperty>(UComboBoxString::StaticClass(),TEXT("DefaultOptions"));
    if(!Options)return 4;
    *Options->ContainerPtrToValuePtr<TArray<FString>>(Choice)={TEXT("반팔 티셔츠"),TEXT("돌핀쇼츠")};
    Choice->ClearOptions();Choice->AddOption(TEXT("반팔 티셔츠"));Choice->AddOption(TEXT("돌핀쇼츠"));Choice->SetSelectedIndex(0);
    return SaveWidget(BP)?0:3;
}
