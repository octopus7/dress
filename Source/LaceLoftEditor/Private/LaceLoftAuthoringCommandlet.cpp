#include "LaceLoftAuthoringCommandlet.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "HAL/IConsoleManager.h"

int32 AuthorDummyQA();
int32 AuthorClothProbe();
int32 AuthorClothProbeScene();
int32 AuthorDummyPhysics();
int32 AuthorDummySurface();
int32 AuthorGarments();
int32 AuthorCreationUI();
int32 AuthorFittingAssets();
int32 AuthorFittingUI();
int32 AuthorTitleUI();

namespace
{
bool SaveAsset(UObject* Asset)
{
    if (!Asset) return false;
    UPackage* Package = Asset->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}
}

ULaceLoftAuthoringCommandlet::ULaceLoftAuthoringCommandlet()
{
    IsClient = true; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 ULaceLoftAuthoringCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens, Switches;
    ParseCommandLine(*Params, Tokens, Switches);
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("TitleUI")) return AuthorTitleUI();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("FittingAssets")) return AuthorFittingAssets();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("FittingUI")) return AuthorFittingUI();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("CreationUI")) return AuthorCreationUI();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("Garments")) return AuthorGarments();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("DummySurface")) return AuthorDummySurface();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("DummyPhysics")) return AuthorDummyPhysics();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("ClothProbeScene")) return AuthorClothProbeScene();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("ClothProbe")) return AuthorClothProbe();
    if (Tokens.Num() == 1 && Tokens[0] == TEXT("DummyQA")) return AuthorDummyQA();
    const bool bReimport = Tokens.Num() == 1 && Tokens[0] == TEXT("DummyReimport");
    if (Tokens.Num() != 1 || (Tokens[0] != TEXT("Dummy") && !bReimport))
    {
        UE_LOG(LogTemp, Error, TEXT("Specify Dummy authoring stage."));
        return 1;
    }
    // Refuse implicit replacement. Reimport of later source versions must be explicit.
    if (!bReimport && FPackageName::DoesPackageExist(TEXT("/Game/Characters/Dummy/SK_Dummy")))
    {
        UE_LOG(LogTemp, Error, TEXT("SK_Dummy already exists; use the explicit reimport workflow."));
        return 2;
    }
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    Task->Filename = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Dummy/LaceLoftDummy.fbx"));
    Task->DestinationPath = TEXT("/Game/Characters/Dummy");
    Task->DestinationName = TEXT("SK_Dummy");
    Task->bAutomated = true;
    Task->bReplaceExisting = bReimport;
    Task->bReplaceExistingSettings = bReimport;
    Task->bSave = false;
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    Factory->SetDetectImportTypeOnImport(false);
    Task->Factory = Factory;
    UFbxImportUI* Options = NewObject<UFbxImportUI>();
    Options->bAutomatedImportShouldDetectType = false;
    Options->MeshTypeToImport = FBXIT_SkeletalMesh;
    Options->bImportAsSkeletal = true;
    Options->bImportMesh = true;
    Options->bImportAnimations = false;
    Options->bCreatePhysicsAsset = false;
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->SkeletalMeshImportData->bConvertScene = true;
    Options->SkeletalMeshImportData->bConvertSceneUnit = true;
    Options->SkeletalMeshImportData->bForceFrontXAxis = true;
    if (bReimport)
    {
        Options->Skeleton = LoadObject<USkeleton>(nullptr,TEXT("/Game/Characters/Dummy/SKEL_Dummy.SKEL_Dummy"));
        Options->SkeletalMeshImportData->bUpdateSkeletonReferencePose = true;
        USkeletalMesh* Existing = LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
        if (!Existing) return 6;
        // UE's unattended reimport replaces task options with this saved import data.
        UFbxSkeletalMeshImportData* SavedOptions = Cast<UFbxSkeletalMeshImportData>(Existing->GetAssetImportData());
        if (!SavedOptions)
        {
            SavedOptions = NewObject<UFbxSkeletalMeshImportData>(Existing);
            SavedOptions->Update(Task->Filename);
            Existing->SetAssetImportData(SavedOptions);
        }
        SavedOptions->bConvertScene = true;
        SavedOptions->bConvertSceneUnit = true;
        SavedOptions->bForceFrontXAxis = true;
        SavedOptions->bUpdateSkeletonReferencePose = true;
    }
    Options->SkeletalMeshImportData->NormalImportMethod = FBXNIM_ImportNormals;
    Task->Options = Options;
    // Reimport otherwise selects Interchange and ignores the explicit legacy FBX options.
    if (IConsoleVariable* InterchangeFBX = IConsoleManager::Get().FindConsoleVariable(TEXT("Interchange.FeatureFlags.Import.FBX")))
        InterchangeFBX->Set(false, ECVF_SetByCode);
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({Task});
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if (!Mesh || !Mesh->GetSkeleton()) return 3;
    USkeleton* Skeleton = Mesh->GetSkeleton();
    FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    if (Skeleton->GetName() != TEXT("SKEL_Dummy"))
        AssetTools.Get().RenameAssets({FAssetRenameData(Skeleton, TEXT("/Game/Characters/Dummy"), TEXT("SKEL_Dummy"))});
    UPackage* Package = CreatePackage(TEXT("/Game/Characters/Dummy/PHYS_Dummy"));
    UPhysicsAsset* Physics = NewObject<UPhysicsAsset>(Package, TEXT("PHYS_Dummy"), RF_Public | RF_Standalone);
    FPhysAssetCreateParams Fit;
    Fit.MinBoneSize = 0.3f;
    Fit.MinWeldSize = 0.01f;
    Fit.bCreateConstraints = false; // Kinematic cloth collision; no ragdoll requested.
    Fit.bIncludeChildBones = false;
    FText Error;
    if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(Physics, Mesh, Fit, Error, true, false))
    {
        UE_LOG(LogTemp, Error, TEXT("Physics creation failed: %s"), *Error.ToString());
        return 4;
    }
    FAssetRegistryModule::AssetCreated(Physics);
    if (!SaveAsset(Physics) || !SaveAsset(Skeleton) || !SaveAsset(Mesh)) return 5;
    UE_LOG(LogTemp, Display, TEXT("Authored dummy: height %.4f cm, %d bones, %d collision bodies"),
        Mesh->GetImportedBounds().BoxExtent.Z * 2, Mesh->GetRefSkeleton().GetNum(), Physics->SkeletalBodySetups.Num());
    return 0;
}
