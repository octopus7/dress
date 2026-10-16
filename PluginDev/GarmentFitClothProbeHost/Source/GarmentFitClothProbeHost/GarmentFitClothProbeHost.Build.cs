using UnrealBuildTool;
public class GarmentFitClothProbeHost : ModuleRules
{
    public GarmentFitClothProbeHost(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
