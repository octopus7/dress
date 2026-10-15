#include "AssetToolsModule.h"
#include "AssetImportTask.h"
#include "Factories/TextureFactory.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionConstant.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

int32 AuthorDummySurface()
{
    const FString Folder=TEXT("/Game/Characters/Dummy/Materials/");
    auto Save=[](UObject* Object)
    {
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        const FString File=FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
        return UPackage::SavePackage(Object->GetOutermost(),Object,*File,Args);
    };
    TMap<FString,UTexture2D*> Textures;
    for(const FString Suffix:{TEXT("White"),TEXT("Checker_2048"),TEXT("Checker_1024"),TEXT("Checker_512"),
                              TEXT("Asymmetric_2048"),TEXT("Asymmetric_1024"),TEXT("Asymmetric_512")})
    {
        auto* Task=NewObject<UAssetImportTask>();
        Task->Filename=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("ArtSource/Dummy/")+
            (Suffix==TEXT("White")?TEXT("BaseColor_White"):Suffix)+TEXT(".png"));
        Task->DestinationPath=Folder.LeftChop(1);Task->DestinationName=TEXT("T_Dummy")+Suffix;
        Task->bAutomated=true;Task->bReplaceExisting=true;Task->bSave=false;
        Task->Factory=NewObject<UTextureFactory>();
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({Task});
        UTexture2D* Texture=nullptr;
        for(UObject* Object:Task->GetObjects())if(auto* Imported=Cast<UTexture2D>(Object))Texture=Imported;
        if(!Texture)return 1;
        Texture->SRGB=true;Texture->CompressionSettings=TC_Default;
        Texture->AddressX=TA_Clamp;Texture->AddressY=TA_Clamp;
        Texture->LODGroup=TEXTUREGROUP_Character;Texture->PostEditChange();
        FAssetCompilingManager::Get().FinishAllCompilation();
        if(!Save(Texture))return 2;
        Textures.Add(Suffix,Texture);
    }
    UPackage* Package=CreatePackage(*(Folder+TEXT("M_Dummy")));Package->FullyLoad();
    auto* Master=FindObject<UMaterial>(Package,TEXT("M_Dummy"));
    if(!Master)
    {
        Master=NewObject<UMaterial>(Package,TEXT("M_Dummy"),RF_Public|RF_Standalone);
        auto* Texture=CastChecked<UMaterialExpressionTextureSampleParameter2D>(
            UMaterialEditingLibrary::CreateMaterialExpression(Master,UMaterialExpressionTextureSampleParameter2D::StaticClass()));
        Texture->ParameterName=TEXT("BaseColorTexture");Texture->Texture=Textures[TEXT("White")];
        UMaterialEditingLibrary::ConnectMaterialProperty(Texture,TEXT("RGB"),MP_BaseColor);
        auto* Roughness=CastChecked<UMaterialExpressionConstant>(UMaterialEditingLibrary::CreateMaterialExpression(Master,UMaterialExpressionConstant::StaticClass()));
        Roughness->R=.65f;UMaterialEditingLibrary::ConnectMaterialProperty(Roughness,TEXT(""),MP_Roughness);
    }
    Master->SetMaterialUsage(MATUSAGE_SkeletalMesh);
    UMaterialEditingLibrary::RecompileMaterial(Master);
    FAssetRegistryModule::AssetCreated(Master);
    if(!Save(Master))return 3;
    UMaterialInstanceConstant* White=nullptr;
    for(const auto& Entry:Textures)
    {
        const FString Name=TEXT("MI_Dummy")+Entry.Key;
        UPackage* InstancePackage=CreatePackage(*(Folder+Name));InstancePackage->FullyLoad();
        auto* Instance=FindObject<UMaterialInstanceConstant>(InstancePackage,*Name);
        if(!Instance)Instance=NewObject<UMaterialInstanceConstant>(InstancePackage,*Name,RF_Public|RF_Standalone);
        Instance->SetParentEditorOnly(Master);
        Instance->SetTextureParameterValueEditorOnly(TEXT("BaseColorTexture"),Entry.Value);
        Instance->PostEditChange();FAssetRegistryModule::AssetCreated(Instance);
        if(!Save(Instance))return 4;
        if(Entry.Key==TEXT("White"))White=Instance;
    }
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if(!Mesh || !White)return 5;
    for(auto& Slot:Mesh->GetMaterials())Slot.MaterialInterface=White;
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(!Save(Mesh))return 6;
    UE_LOG(LogTemp,Display,TEXT("Saved paintable master, white and six QA instances, seven source textures."));
    return 0;
}
