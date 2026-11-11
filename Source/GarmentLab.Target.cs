using UnrealBuildTool;

public class GarmentLabTarget : TargetRules
{
    public GarmentLabTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("GarmentLab");
    }
}
