param(
    [string]$EngineRoot='E:/WorkTemp/Epic Games/UE_5.8',
    [ValidateSet('TShirt','DolphinShorts')][string]$Template='TShirt',
    [string]$RunName=('Fit-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [ValidateRange(1,10000)][int]$MaxSteps=4800,
    [ValidateRange(-2000,2000)][double]$GravityCm=-980,
    [ValidateRange(0,1)][double]$BendingStrength=0.05,
    [ValidateRange(1,200)][int]$Iterations=100,
    [string]$SourceDraft,
    [switch]$Render
)
$ErrorActionPreference='Stop'
$RepoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$HostRoot=Join-Path $RepoRoot 'PluginDev/LaceGarmentHost'
if($RunName -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid run name'}
$Report=Join-Path $HostRoot "Saved/Verification/$RunName"
if(Test-Path -LiteralPath $Report){throw 'Refusing stale fit output'}
New-Item -ItemType Directory -Path $Report | Out-Null
$GravityArgument=$GravityCm.ToString([Globalization.CultureInfo]::InvariantCulture)
$BendingArgument=$BendingStrength.ToString([Globalization.CultureInfo]::InvariantCulture)
$Arguments=@((Join-Path $HostRoot 'LaceGarmentHost.uproject'),'-run=LaceGarmentFit',"-Template=$Template","-MaxSteps=$MaxSteps","-GravityCm=$GravityArgument","-BendingStrength=$BendingArgument","-Iterations=$Iterations",'-unattended','-nosplash','-nosound',"-ReportDir=$Report","-abslog=$Report/Editor.log")
if($SourceDraft){$Arguments+="-SourceDraft=$((Resolve-Path -LiteralPath $SourceDraft -ErrorAction Stop).Path)"}
if($Render){$Arguments+=@('-Render','-AllowCommandletRendering','-RenderOffscreen')}else{$Arguments+='-nullrhi'}
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @Arguments *> (Join-Path $Report 'Console.log')
$EngineExit=$LASTEXITCODE
$Metrics=Join-Path $Report 'fit.json'
if(-not(Test-Path -LiteralPath $Metrics)){Get-Content (Join-Path $Report 'Editor.log') -Tail 35; throw "No fit report (exit $EngineExit)"}
$Result=Get-Content -LiteralPath $Metrics -Raw | ConvertFrom-Json
$Result|Select-Object template,fitted,error,steps,vertices,initialVertexPenetrationCm,maxSeamGapCm,maxRelativeEdgeError,rendered|Format-List
Write-Output "Report: $Metrics"
if($EngineExit -ne 0 -or -not $Result.fitted){throw "Template fit failed (exit $EngineExit)"}
