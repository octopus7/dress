using UnrealBuildTool;
public class GarmentFitClothProbe : ModuleRules
{
    public GarmentFitClothProbe(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Chaos", "ChaosCloth",
            "ChaosClothAsset", "ChaosClothAssetEngine", "ClothingSystemRuntimeCommon",
            "PhysicsCore", "RenderCore", "RHI", "Json", "JsonUtilities", "AssetRegistry", "AssetTools"
        });
    }
}
