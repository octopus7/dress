#include "Development/LaceLoftClothProbeActor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "FileHelpers.h"
#include "EngineUtils.h"

int32 AuthorClothProbeScene()
{
    USkeletalMesh* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    UChaosClothAsset* Asset=LoadObject<UChaosClothAsset>(nullptr,TEXT("/Game/Development/ClothProbe/CA_TShirtProbe.CA_TShirtProbe"));
    if(!Mesh||!Asset)return 2;
    UPackage* MatPackage=CreatePackage(TEXT("/Game/Development/ClothProbe/M_ProbeCloth"));
    MatPackage->FullyLoad();
    UMaterial* Material=FindObject<UMaterial>(MatPackage,TEXT("M_ProbeCloth"));
    if(!Material)
    {
        Material=NewObject<UMaterial>(MatPackage,TEXT("M_ProbeCloth"),RF_Public|RF_Standalone);
        auto* Color=CastChecked<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material,UMaterialExpressionVectorParameter::StaticClass()));
        Color->ParameterName=TEXT("Tint");Color->DefaultValue=FLinearColor(.08f,.3f,.65f);
        UMaterialEditingLibrary::ConnectMaterialProperty(Color,TEXT(""),MP_BaseColor);
    }
    Material->TwoSided=true;
    Material->SetMaterialUsage(MATUSAGE_Clothing);
    Material->SetMaterialUsage(MATUSAGE_SkeletalMesh);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    FAssetRegistryModule::AssetCreated(Material);
    FSavePackageArgs SaveArgs;SaveArgs.TopLevelFlags=RF_Public|RF_Standalone;
    const FString MatFilename=FPackageName::LongPackageNameToFilename(MatPackage->GetName(),FPackageName::GetAssetPackageExtension());
    if(!UPackage::SavePackage(MatPackage,Material,*MatFilename,SaveArgs))return 3;
    UPackage* WhitePackage=CreatePackage(TEXT("/Game/Development/ClothProbe/M_ProbeDummy"));
    WhitePackage->FullyLoad();
    UMaterial* White=FindObject<UMaterial>(WhitePackage,TEXT("M_ProbeDummy"));
    if(!White)
    {
        White=NewObject<UMaterial>(WhitePackage,TEXT("M_ProbeDummy"),RF_Public|RF_Standalone);
        auto* Color=CastChecked<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(White,UMaterialExpressionVectorParameter::StaticClass()));
        Color->ParameterName=TEXT("Tint");Color->DefaultValue=FLinearColor(.8f,.8f,.8f);
        UMaterialEditingLibrary::ConnectMaterialProperty(Color,TEXT(""),MP_BaseColor);
    }
    White->SetMaterialUsage(MATUSAGE_SkeletalMesh);
    UMaterialEditingLibrary::RecompileMaterial(White);
    for(auto& Slot:Mesh->GetMaterials())Slot.MaterialInterface=White;
    for(auto& Slot:Asset->GetMaterials())Slot.MaterialInterface=Material;
    for(UObject* Object:TArray<UObject*>{White,Mesh,Asset})
    {
        const FString Filename=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        if(!UPackage::SavePackage(Object->GetOutermost(),Object,*Filename,SaveArgs))return 3;
    }
    const FString MapFile=FPaths::ProjectContentDir()/TEXT("Development/ClothProbe/L_ClothProbe.umap");
    const bool Existing=FPackageName::DoesPackageExist(TEXT("/Game/Development/ClothProbe/L_ClothProbe"));
    UWorld* World=nullptr;
    if(Existing)
    {
        if(!FEditorFileUtils::LoadMap(MapFile,false,true))return 1;
        World=GEditor->GetEditorWorldContext().World();
    }
    else World=GEditor->NewMap();
    World->GetWorldSettings()->DefaultGameMode=ALaceLoftClothProbeGameMode::StaticClass();
    ALaceLoftClothProbeActor* Actor=nullptr;
    for(TActorIterator<ALaceLoftClothProbeActor> It(World);It;++It){Actor=*It;break;}
    if(!Actor)Actor=World->SpawnActor<ALaceLoftClothProbeActor>();
    Actor->SetActorLabel(TEXT("15cm Dummy and Cloth Probe"));
    Actor->Body->SetSkeletalMesh(Mesh);
    Actor->Body->SetMaterial(0,White);
    Actor->Cloth->SetAsset(Asset);Actor->Cloth->SetMaterial(0,Material);
    Actor->NeutralPose=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Dummy/Animation/A_DummyNeutral.A_DummyNeutral"));
    Actor->RaisedPose=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Dummy/Animation/A_DummyArmsRaised.A_DummyArmsRaised"));
    if(!Actor->NeutralPose||!Actor->RaisedPose)return 4;
    if(!Existing)for(const FRotator Angle:{FRotator(-30,180,0),FRotator(-20,0,0),FRotator(-15,90,0),FRotator(-15,-90,0)})
    {
        auto* Light=World->SpawnActor<ADirectionalLight>();
        Light->SetActorRotation(Angle);Light->GetLightComponent()->SetIntensity(Angle.Yaw==180?4.f:1.5f);
        Light->SetMobility(EComponentMobility::Movable);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(!FEditorFileUtils::SaveMap(World,MapFile))return 5;
    UE_LOG(LogTemp,Display,TEXT("Saved actual-size cloth probe map with native development harness."));
    return 0;
}
