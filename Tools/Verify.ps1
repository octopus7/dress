param([string]$EngineRoot = 'E:\WorkTemp\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $ProjectRoot 'GarmentLab.uproject'
$Report = Join-Path $ProjectRoot ('Saved\Verification\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $Report | Out-Null
& (Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe') `
  (Join-Path $EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll') `
  GarmentLabEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE "-Log=$Report\Build.log"
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed' }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $Project `
  -unattended -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests GarmentLab' `
  '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Report" "-abslog=$Report\Editor.log"
if ($LASTEXITCODE -ne 0) { throw 'Automation process failed' }
$Result = Get-Content (Join-Path $Report 'index.json') -Raw | ConvertFrom-Json
$Result | Select-Object succeeded,succeededWithWarnings,failed,notRun,inProcess
if ($Result.failed -gt 0 -or $Result.notRun -gt 0 -or $Result.inProcess -gt 0 -or $Result.succeeded -lt 1) {
    throw 'Automation did not pass'
}
Write-Output "Verification report: $Report"
