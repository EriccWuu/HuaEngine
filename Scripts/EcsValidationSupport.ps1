$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop' -or $PSVersionTable.PSVersion.Major -ne 5) {
    throw 'System Windows PowerShell 5.1 is required.'
}

function Write-EcsJson([string]$Path, $Value) {
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 20), $encoding)
}

function Get-EcsSourceReceipt([string]$SourceRoot, [string]$EvidenceRoot) {
    $rg = (Get-Command rg -CommandType Application).Source
    $roots = @('HuaEngine/src', 'Editor/src', 'ProjectHub/src', 'CLI/src', 'Tests', 'Tools/Meta', 'Tools/Reflection', 'cmake', 'Scripts') |
        ForEach-Object { Join-Path $SourceRoot $_ }
    $paths = @(& $rg --files @roots -g '*.cpp' -g '*.h' -g '*.cmake' -g '*.in' -g '*.ps1' -g '*.py' -g 'CMakeLists.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Source enumeration failed.' }
    $paths += @('CMakeLists.txt', 'HuaEngine/CMakeLists.txt', 'Editor/CMakeLists.txt', 'ProjectHub/CMakeLists.txt') |
        ForEach-Object { Join-Path $SourceRoot $_ }
    $files = @($paths | Sort-Object -Unique | ForEach-Object {
        [pscustomobject]@{ path = [System.IO.Path]::GetFullPath($_); sha256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    $manifest = Join-Path $EvidenceRoot 'source-manifest.json'
    Write-EcsJson $manifest ([ordered]@{ schema_version = 1; files = $files })
    return (Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Invoke-EcsLogged([string]$Program, [string[]]$Arguments, [string]$Log, [switch]$Quiet) {
    $start = [DateTime]::UtcNow.ToString('o')
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    if ($Quiet) { & $Program @Arguments 2>&1 | Out-File -LiteralPath $Log -Encoding utf8 }
    else { & $Program @Arguments 2>&1 | Tee-Object -FilePath $Log | Out-Host }
    $code = $LASTEXITCODE
    $ErrorActionPreference = $previous
    return [pscustomobject]@{
        program = $Program; arguments = $Arguments; exit_code = $code; expected_exit = 0
        log = $Log; log_sha256 = (Get-FileHash -LiteralPath $Log -Algorithm SHA256).Hash.ToLowerInvariant()
        started_utc = $start; finished_utc = [DateTime]::UtcNow.ToString('o')
    }
}
