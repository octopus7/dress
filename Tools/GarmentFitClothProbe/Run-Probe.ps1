param(
    [ValidateSet('All', 'Build', 'Create', 'Reopen')][string]$Mode = 'All',
    [string]$EngineRoot = 'E:\WorkTemp\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$probeRoot = Join-Path $repoRoot 'PluginDev\GarmentFitClothProbeHost'
$probeProject = Join-Path $probeRoot 'GarmentFitClothProbeHost.uproject'
$runRoot = Join-Path $probeRoot ('Saved\Probe\Runs\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$builder = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path -LiteralPath $editor) -or !(Test-Path -LiteralPath $probeProject)) { throw 'Engine or isolated host missing.' }

if ($Mode -in @('All', 'Build')) {
    & $builder GarmentFitClothProbeHostEditor Win64 Development "-Project=$probeProject" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=4 "-Log=$runRoot\Build.log" *> "$runRoot\Build-console.log"
    if ($LASTEXITCODE -ne 0) { Get-Content "$runRoot\Build-console.log" -Tail 45; throw 'Editor build failed.' }
    Write-Output "Editor build passed: $runRoot\Build.log"
}

function Invoke-ProbeRun([string]$Label, [bool]$Reopen) {
    $out = Join-Path $runRoot $Label
    $report = Join-Path $runRoot ($Label + '-Report')
    $probeArgs = @($probeProject, '-unattended', '-nosplash', '-RenderOffscreen',
        '-ExecCmds=Automation RunTests GarmentFitClothProbe', '-TestExit=Automation Test Queue Empty',
        "-ProbeOutput=$out", "-ReportExportPath=$report", "-abslog=$runRoot\$Label.log", '-stdout')
    if ($Reopen) { $probeArgs += '-ProbeReopen' }
    & $editor @probeArgs *> "$runRoot\$Label-console.log"
    $processExit = $LASTEXITCODE
    if ($processExit -ne 0) { throw "$Label engine process failed ($processExit): $runRoot\$Label.log" }
    # UE can exit 0 even when automation assertions fail: the report is authoritative.
    $result = Get-Content -LiteralPath "$report\index.json" -Raw | ConvertFrom-Json
    if ($result.failed -ne 0 -or $result.notRun -ne 0 -or ($result.succeeded + $result.succeededWithWarnings) -ne 2) {
        throw "$Label automation failed or did not run exactly two tests. Inspect $report\index.json"
    }
    Write-Output "$Label passed: $result"
    foreach ($name in @('free', 'sphere')) {
        $metrics = Get-Content -LiteralPath "$out\$name-metrics.json" -Raw | ConvertFrom-Json
        Write-Output ("{0}: mean drop={1:F6} cm; reset={2:F6} cm; replay={3:F6} cm" -f $name, $metrics.mean_free_drop_cm, $metrics.hard_reset_error_cm, $metrics.replay_max_difference_cm)
    }
}

if ($Mode -in @('All', 'Create')) { Invoke-ProbeRun 'Create' $false }
if ($Mode -in @('All', 'Reopen')) {
    $assets = @('CA_Free.uasset', 'CA_Sphere.uasset', 'PA_Sphere.uasset') | ForEach-Object {
        Join-Path $probeRoot "Plugins\GarmentFitClothProbe\Content\Experiment\$_"
    }
    $before = @($assets | ForEach-Object { (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash })
    Invoke-ProbeRun 'Reopen' $true # Always launches a new Unreal process; no in-memory reload shortcut.
    $after = @($assets | ForEach-Object { (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash })
    for ($i = 0; $i -lt $before.Count; $i++) {
        if ($before[$i] -ne $after[$i]) { throw "Reopen modified source asset $($assets[$i])" }
    }
    Write-Output 'Reopen preserved all three source asset hashes.'
}
if ($Mode -eq 'All') {
    foreach ($name in @('free', 'sphere')) {
        $created = (Get-Content "$runRoot\Create\$name-metrics.json" -Raw | ConvertFrom-Json).final_positions
        $reopened = (Get-Content "$runRoot\Reopen\$name-metrics.json" -Raw | ConvertFrom-Json).final_positions
        if ($created.Count -ne 441 -or $reopened.Count -ne 441) { throw 'Missing final vertex data.' }
        $maxDifference = 0.0
        for ($i = 0; $i -lt 441; $i++) {
            $distanceSquared = 0.0
            for ($axis = 0; $axis -lt 3; $axis++) {
                $delta = [double]$created[$i][$axis] - [double]$reopened[$i][$axis]
                $distanceSquared += $delta * $delta
            }
            $maxDifference = [Math]::Max($maxDifference, [Math]::Sqrt($distanceSquared))
        }
        if ([double]::IsNaN($maxDifference) -or $maxDifference -gt 0.05) { throw "$name differs after fresh-process reopen: $maxDifference cm" }
        Write-Output "$name cross-process maximum position difference: $maxDifference cm"
    }
}
Write-Output "Evidence: $runRoot"
