param(
    [string]$EngineRoot='E:\WorkTemp\Epic Games\UE_5.8',
    [string]$PackagedExe,
    [ValidateSet('', 'TShirt', 'DolphinShorts', 'Both')][string]$Garment='',
    [string]$RunName=('Cloth-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
)
$ErrorActionPreference='Stop'
$ProjectRoot=Split-Path -Parent $PSScriptRoot
$Report=Join-Path $ProjectRoot ('Saved\Automation\Implementation\'+$RunName)
if(Test-Path -LiteralPath $Report){throw "Refusing stale report: $Report"}
New-Item -ItemType Directory -Path $Report | Out-Null
$Arguments=@('/Game/Development/ClothProbe/L_ClothProbe','-windowed','-ForceRes','-ResX=1280','-ResY=960',
    '-RenderOffscreen','-unattended','-nosound','-nosplash','-LaceLoftClothProbe',
    "-ProbeReportDir=$Report","-abslog=$Report\Runtime.log",
    '-ExecCmds=t.MaxFPS 60,p.ClothPhysics.WaitForParallelClothTask 1,p.ChaosClothAsset.WaitForAsyncClothInitialization 1,p.ChaosClothAsset.SimulationDataCheckFinite 1,p.ChaosClothAsset.SimulationDataResetOnNonFinite 0')
if($Garment){$Arguments+="-GarmentProbe=$Garment"}
if($PackagedExe){$Executable=$PackagedExe}
else{
    $Executable=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $Arguments=@((Join-Path $ProjectRoot 'LaceLoft.uproject'),'-game')+$Arguments
}
if(-not(Test-Path -LiteralPath $Executable)){throw "Missing executable: $Executable"}
if($PackagedExe){
    # The Windows GUI bootstrapper otherwise returns to PowerShell before the child game finishes.
    $QuotedArguments=@($Arguments | ForEach-Object { '"'+$_.Replace('"','\"')+'"' })
    $Process=Start-Process -FilePath $Executable -ArgumentList $QuotedArguments -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $Report 'Console.log') -RedirectStandardError (Join-Path $Report 'Stderr.log')
    $EngineExit=$Process.ExitCode
}
else{
    & $Executable @Arguments *> (Join-Path $Report 'Console.log')
    $EngineExit=$LASTEXITCODE
}
$MetricsFile=Join-Path $Report 'metrics.json'
if(-not(Test-Path -LiteralPath $MetricsFile)){throw "No metrics generated; engine exit $EngineExit"}
$Metrics=Get-Content -LiteralPath $MetricsFile -Raw | ConvertFrom-Json
Add-Type -AssemblyName System.Drawing
$Images=@()
$LastPose=if($Garment){3}else{1}
foreach($Pose in 0..$LastPose){foreach($View in 0..2){
    $File=Join-Path $Report "Pose${Pose}_View${View}.png"
    $Bitmap=[System.Drawing.Bitmap]::new($File)
    try{
        $Visible=0;$Blue=0;$Red=0;$Samples=0
        for($Y=0;$Y -lt $Bitmap.Height;$Y+=4){for($X=0;$X -lt $Bitmap.Width;$X+=4){
            $Pixel=$Bitmap.GetPixel($X,$Y);$Samples++
            if([Math]::Max($Pixel.R,[Math]::Max($Pixel.G,$Pixel.B)) -gt 20){$Visible++}
            if($Pixel.R -gt 40 -and $Pixel.R -gt $Pixel.B*1.2){$Red++}
            if($Garment -eq 'TShirt' -or $Garment -eq 'Both'){
                if($Pixel.G -gt 40 -and $Pixel.G -gt $Pixel.R*1.2){$Blue++}
            }elseif($Garment -eq 'DolphinShorts'){
                if($Pixel.R -gt 40 -and $Pixel.R -gt $Pixel.B*1.2){$Blue++}
            }elseif($Pixel.B -gt 40 -and $Pixel.B -gt $Pixel.R*1.2){$Blue++}
        }}
        $Images+= [pscustomobject]@{file=$File;width=$Bitmap.Width;height=$Bitmap.Height;visible_fraction=$Visible/$Samples;colored_samples=$Blue}
        if($Visible/$Samples -lt .01 -or $Blue -lt 20){throw "Empty/dark scene or no visible cloth: $File"}
        if($Garment -eq 'Both' -and $Red -lt 20){throw "Bottom garment missing: $File"}
    }finally{$Bitmap.Dispose()}
}}
$Images | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Report 'image-validation.json')
$Metrics.poses | Format-Table
if($EngineExit -ne 0 -or -not $Metrics.passed){throw "Cloth probe failed: $($Metrics.errors -join '; ') (exit $EngineExit)"}
Write-Output "Cloth runtime and image validation passed: $Report"
