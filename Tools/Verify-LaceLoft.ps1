param(
    [string]$EngineRoot = 'E:\WorkTemp\Epic Games\UE_5.8',
    [string]$RunName = ('Run-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $ProjectRoot 'LaceLoft.uproject'
$Report = Join-Path $ProjectRoot ('Saved\Automation\Implementation\' + $RunName)
New-Item -ItemType Directory -Force -Path $Report | Out-Null
if (-not $SkipBuild) {
    & (Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe') `
      (Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll') `
      '-Target=LaceLoftEditor Win64 Development' '-Target=LaceLoft Win64 Development' `
      "-Project=$Project" -WaitMutex -NoHotReloadFromIDE *> (Join-Path $Report 'Build.log')
    if ($LASTEXITCODE -ne 0) { Get-Content (Join-Path $Report 'Build.log') -Tail 40; throw 'UE build failed' }
}
# A new report is mandatory; a previous passing report must never mask a failed launch.
$Index = Join-Path $Report 'index.json'
if (Test-Path -LiteralPath $Index) { throw "Report already exists. Use a new RunName: $Index" }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $Project `
  -unattended -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests LaceLoft' `
  '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Report" "-abslog=$Report\Editor.log" `
  *> (Join-Path $Report 'Console.log')
$EngineExit = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $Index)) { throw "No automation report; engine exit $EngineExit" }
$Result = Get-Content -LiteralPath $Index -Raw | ConvertFrom-Json
$Result | Select-Object succeeded,succeededWithWarnings,failed,notRun,inProcess | Format-List
$Result.tests | Select-Object fullTestPath,state,warnings,errors | Format-Table
if ($EngineExit -ne 0 -or $Result.failed -gt 0 -or $Result.notRun -gt 0 -or $Result.inProcess -gt 0 `
    -or ($Result.succeeded + $Result.succeededWithWarnings) -eq 0) {
    throw "Automation failed. Inspect $Index (engine exit $EngineExit)"
}
Write-Output "Verification report: $Index"
