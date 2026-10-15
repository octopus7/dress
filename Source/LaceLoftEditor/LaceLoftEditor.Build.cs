using UnrealBuildTool;

public class LaceLoftEditor : ModuleRules
{
    public LaceLoftEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "LaceLoft", "UMG", "Slate", "SlateCore",
            "UnrealEd", "UMGEditor", "Kismet", "KismetCompiler", "AssetRegistry", "InputCore",
            "AssetTools", "PhysicsUtilities", "RenderCore", "ChaosClothAsset", "ChaosClothAssetEngine",
            "ChaosClothAssetDataflowNodes", "ChaosCloth", "Chaos", "DataflowCore", "DataflowEngine", "ChaosCore", "Json", "MaterialEditor"
        });
    }
}
