param(
    [string]$EngineRoot = 'E:/WorkTemp/Epic Games/UE_5.8',
    [string]$RunName = ('Probe-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [switch]$VerifyExisting,
    [switch]$Render,
    [switch]$BodySkin,
    [switch]$Poses,
    [switch]$RuntimeOnly,
    [switch]$GenerateTethers,
    [switch]$RuntimeWaistband,
    [switch]$RuntimeSkinnedBody,
    [ValidateRange(0,400)][int]$RuntimeIterations=0,
    [ValidateRange(-1,2)][double]$RuntimeGravityScale=-1,
    [ValidateRange(-1,1)][double]$RuntimeFriction=-1,
    [ValidateRange(-1,1)][double]$RuntimeBendingStrength=-1,
    [switch]$HingeBending,
    [ValidateSet('TShirt','DolphinShorts')][string]$Template,
    [string]$SourceDraft,
    [ValidateRange(1,10000)][int]$MaxSteps=4800
)
$ErrorActionPreference='Stop'
if($RuntimeOnly -and (-not $Template -or $SourceDraft -or $VerifyExisting)){throw 'RuntimeOnly requires Template and excludes SourceDraft/VerifyExisting; it does not check source identity'}
if($GenerateTethers -and -not $RuntimeOnly){throw 'GenerateTethers is a transient RuntimeOnly diagnostic'}
if($RuntimeWaistband -and (-not $RuntimeOnly -or $Template -ne 'DolphinShorts')){throw 'RuntimeWaistband requires RuntimeOnly DolphinShorts'}
if($RuntimeSkinnedBody -and (-not $RuntimeOnly -or -not $Poses)){throw 'RuntimeSkinnedBody requires RuntimeOnly and Poses'}
if($RuntimeIterations -and -not $RuntimeOnly){throw 'RuntimeIterations is a transient RuntimeOnly diagnostic'}
if($RuntimeGravityScale -ge 0 -and -not $RuntimeOnly){throw 'RuntimeGravityScale is a transient RuntimeOnly diagnostic'}
if($RuntimeFriction -ge 0 -and -not $RuntimeOnly){throw 'RuntimeFriction is a transient RuntimeOnly diagnostic'}
if(($RuntimeBendingStrength -ge 0 -or $HingeBending) -and -not $RuntimeOnly){throw 'Runtime bending overrides are transient RuntimeOnly diagnostics'}
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
if($Poses){$Arguments+='-Poses'}
if($RuntimeOnly){$Arguments+='-RuntimeOnly'}
if($GenerateTethers){$Arguments+='-GenerateTethers'}
if($RuntimeWaistband){$Arguments+='-RuntimeWaistband'}
if($RuntimeSkinnedBody){$Arguments+=@('-RuntimeSkinnedBody','-LogCmds=LogChaosCloth VeryVerbose')}
if($RuntimeIterations){$Arguments+="-RuntimeIterations=$RuntimeIterations"}
if($RuntimeGravityScale -ge 0){$Arguments+="-RuntimeGravityScale=$($RuntimeGravityScale.ToString([System.Globalization.CultureInfo]::InvariantCulture))"}
if($RuntimeFriction -ge 0){$Arguments+="-RuntimeFriction=$($RuntimeFriction.ToString([System.Globalization.CultureInfo]::InvariantCulture))"}
if($RuntimeBendingStrength -ge 0){$Arguments+="-RuntimeBendingStrength=$($RuntimeBendingStrength.ToString([System.Globalization.CultureInfo]::InvariantCulture))"}
if($HingeBending){$Arguments+='-HingeBending'}
if($Template){$Arguments+=@("-Template=$Template","-MaxSteps=$MaxSteps")}
if($SourceDraft){
    if(-not $Template){throw 'SourceDraft requires Template'}
    $Arguments+="-SourceDraft=$((Resolve-Path -LiteralPath $SourceDraft -ErrorAction Stop).Path)"
}
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @Arguments *> (Join-Path $Report 'Console.log')
$EngineExit=$LASTEXITCODE
if(-not(Test-Path (Join-Path $Report 'metrics.json'))){Get-Content (Join-Path $Report 'Editor.log') -Tail 35; throw "No probe metrics (exit $EngineExit)"}
$Metrics=Get-Content (Join-Path $Report 'metrics.json') -Raw|ConvertFrom-Json
$Metrics|Select-Object passed,reopenedInNewProcess,steps,maxRelativeEdgeError,runtimeParticles,runtimeMaxDisplacementCm,runtimeBodyPenetrationCm|Format-List
if($EngineExit -ne 0 -or -not $Metrics.passed){throw "Probe failed (exit $EngineExit)"}
Write-Output "Report: $Report/metrics.json"
