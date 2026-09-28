param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [int[]]$EntityCounts = @(1000, 100000, 1000000),
    [ValidateRange(1, 10000)][int]$Samples = 31,
    [uint32]$Seed = 1729,
    [ValidateSet('serial', 'parallel')][string[]]$Modes = @('serial', 'parallel')
)

$ErrorActionPreference = 'Stop'
$benchmarkExecutable = (Resolve-Path -LiteralPath $Executable).Path
$benchmarkOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $benchmarkOutput -Force | Out-Null
$benchmarkResults = @()
$benchmarkStarted = [DateTime]::UtcNow.ToString('o')
$benchmarkEncoding = New-Object System.Text.UTF8Encoding($false)

foreach ($scenario in @('dense', 'fragmented', 'nonpod', 'structural')) {
    foreach ($entityCount in $EntityCounts) {
        if ($entityCount -lt 1 -or $entityCount -gt 10000000) {
            throw 'Entity counts must be between 1 and 10000000.'
        }
      foreach ($mode in $Modes) {
        $resultPath = Join-Path $benchmarkOutput ($mode + '-' + $scenario + '-' + $entityCount + '.json')
        $errorPath = Join-Path $benchmarkOutput ($mode + '-' + $scenario + '-' + $entityCount + '.stderr.log')
        $benchmarkArguments = @('--entities', $entityCount, '--scenario', $scenario,
            '--samples', $Samples, '--seed', $Seed, '--mode', $mode)
        $benchmarkText = & $benchmarkExecutable @benchmarkArguments 2> $errorPath
        if ($LASTEXITCODE -ne 0) {
            throw ('Benchmark failed: ' + $mode + '/' + $scenario + '/' + $entityCount + '. See ' + $errorPath)
        }
        [System.IO.File]::WriteAllText($resultPath,
            ($benchmarkText -join [Environment]::NewLine), $benchmarkEncoding)
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        if ($result.mode -ne $mode) { throw 'Benchmark mode does not match the requested mode.' }
        $benchmarkResults += $result
        Write-Output ('{0}/{1}/{2}: median {3:F3} ms, p95 {4:F3} ms' -f
            $mode, $scenario, $entityCount, $result.median_ms, $result.p95_ms)
      }
    }
}

$processor = Get-CimInstance Win32_Processor | Select-Object -First 1
$computer = Get-CimInstance Win32_ComputerSystem
$manifest = [ordered]@{
    schema = 1
    started_utc = $benchmarkStarted
    finished_utc = [DateTime]::UtcNow.ToString('o')
    executable = $benchmarkExecutable
    executable_sha256 = (Get-FileHash -LiteralPath $benchmarkExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
    processor = $processor.Name
    logical_processors = $computer.NumberOfLogicalProcessors
    physical_memory_bytes = $computer.TotalPhysicalMemory
    powershell = $PSVersionTable.PSVersion.ToString()
    results = $benchmarkResults
}
[System.IO.File]::WriteAllText((Join-Path $benchmarkOutput 'benchmark-report.json'),
    ($manifest | ConvertTo-Json -Depth 6), $benchmarkEncoding)
Write-Output ('Saved benchmark report: ' + (Join-Path $benchmarkOutput 'benchmark-report.json'))
