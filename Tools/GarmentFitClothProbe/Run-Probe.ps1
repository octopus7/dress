param(
    [ValidateSet('All', 'Build', 'Create', 'Reopen')][string]$Mode = 'All',
    [ValidateSet('Regression', 'Sewing', 'BodyPins', 'Integration', 'Rendering', 'All')][string]$Suite = 'Regression',
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
$suiteTests = @{
    Regression = @('InputValidation', 'NativeTenSecondsAndReset')
    Sewing = @('Sewing.EqualLength', 'Sewing.NonuniformRefinement', 'Sewing.DirectionFlip', 'Sewing.MismatchPolicy', 'Sewing.InvalidBoundary', 'Sewing.InvalidCorrespondence', 'Sewing.NativeExperiment')
    BodyPins = @('BodyPins.TargetAndLifecycle', 'BodyPins.NativeFollowReleaseAndReopen')
    Integration = @('Integration.SewnBodyPins')
    Rendering = @('Rendering.SurfacesExcludeDebugAndBody')
}
$selectedTests = if ($Suite -eq 'All') { @($suiteTests.Values | ForEach-Object { $_ }) } else { @($suiteTests[$Suite]) }
$expectedTests = @($selectedTests | ForEach-Object { 'GarmentFitClothProbe.' + $_ } | Sort-Object)
# UE supports '+' OR filters and '^...$' exact names. Keep the original two-test suite reproducible.
$testFilter = ($expectedTests | ForEach-Object { '^' + $_ + '$' }) -join '+'

if ($Mode -in @('All', 'Build')) {
    & $builder GarmentFitClothProbeHostEditor Win64 Development "-Project=$probeProject" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=4 "-Log=$runRoot\Build.log" *> "$runRoot\Build-console.log"
    if ($LASTEXITCODE -ne 0) { Get-Content "$runRoot\Build-console.log" -Tail 45; throw 'Editor build failed.' }
    Write-Output "Editor build passed: $runRoot\Build.log"
}

function Invoke-ProbeRun([string]$Label, [bool]$Reopen) {
    $out = Join-Path $runRoot $Label
    $report = Join-Path $runRoot ($Label + '-Report')
    $probeArgs = @($probeProject, '-unattended', '-nosplash', '-RenderOffscreen',
        "-ExecCmds=Automation RunTests $testFilter", '-TestExit=Automation Test Queue Empty',
        "-ProbeOutput=$out", "-ReportExportPath=$report", "-abslog=$runRoot\$Label.log", '-stdout')
    if ($Reopen) { $probeArgs += '-ProbeReopen' }
    & $editor @probeArgs *> "$runRoot\$Label-console.log"
    $processExit = $LASTEXITCODE
    if ($processExit -ne 0) { throw "$Label engine process failed ($processExit): $runRoot\$Label.log" }
    # UE can exit 0 even when automation assertions fail: the report is authoritative.
    $result = Get-Content -LiteralPath "$report\index.json" -Raw | ConvertFrom-Json
    $actualTests = @($result.tests | ForEach-Object { $_.fullTestPath } | Sort-Object)
    if ($result.failed -ne 0 -or $result.notRun -ne 0 -or ($result.succeeded + $result.succeededWithWarnings) -ne $expectedTests.Count -or
        @(Compare-Object $expectedTests $actualTests).Count -ne 0) {
        foreach ($test in $result.tests) {
            foreach ($entry in $test.entries) {
                if ($entry.event.type -eq 'Error') { Write-Output "$($test.fullTestPath): $($entry.event.message)" }
            }
        }
        throw "$Label automation failed or test selection differs from $Suite. Inspect $report\index.json"
    }
    Write-Output "$Label $Suite passed: $($result.succeeded) clean, $($result.succeededWithWarnings) with warnings. Report: $report\index.json"
    foreach ($name in $(if ($Suite -in @('Regression', 'All')) { @('free', 'sphere') } else { @() })) {
        $metrics = Get-Content -LiteralPath "$out\$name-metrics.json" -Raw | ConvertFrom-Json
        Write-Output ("{0}: mean drop={1:F6} cm; reset={2:F6} cm; replay={3:F6} cm" -f $name, $metrics.mean_free_drop_cm, $metrics.hard_reset_error_cm, $metrics.replay_max_difference_cm)
    }
}

if ($Mode -in @('All', 'Create')) { Invoke-ProbeRun 'Create' $false }
if ($Mode -in @('All', 'Reopen')) {
    $assetRoot = Join-Path $probeRoot 'Plugins\GarmentFitClothProbe\Content'
    $assets = @(Get-ChildItem -LiteralPath $assetRoot -Recurse -File -Filter '*.uasset' | Sort-Object FullName)
    if ($assets.Count -eq 0) { throw 'No saved source assets exist to reopen.' }
    $before = @($assets | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash })
    Invoke-ProbeRun 'Reopen' $true # Always launches a new Unreal process; no in-memory reload shortcut.
    $afterAssets = @(Get-ChildItem -LiteralPath $assetRoot -Recurse -File -Filter '*.uasset' | Sort-Object FullName)
    if (@(Compare-Object @($assets.FullName) @($afterAssets.FullName)).Count -ne 0) { throw 'Reopen created or removed a source asset.' }
    $after = @($assets | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash })
    for ($i = 0; $i -lt $before.Count; $i++) {
        if ($before[$i] -ne $after[$i]) { throw "Reopen modified source asset $($assets[$i])" }
    }
    @(for ($i = 0; $i -lt $assets.Count; $i++) {
        [PSCustomObject]@{asset=$assets[$i].FullName.Substring($assetRoot.Length + 1);sha256_before=$before[$i];sha256_after=$after[$i]}
    }) | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath "$runRoot\SourceAssetHashes.json" -Encoding utf8
    Write-Output "Reopen preserved all $($assets.Count) source asset hashes."
}
# Rendering verifies pixel coverage and deliberate negative controls in each
# process; it does not simulate or emit final-position metrics.
if ($Mode -eq 'All' -and $Suite -ne 'Rendering') {
    $createRoot = Join-Path $runRoot 'Create'
    $reopenRoot = Join-Path $runRoot 'Reopen'
    $metricsFiles = @(Get-ChildItem -LiteralPath $createRoot -Recurse -File -Filter '*metrics.json')
    if ($metricsFiles.Count -eq 0) { throw 'No native simulation metrics were produced.' }
    $comparison = @()
    foreach ($file in $metricsFiles) {
        $relative = $file.FullName.Substring($createRoot.Length + 1)
        $created = (Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json).final_positions
        $reopened = (Get-Content -LiteralPath (Join-Path $reopenRoot $relative) -Raw | ConvertFrom-Json).final_positions
        if ($created.Count -eq 0 -or $created.Count -ne $reopened.Count) { throw "Missing final vertex data in $relative" }
        $maxDifference = 0.0
        for ($i = 0; $i -lt $created.Count; $i++) {
            if ($created[$i].Count -ne 3 -or $reopened[$i].Count -ne 3) { throw "Invalid position in $relative" }
            $distanceSquared = 0.0
            for ($axis = 0; $axis -lt 3; $axis++) {
                $delta = [double]$created[$i][$axis] - [double]$reopened[$i][$axis]
                if ([double]::IsNaN($delta) -or [double]::IsInfinity($delta)) { throw "Non-finite position in $relative" }
                $distanceSquared += $delta * $delta
            }
            $maxDifference = [Math]::Max($maxDifference, [Math]::Sqrt($distanceSquared))
        }
        if ($maxDifference -gt 0.05) { throw "$relative differs after fresh-process reopen: $maxDifference cm" }
        $comparison += [PSCustomObject]@{file=$relative;vertices=$created.Count;maximum_difference_cm=$maxDifference}
        Write-Output "$relative cross-process maximum position difference: $maxDifference cm"
    }
    $comparison | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$runRoot\CrossProcess.json" -Encoding utf8
}
Write-Output "Evidence: $runRoot"
