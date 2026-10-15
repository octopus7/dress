using UnrealBuildTool;
public class LaceGarmentHostTarget : TargetRules
{
    public LaceGarmentHostTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("LaceGarmentHost");
    }
}
