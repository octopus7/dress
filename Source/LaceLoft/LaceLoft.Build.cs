using UnrealBuildTool;

public class LaceLoft : ModuleRules
{
    public LaceLoft(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "InputCore", "ChaosClothAssetEngine", "ClothingSystemRuntimeInterface", "Json", "RHI" });
    }
}
