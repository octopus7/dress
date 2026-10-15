param(
    [string]$EngineRoot = 'E:/WorkTemp/Epic Games/UE_5.8',
    [string]$RunName = ('Run-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [switch]$SkipBuild,
    [switch]$BuildOnly
)
$ErrorActionPreference = 'Stop'
if ($SkipBuild -and $BuildOnly) { throw 'SkipBuild and BuildOnly are mutually exclusive' }
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$HostRoot = Join-Path $RepoRoot 'PluginDev/LaceGarmentHost'
$Project = Join-Path $HostRoot 'LaceGarmentHost.uproject'
if ($RunName -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Invalid run name' }
$Report = Join-Path $HostRoot "Saved/Verification/$RunName"
if (Test-Path -LiteralPath $Report) { throw "Refusing stale report: $Report" }
New-Item -ItemType Directory -Path $Report | Out-Null
if (-not $SkipBuild) {
    & (Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe') `
      (Join-Path $EngineRoot 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll') `
      'LaceGarmentHostEditor' 'Win64' 'Development' "-Project=$Project" -WaitMutex -NoHotReloadFromIDE `
      "-Log=$Report/UBT.log" *> (Join-Path $Report 'Build.log')
    if ($LASTEXITCODE -ne 0) { Get-Content (Join-Path $Report 'Build.log') -Tail 50; throw 'Editor build failed' }
}
if ($BuildOnly) { Write-Output "Editor build succeeded. Tests were not run. Log: $Report/Build.log"; return }
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $Project `
  -unattended -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests LaceGarment' `
  '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Report" "-abslog=$Report/Editor.log" `
  *> (Join-Path $Report 'Console.log')
$EngineExit = $LASTEXITCODE
$Index = Join-Path $Report 'index.json'
if (-not (Test-Path -LiteralPath $Index)) { Get-Content (Join-Path $Report 'Console.log') -Tail 35; throw "No report (exit $EngineExit)" }
$Result = Get-Content -LiteralPath $Index -Raw | ConvertFrom-Json
$Result.tests | Select-Object fullTestPath,state,errors,warnings | Format-Table
Write-Output "Report: $Index"
if ($EngineExit -ne 0 -or $Result.failed -gt 0 -or $Result.notRun -gt 0 -or $Result.inProcess -gt 0 `
    -or ($Result.succeeded + $Result.succeededWithWarnings) -eq 0) { throw "Automation failed (exit $EngineExit)" }
