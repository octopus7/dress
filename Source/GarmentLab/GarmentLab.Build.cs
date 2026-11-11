using UnrealBuildTool;

public class GarmentLab : ModuleRules
{
    public GarmentLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore" });
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "UMGEditor", "EngineSettings" });
        }
    }
}
