using UnrealBuildTool;
public class LaceGarmentEditor : ModuleRules
{
    public LaceGarmentEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "GeometryCore", "ChaosClothAsset", "ChaosClothAssetEngine",
            "ChaosCloth", "Chaos", "ChaosCore", "ChaosSolverEngine", "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface",
            "Json", "JsonUtilities", "Projects", "AssetRegistry", "MaterialEditor", "RenderCore", "RHI", "AssetTools", "PhysicsUtilities", "GeometryFramework"
        });
    }
}
