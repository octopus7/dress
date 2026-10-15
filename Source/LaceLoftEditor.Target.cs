using UnrealBuildTool;

public class LaceLoftEditorTarget : TargetRules
{
    public LaceLoftEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new[] { "LaceLoft", "LaceLoftEditor" });
    }
}
