using UnrealBuildTool;
public class LaceGarmentHostEditorTarget : TargetRules
{
    public LaceGarmentHostEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("LaceGarmentHost");
    }
}
