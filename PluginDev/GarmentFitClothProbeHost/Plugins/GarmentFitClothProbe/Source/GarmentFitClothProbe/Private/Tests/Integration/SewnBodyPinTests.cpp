#include "ProbeSupport.h"
#include "Sewing/SewingProbe.h"
#include "BodyPins/BodyPinProbe.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewnBodyPinProbe,
    "GarmentFitClothProbe.Integration.SewnBodyPins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSewnBodyPinProbe::RunTest(const FString&)
{
    using namespace GarmentFitClothProbe;
    FRunContext Context;
    FString Error;
    if (!Support::GetRunContext(TEXT("Integration"), Context, Error))
    {
        AddError(Error);
        return false;
    }
    UChaosClothAsset* SewnAsset = Sewing::CreateIntegrationAsset(Context, Error);
    if (!SewnAsset)
    {
        AddError(Error.IsEmpty() ? TEXT("Combined native sewing/body pin experiment failed without a diagnostic") : Error);
        return false;
    }
    const auto& Collections = static_cast<const UChaosClothAsset*>(SewnAsset)->GetClothCollections();
    if (Collections.Num() != 1) { AddError(TEXT("Integration requires one source LOD")); return false; }
    const UE::Chaos::ClothAsset::FCollectionClothConstFacade Before(Collections[0]);
    TArray<FVector2f> Source2D;
    TArray<FVector3f> Source3D;
    TArray<int32> SourceLookup;
    Source2D.Append(Before.GetSimPosition2D().GetData(), Before.GetSimPosition2D().Num());
    Source3D.Append(Before.GetSimPosition3D().GetData(), Before.GetSimPosition3D().Num());
    SourceLookup.Append(Before.GetSimVertex3DLookup().GetData(), Before.GetSimVertex3DLookup().Num());
    if (!BodyPins::RunCombined(SewnAsset, Context, Error)) { AddError(Error); return false; }
    const UE::Chaos::ClothAsset::FCollectionClothConstFacade After(Collections[0]);
    if (!TestEqual(TEXT("Input 2D count retained"), After.GetSimPosition2D().Num(), Source2D.Num()) ||
        !TestEqual(TEXT("Input 3D count retained"), After.GetSimPosition3D().Num(), Source3D.Num()) ||
        !TestEqual(TEXT("Input lookup count retained"), After.GetSimVertex3DLookup().Num(), SourceLookup.Num())) return false;
    for (int32 I = 0; I < Source2D.Num(); ++I)
        if (!TestTrue(TEXT("Pin integration retains input source 2D"), Source2D[I] == After.GetSimPosition2D()[I]) ||
            !TestEqual(TEXT("Pin integration retains input welded lookup"), SourceLookup[I], After.GetSimVertex3DLookup()[I])) return false;
    for (int32 I = 0; I < Source3D.Num(); ++I)
        if (!TestTrue(TEXT("Pin integration retains input source 3D"), Source3D[I] == After.GetSimPosition3D()[I])) return false;

    // Inspect dependencies recorded in the actual saved packages, including soft
    // references, rather than inferring isolation only from source path strings.
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanPathsSynchronous({TEXT("/GarmentFitClothProbe")}, true);
    TArray<FAssetData> Assets;
    Registry.GetAssetsByPath(FName(TEXT("/GarmentFitClothProbe")), Assets, true, true);
    bool bFoundBody = false;
    TSharedRef<FJsonObject> DependencyReport = MakeShared<FJsonObject>();
    for (const FAssetData& Asset : Assets)
    {
        bFoundBody |= Asset.PackageName == FName(TEXT("/GarmentFitClothProbe/BodyPins/SM_LunaBindPose"));
        TArray<FName> Dependencies;
        Registry.GetDependencies(Asset.PackageName, Dependencies);
        TArray<TSharedPtr<FJsonValue>> Names;
        for (FName Dependency : Dependencies)
        {
            const FString Path = Dependency.ToString();
            if (Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/LaceGarment/")) || Path.StartsWith(TEXT("/GarmentFitLab/")))
            { AddError(Asset.PackageName.ToString() + TEXT(" depends on main product package ") + Path); return false; }
            Names.Add(MakeShared<FJsonValueString>(Path));
        }
        DependencyReport->SetArrayField(Asset.PackageName.ToString(), Names);
    }
    if (!TestTrue(TEXT("Saved independent mannequin appears in package dependency scan"), bFoundBody)) return false;
    FString Json;
    FJsonSerializer::Serialize(DependencyReport, TJsonWriterFactory<>::Create(&Json));
    if (!FFileHelper::SaveStringToFile(Json, *(Context.OutputDirectory / TEXT("package-dependencies.json"))))
    { AddError(TEXT("Cannot save package dependency evidence")); return false; }
    return true;
}
