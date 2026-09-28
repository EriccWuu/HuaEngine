param(
    [Parameter(Mandatory=$true)][string]$LegacyExecutable,
    [Parameter(Mandatory=$true)][string]$NewExecutable,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateRange(1,10000)][int]$Samples = 31,
    [uint32]$Seed = 1729,
    [int[]]$EntityCounts = @(1000,100000,1000000),
    [Parameter(Mandatory=$true)][string]$HostReport,
    [ValidateRange(30,3600)][int]$CaseTimeoutSeconds = 600
)
. (Join-Path $PSScriptRoot 'EcsValidationSupport.ps1')
$sourceRoot = Split-Path -Parent $PSScriptRoot
$legacy = (Resolve-Path -LiteralPath $LegacyExecutable).Path
$current = (Resolve-Path -LiteralPath $NewExecutable).Path
$output = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Use a new comparison output directory.' }
New-Item -ItemType Directory -Path $output | Out-Null
$sourceHash = Get-EcsSourceReceipt $sourceRoot $output
$commands = New-Object 'System.Collections.Generic.List[object]'
$cases = New-Object 'System.Collections.Generic.List[object]'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class EcsBenchmarkProcess {
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    public static uint ExitCode(IntPtr handle) {
        uint code; if (!GetExitCodeProcess(handle, out code)) throw new System.ComponentModel.Win32Exception(); return code;
    }
}
'@
function Invoke-BenchmarkCase([string]$Program,[string[]]$Arguments,[string]$Log) {
    $started=[DateTime]::UtcNow
    $stderr=$Log+'.stderr.log'
    $process=Start-Process -FilePath $Program -ArgumentList $Arguments -WorkingDirectory (Split-Path -Parent $Program) -WindowStyle Hidden -PassThru -RedirectStandardOutput $Log -RedirectStandardError $stderr
    $handle=$process.Handle
    $nextProgress=60
    $timedOut=$false
    try {
        while (-not $process.WaitForExit(1000)) {
            $elapsed=([DateTime]::UtcNow-$started).TotalSeconds
            if ($elapsed -ge $nextProgress) { Write-Host ('Benchmark PID {0}: {1:F0}s elapsed: {2}' -f $process.Id,$elapsed,($Arguments -join ' ')); $nextProgress+=60 }
            if ($elapsed -ge $CaseTimeoutSeconds) { $timedOut=$true; $process.Kill(); $process.WaitForExit(); break }
        }
        $process.WaitForExit()
        $exitCode=if($timedOut){124}else{[EcsBenchmarkProcess]::ExitCode($handle)}
        return [pscustomobject]@{program=$Program; arguments=$Arguments; exit_code=$exitCode; expected_exit=0; process_id=$process.Id;
            log=$Log; log_sha256=(Get-FileHash -LiteralPath $Log -Algorithm SHA256).Hash.ToLowerInvariant();
            stderr=$stderr; stderr_sha256=(Get-FileHash -LiteralPath $stderr -Algorithm SHA256).Hash.ToLowerInvariant();
            timeout_seconds=$CaseTimeoutSeconds; timed_out=$timedOut; started_utc=$started.ToString('o'); finished_utc=[DateTime]::UtcNow.ToString('o')}
    } finally { $process.Dispose() }
}
$processor = Get-CimInstance Win32_Processor | Select-Object -First 1
$computer = Get-CimInstance Win32_ComputerSystem
$operatingSystem = Get-CimInstance Win32_OperatingSystem
$busyNames=@('MSBuild','cl','link','ninja','ctest','Editor','ProjectHub','ECSBenchmark','ECSBenchmark-entt-hash-fixed')
$busy=@(Get-Process | Where-Object { $_.ProcessName -in $busyNames } | Select-Object Id,ProcessName)
if ($busy.Count) { Write-EcsJson (Join-Path $output 'busy-processes.json') $busy; throw 'Build, test, benchmark or graphical host work is still running.' }
$loadSamples=@()
foreach ($sample in 1..5) {
    $loadSamples += [pscustomobject]@{utc=[DateTime]::UtcNow.ToString('o'); processor_load_percent=(Get-CimInstance Win32_Processor | Measure-Object LoadPercentage -Average).Average}
    if ($sample -lt 5) { Start-Sleep -Seconds 1 }
}
$report = [ordered]@{
    schema_version = 2; source_manifest_sha256 = $sourceHash
    started_utc = [DateTime]::UtcNow.ToString('o'); configuration = 'Release'
    legacy_executable = $legacy; legacy_sha256 = (Get-FileHash -LiteralPath $legacy -Algorithm SHA256).Hash.ToLowerInvariant()
    new_executable = $current; new_sha256 = (Get-FileHash -LiteralPath $current -Algorithm SHA256).Hash.ToLowerInvariant()
    processor = $processor.Name; logical_processors = $computer.NumberOfLogicalProcessors
    physical_memory_bytes = $computer.TotalPhysicalMemory; powershell = $PSVersionTable.PSVersion.ToString()
    operating_system = [pscustomobject]@{caption=$operatingSystem.Caption; version=$operatingSystem.Version; build=$operatingSystem.BuildNumber; architecture=$operatingSystem.OSArchitecture}
    idle_conditions = [pscustomobject]@{checked_process_names=$busyNames; concurrent_build_test_host_processes=$busy; startup_processor_load_samples=$loadSamples;
        scope='Measured immediately before the matrix. Background operating-system work was not disabled; this is not a claim of zero machine load.'}
    scope = 'Whole query step including dispatch and wait; actual host frames are reported separately.'
    comparison_caveats = @(
        'New setup excludes explicit type/reflection registration; the preserved legacy setup includes first-use component pool registration.',
        'New cold time includes Timeline construction and parallel worker startup, not just cold Query matching.',
        'The new benchmark counts visited rows with one atomic update per batch; the legacy version increments a local counter per entity.',
        'Dispatch time can overlap worker execution and wait time includes outstanding callbacks; neither is pure scheduler overhead.',
        'C++ new counters count calls, not allocated bytes or operating-system allocations.',
        'The nonpod scenario stores owned payloads; its hot arithmetic query reads and writes only position and velocity.',
        'Legacy dispatch, chunk and compaction counters were not instrumented and remain unavailable, never zero-filled.'
    )
    commands = @(); cases = @(); complete = $false
}
$actualHosts=Get-Content -LiteralPath $HostReport -Raw | ConvertFrom-Json
if (-not $actualHosts.complete -or $actualHosts.configuration -ne 'Release' -or @($actualHosts.runs).Count -ne 2) { throw 'A complete actual Release host report is required.' }
$frames=@($actualHosts.runs | ForEach-Object {
    if (@($_.frame_interval_ms).Count -lt 31) { throw 'The actual host report lacks frame duration samples.' }
    [pscustomobject]@{host=$_.host; process_id=$_.process_id; scope=$_.frame_interval_scope; samples_ms=$_.frame_interval_ms;
        median_ms=$_.frame_interval_median_ms; p95_ms=$_.frame_interval_p95_ms; configuration='Release'}
})
$report['actual_host_frames']=[pscustomobject]@{report=[System.IO.Path]::GetFullPath($HostReport);
    sha256=(Get-FileHash -LiteralPath $HostReport -Algorithm SHA256).Hash.ToLowerInvariant(); runs=$frames;
    note='Actual GUI/Present frame intervals are separate observations and are not derived from ECS benchmark query steps.'}
foreach ($scenario in @('dense','fragmented','nonpod','structural')) {
    foreach ($count in $EntityCounts) {
        if ($count -le 0) { throw 'Entity counts must be positive.' }
        $checksum = $null
        foreach ($mode in @('entt-facade','soa-serial','soa-parallel')) {
            $program = if ($mode -eq 'entt-facade') { $legacy } else { $current }
            $arguments = @('--entities',[string]$count,'--scenario',$scenario,'--samples',[string]$Samples,'--seed',[string]$Seed)
            if ($mode -ne 'entt-facade') { $arguments += @('--mode', $mode.Substring(4)) }
            $log = Join-Path $output ($mode+'-'+$scenario+'-'+$count+'.json')
            Write-Host ('Starting '+$mode+'/'+$scenario+'/'+$count)
            $receipt = Invoke-BenchmarkCase $program $arguments $log
            $commands.Add($receipt)
            if ($receipt.exit_code -ne 0) {
                $report.commands=$commands.ToArray(); $report.cases=$cases.ToArray()
                Write-EcsJson (Join-Path $output 'commands.json') $commands.ToArray()
                Write-EcsJson (Join-Path $output 'performance-comparison.json') $report
                throw "Benchmark failed: $mode/$scenario/$count"
            }
            $case = Get-Content -LiteralPath $log -Raw | ConvertFrom-Json
            if ($case.configuration -ne 'Release' -or $case.entities -ne $count -or $case.scenario -ne $scenario -or
                $case.samples -ne $Samples -or $case.warmup_samples -ne 4 -or $case.verified_iterations -ne $Samples+5 -or
                $case.seed -ne $Seed -or @($case.samples_ms).Count -ne $Samples) { throw 'Benchmark workload does not match the requested matrix.' }
            if ($mode -eq 'entt-facade') {
                if ($case.backend -ne 'entt-facade') { throw 'The saved baseline is not the legacy backend.' }
                foreach ($field in @('dispatch_total_ms','wait_total_ms','chunks_before_compaction','chunks_after_compaction','compaction_ms','compaction_moved_bytes','compaction_returned_bytes','pool_bytes','occupancy')) {
                    $case | Add-Member -NotePropertyName $field -NotePropertyValue $null
                }
                $case | Add-Member -NotePropertyName unavailable_metrics_reason -NotePropertyValue 'The preserved legacy executable did not instrument dispatch, chunks or compaction.'
                $checksum = [string]$case.checksum
            } else {
                if ($case.backend -ne 'hua-runtime' -or $case.mode -ne $mode.Substring(4)) { throw 'Unexpected runtime backend or mode.' }
                if ([string]$case.checksum -ne $checksum) { throw 'Legacy and runtime results differ.' }
                if ($scenario -in @('dense','nonpod') -and ($case.hot_group_match_delta -ne 0 -or $case.hot_layout_bind_delta -ne 0)) {
                    throw 'A stable hot Query rebuilt matching or layout bindings.'
                }
                $case.backend = $mode
            }
            $case | Add-Member -NotePropertyName raw_result -NotePropertyValue $log
            $case | Add-Member -NotePropertyName raw_sha256 -NotePropertyValue $receipt.log_sha256
            $cases.Add($case)
            Write-Host ('Completed {0}/{1}/{2}: median {3:F3} ms, P95 {4:F3} ms' -f $mode,$scenario,$count,$case.median_ms,$case.p95_ms)
            $report.commands = $commands.ToArray(); $report.cases = $cases.ToArray()
            Write-EcsJson (Join-Path $output 'performance-comparison.json') $report
        }
    }
}
$report.complete = $true
$report['finished_utc'] = [DateTime]::UtcNow.ToString('o')
Write-EcsJson (Join-Path $output 'performance-comparison.json') $report
