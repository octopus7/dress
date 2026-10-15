param(
    [string]$EngineRoot = 'E:/WorkTemp/Epic Games/UE_5.8',
    [string]$RunName = ('Probe-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [switch]$VerifyExisting,
    [switch]$Render,
    [switch]$BodySkin
)
$ErrorActionPreference='Stop'
$RepoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$HostRoot=Join-Path $RepoRoot 'PluginDev/LaceGarmentHost'
if($RunName -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid run name'}
$Report=Join-Path $HostRoot "Saved/Verification/$RunName"
if(Test-Path -LiteralPath $Report){throw 'Refusing stale report'}
New-Item -ItemType Directory -Path $Report | Out-Null
$Arguments=@((Join-Path $HostRoot 'LaceGarmentHost.uproject'),'-run=LaceGarmentProbe',
    '-unattended','-nosplash','-nosound',"-ReportDir=$Report","-abslog=$Report/Editor.log")
if($Render){$Arguments+=@('-Render','-AllowCommandletRendering','-RenderOffscreen')}
else{$Arguments+='-nullrhi'}
if($VerifyExisting){$Arguments+='-VerifyExisting'}
if($BodySkin){$Arguments+='-BodySkin'}
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @Arguments *> (Join-Path $Report 'Console.log')
$EngineExit=$LASTEXITCODE
if(-not(Test-Path (Join-Path $Report 'metrics.json'))){Get-Content (Join-Path $Report 'Editor.log') -Tail 35; throw "No probe metrics (exit $EngineExit)"}
$Metrics=Get-Content (Join-Path $Report 'metrics.json') -Raw|ConvertFrom-Json
$Metrics|Select-Object passed,reopenedInNewProcess,steps,maxRelativeEdgeError,runtimeParticles,runtimeMaxDisplacementCm,runtimeBodyPenetrationCm|Format-List
if($EngineExit -ne 0 -or -not $Metrics.passed){throw "Probe failed (exit $EngineExit)"}
Write-Output "Report: $Report/metrics.json"
