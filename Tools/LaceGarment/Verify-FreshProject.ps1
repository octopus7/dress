param(
    [string]$EngineRoot='E:/WorkTemp/Epic Games/UE_5.8',
    [string]$RunName=('Fresh-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [int]$ExpectedTests=37
)
$ErrorActionPreference='Stop'
if($RunName -notmatch '^[a-zA-Z0-9_-]+$' -or $ExpectedTests -lt 1){throw 'Invalid verification arguments'}
$RepoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$HostRoot=Join-Path $RepoRoot 'PluginDev/LaceGarmentHost'
$Report=Join-Path $HostRoot "Saved/Verification/$RunName"
if(Test-Path -LiteralPath $Report){throw 'Refusing stale acceptance output'}
New-Item -ItemType Directory -Path $Report | Out-Null
$Package=Join-Path $Report 'PackagedPlugin'
$Original=Join-Path $HostRoot 'Plugins/LaceGarment/LaceGarment.uplugin'
& (Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat') BuildPlugin `
    "-Plugin=$Original" "-Package=$Package" -TargetPlatforms=Win64 -Unattended -NoP4 `
    *> (Join-Path $Report 'BuildPlugin.log')
if($LASTEXITCODE -ne 0){Get-Content (Join-Path $Report 'BuildPlugin.log') -Tail 50; throw 'BuildPlugin failed'}
$Fresh=Join-Path $Report 'FreshBlueprintProject'
$PluginDestination=Join-Path $Fresh 'Plugins/LaceGarment'
New-Item -ItemType Directory -Path $PluginDestination -Force | Out-Null
Get-ChildItem -LiteralPath $Package -Force | Copy-Item -Destination $PluginDestination -Recurse
$Project=Join-Path $Fresh 'LaceGarmentAcceptance.uproject'
@{
    FileVersion=3; EngineAssociation='5.8'; Category=''; Description='Local independent plugin acceptance fixture'
    Plugins=@(@{Name='LaceGarment';Enabled=$true},@{Name='AndroidFileServer';Enabled=$false})
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Project -Encoding utf8
function Invoke-ProjectAcceptance([string]$Project,[string]$VerificationRoot) {
$TestReport=Join-Path $VerificationRoot 'Automation'
New-Item -ItemType Directory -Path $TestReport | Out-Null
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $Project `
    -unattended -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests LaceGarment' `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$TestReport" "-abslog=$TestReport/Editor.log" `
    *> (Join-Path $TestReport 'Console.log')
$EngineExit=$LASTEXITCODE
$Index=Join-Path $TestReport 'index.json'
if(-not(Test-Path -LiteralPath $Index)){Get-Content (Join-Path $TestReport 'Console.log') -Tail 35; throw "No fresh project test report (exit $EngineExit)"}
$Result=Get-Content -LiteralPath $Index -Raw | ConvertFrom-Json
if($EngineExit -ne 0 -or $Result.failed -gt 0 -or $Result.notRun -gt 0 -or $Result.inProcess -gt 0 `
    -or ($Result.succeeded+$Result.succeededWithWarnings) -lt $ExpectedTests){throw "Fresh project automation failed (exit $EngineExit)"}
$Probe=Join-Path $VerificationRoot 'ReopenProbe'
New-Item -ItemType Directory -Path $Probe | Out-Null
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $Project `
    -run=LaceGarmentProbe -VerifyExisting -BodySkin -unattended -nosplash -nullrhi -nosound `
    "-ReportDir=$Probe" "-abslog=$Probe/Editor.log" *> (Join-Path $Probe 'Console.log')
$ProbeExit=$LASTEXITCODE
$Metrics=Join-Path $Probe 'metrics.json'
if(-not(Test-Path -LiteralPath $Metrics)){throw "No fresh project probe report (exit $ProbeExit)"}
$ProbeResult=Get-Content -LiteralPath $Metrics -Raw | ConvertFrom-Json
if($ProbeExit -ne 0 -or -not $ProbeResult.passed){throw "Fresh project probe failed (exit $ProbeExit)"}
return @{project=$Project;testsPassed=$Result.succeeded+$Result.succeededWithWarnings;reopenedBodySkinProbe=$true;runtimeParticles=$ProbeResult.runtimeParticles}
}
$BlueprintResult=Invoke-ProjectAcceptance $Project (Join-Path $Report 'Blueprint')
$FreshCpp=Join-Path $Report 'FreshCppProject'
$CppPlugin=Join-Path $FreshCpp 'Plugins/LaceGarment'
New-Item -ItemType Directory -Path $CppPlugin -Force | Out-Null
Get-ChildItem -LiteralPath $Package -Force | Copy-Item -Destination $CppPlugin -Recurse
# The dedicated host contains only an empty module and target definitions.
# No Source, Content or Config from the LaceLoft game is copied.
Copy-Item -LiteralPath (Join-Path $HostRoot 'Source') -Destination (Join-Path $FreshCpp 'Source') -Recurse
$CppProject=Join-Path $FreshCpp 'LaceGarmentHost.uproject'
Copy-Item -LiteralPath (Join-Path $HostRoot 'LaceGarmentHost.uproject') -Destination $CppProject
& (Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe') `
    (Join-Path $EngineRoot 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll') `
    'LaceGarmentHostEditor' 'Win64' 'Development' "-Project=$CppProject" -WaitMutex -NoHotReloadFromIDE `
    "-Log=$Report/CppUBT.log" *> (Join-Path $Report 'CppBuild.log')
if($LASTEXITCODE -ne 0){Get-Content (Join-Path $Report 'CppBuild.log') -Tail 40; throw 'Fresh C++ Editor build failed'}
$CppResult=Invoke-ProjectAcceptance $CppProject (Join-Path $Report 'Cpp')
@{
    scope='Development checkpoint only; authoring UI, representative garments, cook and packaged-game gates remain pending'
    engineRoot=$EngineRoot; blueprint=$BlueprintResult; cpp=$CppResult
    precompiledPluginLoaded=$true; cppEditorBuilt=$true
    compilerInstalledOnThisMachine=$true; compilerFreeMachineTested=$false
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Report 'acceptance.json') -Encoding utf8
Write-Output "Fresh Blueprint and C++ projects passed $($BlueprintResult.testsPassed) / $($CppResult.testsPassed) tests and both reopened Cloth runtime probes."
Write-Output "Report: $Report/acceptance.json"
