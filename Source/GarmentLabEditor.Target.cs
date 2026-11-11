using UnrealBuildTool;

public class GarmentLabEditorTarget : TargetRules
{
    public GarmentLabEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("GarmentLab");
    }
}
