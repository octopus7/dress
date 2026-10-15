param(
    [string]$EngineRoot='E:/WorkTemp/Epic Games/UE_5.8',
    [string]$RunName=('Body-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [switch]$Render
)
$ErrorActionPreference='Stop'
$RepoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$HostRoot=Join-Path $RepoRoot 'PluginDev/LaceGarmentHost'
if($RunName -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid run name'}
$Report=Join-Path $HostRoot "Saved/Verification/$RunName"
if(Test-Path -LiteralPath $Report){throw 'Refusing stale report'}
New-Item -ItemType Directory -Path $Report | Out-Null
$Arguments=@((Join-Path $HostRoot 'LaceGarmentHost.uproject'),'-run=LaceGarmentBody','-unattended','-nosplash','-nosound',"-ReportDir=$Report","-abslog=$Report/Editor.log")
if($Render){$Arguments+=@('-Render','-AllowCommandletRendering','-RenderOffscreen')}else{$Arguments+='-nullrhi'}
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @Arguments *> (Join-Path $Report 'Console.log')
$EngineExit=$LASTEXITCODE
if($EngineExit -ne 0 -or -not(Test-Path -LiteralPath (Join-Path $Report 'body.json'))){
    Get-Content (Join-Path $Report 'Editor.log') -Tail 30
    throw "Body import failed (exit $EngineExit)"
}
$ReportData=Get-Content (Join-Path $Report 'body.json') -Raw|ConvertFrom-Json
$ReportData|Select-Object heightCm,bones,physicsBodies|Format-List
Write-Output "Import completed; acceptance tests must be run separately. Report: $Report/body.json"
