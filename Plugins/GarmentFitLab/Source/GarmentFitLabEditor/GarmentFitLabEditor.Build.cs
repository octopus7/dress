using UnrealBuildTool;

public class GarmentFitLabEditor : ModuleRules
{
    public GarmentFitLabEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GarmentFitLab", "UnrealEd", "UMG", "UMGEditor", "Kismet"
        });
    }
}
