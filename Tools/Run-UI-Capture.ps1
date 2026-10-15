param([string]$RunName='UI-Capture',[int]$Width=1920,[int]$Height=1080,[string]$Garment='TShirt',[string]$Map='L_GarmentCreation',[string]$Pose='Neutral',[string]$SourceSlot='',[string]$PackagedExe='',[switch]$Complete,[switch]$Wireframe)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Join-Path $root ('Saved/Automation/Implementation/'+$RunName)
if(Test-Path -LiteralPath $report){throw "Refusing stale report $report"}
New-Item -ItemType Directory -Path $report | Out-Null
$arguments=@("/Game/Maps/$Map",'-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height",'-RenderOffscreen','-unattended','-nosound','-nosplash',"-LaceLoftUICapture=$report/Creation.png","-UICaptureGarment=$Garment","-UICapturePose=$Pose","-UICaptureSourceSlot=$SourceSlot","-abslog=$report/Runtime.log")
if($Complete){$arguments+='-UIProbeComplete'}
if($Wireframe){$arguments+='-UIProbeWireframe'}
if($PackagedExe){
    $quoted=@($arguments | ForEach-Object { '"'+$_.Replace('"','\"')+'"' })
    $process=Start-Process -FilePath $PackagedExe -ArgumentList $quoted -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput "$report/Console.log" -RedirectStandardError "$report/Stderr.log"
    $engineExit=$process.ExitCode
}else{
    & 'E:/WorkTemp/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' "$root/LaceLoft.uproject" -game @arguments *> "$report/Console.log"
    $engineExit=$LASTEXITCODE
}
if($engineExit -ne 0 -or -not(Test-Path -LiteralPath "$report/Creation.png")){throw 'UI capture failed'}
if($Map -eq 'L_GarmentCreation' -or $Map -eq 'L_FittingRoom'){
    $inputReport=Get-Content -LiteralPath "$report/routed-input.json" -Raw | ConvertFrom-Json
    if(-not $inputReport.passed){throw 'Actual Slate hit-tested input failed'}
}
Add-Type -AssemblyName System.Drawing
$bitmap=[System.Drawing.Bitmap]::new("$report/Creation.png")
try{if($bitmap.Width -ne $Width -or $bitmap.Height -ne $Height){throw "Wrong actual resolution $($bitmap.Width)x$($bitmap.Height)"}}finally{$bitmap.Dispose()}
Write-Output "UI image: $report/Creation.png"
