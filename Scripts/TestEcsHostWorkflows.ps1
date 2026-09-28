param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [ValidateSet('Debug','Release')][string]$Configuration,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot,
    [ValidateRange(10,600)][int]$TimeoutSeconds = 120
)
. (Join-Path $PSScriptRoot 'EcsValidationSupport.ps1')
$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$build = (Resolve-Path -LiteralPath $BuildRoot).Path
$evidence = [System.IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $evidence) { throw 'Use a fresh host evidence directory.' }
New-Item -ItemType Directory -Path $evidence | Out-Null
$sourceHash = Get-EcsSourceReceipt $source $evidence
$commands = New-Object 'System.Collections.Generic.List[object]'
$runs = New-Object 'System.Collections.Generic.List[object]'
$report = [ordered]@{schema_version=1; source_manifest_sha256=$sourceHash; configuration=$Configuration;
    started_utc=[DateTime]::UtcNow.ToString('o'); complete=$false; commands=@(); runs=@()}
$ctest=(Get-Command ctest -CommandType Application).Source
$probeDiscovery=Invoke-EcsLogged $ctest @('--test-dir',$build,'-C',$Configuration,'-R','^RenderingOperationsSmoke$','--show-only=json-v1') (Join-Path $evidence 'graphics-probe-discovery.log') -Quiet
$commands.Add($probeDiscovery)
if ($probeDiscovery.exit_code -ne 0) { throw 'Cannot discover the actual graphics-thread test evidence.' }
$probeListing=Get-Content -LiteralPath $probeDiscovery.log -Raw | ConvertFrom-Json
$workspaceArgument=@($probeListing.tests[0].command | Where-Object { $_ -like '-DTEST_WORKSPACE=*' })
if ($workspaceArgument.Count -ne 1) { throw 'The graphics probe test workspace is ambiguous.' }
$probePath=Join-Path $workspaceArgument[0].Substring(17) 'render-thread-evidence.json'
$probe=Get-Content -LiteralPath $probePath -Raw | ConvertFrom-Json
if ($probe.scope -ne 'RenderGraph pass execution and direct RenderDevice::CreateBuffer' -or
    $probe.main_thread -ne $probe.pass_thread -or $probe.observed_rhi_calls -lt 1 -or $probe.calls_on_worker -ne 0) {
    throw 'The actual render graph/RHI thread observation failed.'
}
$probeCopy=Join-Path $evidence 'render-thread-evidence.json'
Copy-Item -LiteralPath $probePath -Destination $probeCopy
$report['graphics_runtime_probe']=[pscustomobject]@{scope=$probe.scope; path=$probeCopy;
    sha256=(Get-FileHash -LiteralPath $probeCopy -Algorithm SHA256).Hash.ToLowerInvariant();
    note='Host dispatch counters and this executed RenderGraph/RHI probe are separate observations; neither instruments every RHI call.'}
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
public static class EcsBmpEvidence {
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    public static uint ExitCode(IntPtr process) {
        uint code;
        if (!GetExitCodeProcess(process, out code)) throw new System.ComponentModel.Win32Exception();
        return code;
    }
    public static long[] Inspect(string path) {
        byte[] data = File.ReadAllBytes(path);
        if (data.Length < 54 || data[0] != 66 || data[1] != 77) throw new InvalidDataException("Not a complete BMP");
        uint fileSize = BitConverter.ToUInt32(data, 2), offset = BitConverter.ToUInt32(data, 10);
        uint header = BitConverter.ToUInt32(data, 14), compression = BitConverter.ToUInt32(data, 30);
        int width = BitConverter.ToInt32(data, 18), signedHeight = BitConverter.ToInt32(data, 22);
        ushort planes = BitConverter.ToUInt16(data, 26), bits = BitConverter.ToUInt16(data, 28);
        if (header != 40 || width <= 0 || signedHeight >= 0 || signedHeight == int.MinValue ||
            planes != 1 || bits != 32 || compression != 0 || offset != 54 || fileSize != data.Length)
            throw new InvalidDataException("Expected the host's uncompressed, top-down BGRA BMP");
        long height = -(long)signedHeight, bytes = checked((long)width * height * 4);
        if (offset + bytes != data.Length) throw new InvalidDataException("BMP pixel payload length is inconsistent");
        long varied = 0;
        for (long i = offset; i < data.Length; i += 4) {
            if ((data[i] != 0 || data[i+1] != 0 || data[i+2] != 0) &&
                (data[i] != data[offset] || data[i+1] != data[offset+1] || data[i+2] != data[offset+2])) ++varied;
        }
        return new long[] { width, height, varied, bytes };
    }
}
'@
function Save-HostReport {
    $report.commands=$commands.ToArray(); $report.runs=$runs.ToArray()
    Write-EcsJson (Join-Path $evidence 'host-workflows.json') $report
}
function Assert-Captures($value) {
    if (@($value.captures).Count -eq 0) { throw 'The host did not capture a presented frame.' }
    foreach ($capture in $value.captures) {
        if (-not (Test-Path -LiteralPath $capture.path -PathType Leaf) -or $capture.drawn_pixels -le 0 -or
            $capture.width -le 0 -or $capture.height -le 0 -or
            (Get-FileHash -LiteralPath $capture.path -Algorithm SHA256).Hash -ne $capture.sha256) {
            throw 'A presented host capture is absent, empty, or changed.'
        }
        $pixels=[EcsBmpEvidence]::Inspect($capture.path)
        if ($pixels[0] -ne $capture.width -or $pixels[1] -ne $capture.height -or
            $pixels[2] -le 0 -or $pixels[2] -ne $capture.drawn_pixels) { throw 'Recomputed BMP dimensions or pixels do not match the host report.' }
    }
}
function Assert-Host($value,[string]$name,[int]$expectedPid) {
    if ($value.host -ne $name -or -not $value.success -or $value.exit_code -ne 0 -or
        $value.process_id -ne $expectedPid -or $value.frames_rendered -lt 3 -or $value.gui_frames -lt 3 -or
        $value.graphics_calls_on_worker -ne 0 -or $value.main_thread_id -ne $value.graphics_thread_id) {
        throw ('Host workflow failed: '+$name+' / '+$value.error)
    }
    $required=if ($name -eq 'Editor') {
        @('engine_initialized','project_opened','scene_loaded','entity_created','component_edited','undo_redo','saved_reloaded','play_frame_rendered','stopped','shutdown_clean')
    } else { @('hub_initialized','project_listed','project_opened','editor_launched','child_project_loaded','child_play_frame_rendered','child_saved_reloaded','child_shutdown_clean','shutdown_clean') }
    foreach ($event in $required) { if ($event -notin $value.events) { throw ('Missing real host event: '+$name+'/'+$event) } }
    if ($value.event_count -ne @($value.events).Count) { throw 'Host event count is inconsistent.' }
    $intervals=@($value.frame_interval_ms | ForEach-Object { [double]$_ })
    if ($intervals.Count -lt 31 -or $intervals.Count -ne $value.frame_interval_sample_count -or
        $value.frame_interval_scope -ne 'consecutive real host OnUpdate starts, including prior update, GUI and Present') { throw 'Actual host frame timing samples are incomplete.' }
    foreach ($interval in $intervals) { if ([double]::IsNaN($interval) -or [double]::IsInfinity($interval) -or $interval -lt 0) { throw 'Invalid actual host frame duration.' } }
    $ordered=@($intervals | Sort-Object)
    $median=$ordered[[int][Math]::Ceiling($ordered.Count * 0.5)-1]
    $p95=$ordered[[int][Math]::Ceiling($ordered.Count * 0.95)-1]
    if ([Math]::Abs($median-$value.frame_interval_median_ms) -gt 0.00001 -or [Math]::Abs($p95-$value.frame_interval_p95_ms) -gt 0.00001) { throw 'Host frame percentiles do not match the raw interval samples.' }
    Assert-Captures $value
    if ($name -eq 'Editor') {
        if ($value.scene_frames_rendered -lt 3 -or $value.snapshots.entity_uuid -ne $value.snapshots.reloaded_entity_uuid -or
            $value.snapshots.edited_position_x -ne $value.snapshots.reloaded_position_x -or
            -not (Test-Path -LiteralPath $value.snapshots.scene_path -PathType Leaf)) { throw 'The Editor saved-state round trip is incomplete.' }
        $scene=Get-Content -LiteralPath $value.snapshots.scene_path -Raw
        foreach ($expected in @('version: 3',$value.snapshots.entity_uuid,$value.snapshots.mesh_guid,$value.snapshots.material_guid)) {
            if (-not $expected -or -not $scene.Contains($expected)) { throw 'The actual saved scene does not match the Editor snapshot.' }
        }
    }
}
$previousEnvironment=@{}
foreach ($name in @('LOCALAPPDATA','APPDATA','TEMP','TMP','TMPDIR')) { $previousEnvironment[$name]=[Environment]::GetEnvironmentVariable($name,'Process') }
try {
    foreach ($hostName in @('Editor','ProjectHub')) {
        $directory=Join-Path $evidence $hostName
        New-Item -ItemType Directory -Path $directory | Out-Null
        $isolated=Join-Path $directory 'environment'
        New-Item -ItemType Directory -Path $isolated | Out-Null
        foreach ($name in $previousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$isolated,'Process') }
        $executable=Join-Path $build ('bin/'+$Configuration+'-Windows-x64/'+$hostName+'.exe')
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw ('Host executable is missing: '+$executable) }
        $resultPath=Join-Path $directory 'report.json'
        $stdout=Join-Path $directory 'stdout.log'; $stderr=Join-Path $directory 'stderr.log'; $log=Join-Path $directory 'process.log'
        $started=[DateTime]::UtcNow
        $process=Start-Process -FilePath $executable -ArgumentList @('--ecs-smoke',('"'+$resultPath+'"')) -WorkingDirectory (Split-Path -Parent $executable) -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $nativeHandle=$process.Handle
        $observed=New-Object 'System.Collections.Generic.List[object]'
        $owned=@{ $process.Id=$true }
        $timedOut=$false
        try {
            while (-not $process.WaitForExit(100)) {
                foreach ($child in Get-CimInstance Win32_Process | Where-Object { $owned.ContainsKey([int]$_.ParentProcessId) }) {
                    if (-not $owned.ContainsKey([int]$child.ProcessId)) {
                        $owned[[int]$child.ProcessId]=$true
                        $observed.Add([pscustomobject]@{process_id=$child.ProcessId; parent_process_id=$child.ParentProcessId;
                            executable=$child.ExecutablePath; command_line=$child.CommandLine; observed_utc=[DateTime]::UtcNow.ToString('o')})
                    }
                }
                if (([DateTime]::UtcNow-$started).TotalSeconds -ge $TimeoutSeconds) { $timedOut=$true; break }
            }
            if ($timedOut) {
                foreach ($child in Get-CimInstance Win32_Process | Where-Object { $owned.ContainsKey([int]$_.ProcessId) }) {
                    if ($child.CreationDate.ToUniversalTime() -ge $started.AddSeconds(-1)) { Stop-Process -Id $child.ProcessId -Force -ErrorAction SilentlyContinue }
                }
                [void]$process.WaitForExit(5000)
            } else { $process.WaitForExit() }
            $process.Refresh()
            $exitCode=if($timedOut){124}else{[EcsBmpEvidence]::ExitCode($nativeHandle)}
            $text=(Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue)+(Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue)
            [System.IO.File]::WriteAllText($log,[string]$text,(New-Object System.Text.UTF8Encoding($false)))
            $commands.Add([pscustomobject]@{program=$executable; arguments=@('--ecs-smoke',$resultPath); exit_code=$exitCode; expected_exit=0;
                process_id=$process.Id; log=$log; log_sha256=(Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash.ToLowerInvariant();
                started_utc=$started.ToString('o'); finished_utc=[DateTime]::UtcNow.ToString('o'); child_processes=$observed.ToArray()})
            Save-HostReport
            if ($exitCode -ne 0) { throw ('Actual host failed or timed out: '+$hostName+'; inspect '+$directory) }
            $value=Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
            $value | Add-Member -NotePropertyName executable -NotePropertyValue $executable
            $value | Add-Member -NotePropertyName executable_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
            $runs.Add($value); Save-HostReport
            Assert-Host $value $hostName $process.Id
            if ($hostName -eq 'ProjectHub') {
                if ($value.child_process_id -le 0 -or $value.child_process_id -eq $process.Id -or
                    (Get-FileHash -LiteralPath $value.child_executable -Algorithm SHA256).Hash -ne $value.child_executable_sha256) { throw 'Hub child executable identity is invalid.' }
                $child=Get-Content -LiteralPath $value.child_report -Raw | ConvertFrom-Json
                Assert-Host $child 'Editor' $value.child_process_id
                if ($child.snapshots.source_project_path -ne $value.snapshots.project_path) { throw 'The child did not open the project selected by ProjectHub.' }
                if (-not $owned.ContainsKey([int]$value.child_process_id)) { throw 'The driver did not observe the actual Editor child process.' }
            }
        } finally { $process.Dispose() }
        Write-Output ($hostName+' '+$Configuration+' real window workflow passed.')
    }
    $report.complete=$true; $report['finished_utc']=[DateTime]::UtcNow.ToString('o'); Save-HostReport
} finally {
    foreach ($name in $previousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process') }
}
