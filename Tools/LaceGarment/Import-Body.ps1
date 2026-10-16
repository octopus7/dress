param(
    [string]$EngineRoot='E:/WorkTemp/Epic Games/UE_5.8',
    [string]$RunName=('Body-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
    [switch]$Render,
    [switch]$RepairSkin,
    [string]$InspectionTexture,
    [ValidateSet('Reference','ArmsRaised','ArmsForward','Elbows90','Knees90','Sitting','HeadTurn')][string]$InspectionPose
)
$ErrorActionPreference='Stop'
if($RepairSkin -and ($InspectionTexture -or $InspectionPose -or $Render)){throw 'RepairSkin cannot be combined with read-only inspection or rendering'}
$RepoRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$HostRoot=Join-Path $RepoRoot 'PluginDev/LaceGarmentHost'
if($RunName -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid run name'}
$Report=Join-Path $HostRoot "Saved/Verification/$RunName"
if(Test-Path -LiteralPath $Report){throw 'Refusing stale report'}
New-Item -ItemType Directory -Path $Report | Out-Null
$Arguments=@((Join-Path $HostRoot 'LaceGarmentHost.uproject'),'-run=LaceGarmentBody','-unattended','-nosplash','-nosound',"-ReportDir=$Report","-abslog=$Report/Editor.log")
if($RepairSkin){$Arguments+='-RepairSkin'}
if($InspectionTexture){$Arguments+="-InspectionTexture=$((Resolve-Path -LiteralPath $InspectionTexture -ErrorAction Stop).Path)"; $Render=$true}
if($InspectionPose){$Arguments+="-InspectionPose=$InspectionPose"; $Render=$true}
if($Render){$Arguments+=@('-Render','-AllowCommandletRendering','-RenderOffscreen')}else{$Arguments+='-nullrhi'}
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @Arguments *> (Join-Path $Report 'Console.log')
$EngineExit=$LASTEXITCODE
if($RepairSkin){
    $SkinPath=Join-Path $Report 'skin-repair.json'
    if($EngineExit -ne 0 -or -not(Test-Path -LiteralPath $SkinPath)){throw "Body skin repair failed (exit $EngineExit)"}
    $Skin=Get-Content -LiteralPath $SkinPath -Raw | ConvertFrom-Json
    if(-not $Skin.succeeded){throw $Skin.error}
    $Skin | Select-Object skinRepairVersion,sourceProfile,changedVertices,saved | Format-List
    Write-Output "Body skin repair report: $SkinPath; pose acceptance must be checked separately."
    return
}
if($InspectionTexture -or $InspectionPose){
    $InspectionPath=Join-Path $Report 'inspection.json'
    if($EngineExit -ne 0 -or -not(Test-Path -LiteralPath $InspectionPath)){throw "Body visual inspection failed (exit $EngineExit)"}
    $Inspection=Get-Content -LiteralPath $InspectionPath -Raw | ConvertFrom-Json
    if(-not $Inspection.rendered -or -not $Inspection.poseDataValid -or $Inspection.assetsSaved){throw 'Invalid read-only inspection report'}
    Write-Output "Read-only body views: $Report; visual review is still required."
    return
}
if($EngineExit -ne 0 -or -not(Test-Path -LiteralPath (Join-Path $Report 'body.json'))){
    Get-Content (Join-Path $Report 'Editor.log') -Tail 30
    throw "Body import failed (exit $EngineExit)"
}
$ReportData=Get-Content (Join-Path $Report 'body.json') -Raw|ConvertFrom-Json
$ReportData|Select-Object heightCm,bones,physicsBodies|Format-List
Write-Output "Import completed; acceptance tests must be run separately. Report: $Report/body.json"
