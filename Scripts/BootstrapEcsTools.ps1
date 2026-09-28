param(
    [Parameter(Mandatory = $true)][string]$ToolsRoot,
    [Parameter(Mandatory = $true)][string]$EvidenceRoot,
    [string]$LockFile = '',
    [string]$CMake = 'cmake',
    [string]$VsRoot = '',
    [ValidateSet('all', 'python', 'llvm')][string]$Tool = 'all',
    [switch]$VerifyDownloadsOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $LockFile) { $LockFile = Join-Path $PSScriptRoot '../Tools/Meta/toolchain-lock.json' }
if ($PSVersionTable.PSEdition -ne 'Desktop' -or $PSVersionTable.PSVersion.Major -ne 5) {
    throw 'Run this script with system Windows PowerShell 5.1.'
}

$ToolsRoot = [IO.Path]::GetFullPath($ToolsRoot)
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
$LockFile = [IO.Path]::GetFullPath($LockFile)
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Assert-Descendant([string]$Candidate, [string]$Parent) {
    $absolute = [IO.Path]::GetFullPath($Candidate)
    $prefix = [IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $absolute.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside its intended workspace: $absolute"
    }
}
Assert-Descendant $ToolsRoot $repositoryRoot
Assert-Descendant $EvidenceRoot $repositoryRoot
New-Item -ItemType Directory -Force -Path $ToolsRoot, $EvidenceRoot | Out-Null
$downloadRoot = Join-Path $ToolsRoot 'downloads'
New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
$runId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
$logPath = Join-Path $EvidenceRoot "bootstrap-$runId.log"
$reportPath = Join-Path $EvidenceRoot "bootstrap-$runId.json"
$report = [ordered]@{
    schemaVersion = 1
    startedUtc = [DateTime]::UtcNow.ToString('o')
    toolsRoot = $ToolsRoot
    lockFile = $LockFile
    powershell = $PSVersionTable.PSVersion.ToString()
    verifyOnly = [bool]$VerifyDownloadsOnly
    succeeded = $false
    tools = @()
    error = $null
}
function Write-Evidence([string]$Message) {
    $line = '[' + [DateTime]::UtcNow.ToString('o') + '] ' + $Message
    Write-Host $line
    Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
}
function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    Write-Evidence ($Executable + ' ' + ($Arguments -join ' '))
    $savedErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Executable @Arguments 2>&1 | ForEach-Object { Write-Evidence ([string]$_) }
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorAction
    if ($exitCode -ne 0) { throw "Command failed with exit code ${exitCode}: $Executable" }
}
function Get-VerifiedHash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Archive is missing: $Path" }
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Evidence "SHA256 $actual $Path"
    if ($actual -ne $Expected.ToLowerInvariant()) {
        throw "SHA-256 mismatch for $Path. Expected $Expected; actual $actual. Cached file was preserved for diagnosis."
    }
    return $actual
}

try {
    $CMake = (Get-Command $CMake -CommandType Application -ErrorAction Stop).Source
    $lock = Get-Content -LiteralPath $LockFile -Raw | ConvertFrom-Json
    if ($lock.schemaVersion -ne 1 -or $lock.platform -ne 'windows-x64') { throw 'Unsupported tool lock.' }
    $selected = if ($Tool -eq 'all') { @('python', 'llvm', 'zlib', 'zstd', 'libxml2') } else { @($Tool) }
    foreach ($name in $selected) {
        $entry = $lock.$name
        $archivePath = Join-Path $downloadRoot $entry.archive
        $installPath = Join-Path $ToolsRoot $entry.directory
        Assert-Descendant $archivePath $downloadRoot
        Assert-Descendant $installPath $ToolsRoot
        if (-not (Test-Path -LiteralPath $archivePath) -and -not $VerifyDownloadsOnly) {
            $partialPath = $archivePath + '.partial-' + $runId
            Write-Evidence "Downloading locked $name $($entry.version) from $($entry.url)"
            Invoke-Checked 'C:/Windows/System32/curl.exe' @('--fail', '--location', '--silent', '--show-error', '--retry', '2', '--output', $partialPath, $entry.url)
            $null = Get-VerifiedHash $partialPath $entry.sha256
            Assert-Descendant $partialPath $downloadRoot
            Move-Item -LiteralPath $partialPath -Destination $archivePath
        }
        $actualHash = Get-VerifiedHash $archivePath $entry.sha256
        $report.tools += [ordered]@{ name = $name; version = $entry.version; url = $entry.url; archive = $archivePath; sha256 = $actualHash; install = $installPath }
        if ($VerifyDownloadsOnly) { continue }

        $stampPath = Join-Path $installPath '.hua-tool.json'
        if (Test-Path -LiteralPath $installPath) {
            if (-not (Test-Path -LiteralPath $stampPath)) { throw "Tool directory has no completed-install stamp: $installPath" }
            $stamp = Get-Content -LiteralPath $stampPath -Raw | ConvertFrom-Json
            if ($stamp.sha256 -ne $entry.sha256) { throw "Installed tool does not match the lock: $installPath" }
            Write-Evidence "Reusing verified installation: $installPath"
        } else {
            $stagingRoot = Join-Path $ToolsRoot ('.staging-' + $name + '-' + $runId)
            New-Item -ItemType Directory -Path $stagingRoot | Out-Null
            if ($entry.format -eq 'zip') {
                Expand-Archive -LiteralPath $archivePath -DestinationPath $stagingRoot
            } else {
                Push-Location $stagingRoot
                try { Invoke-Checked $CMake @('-E', 'tar', 'xf', $archivePath) } finally { Pop-Location }
            }
            $extracted = if ($entry.archiveRoot) { Join-Path $stagingRoot $entry.archiveRoot } else { $stagingRoot }
            Assert-Descendant $extracted $ToolsRoot
            Assert-Descendant $installPath $ToolsRoot
            if (-not (Test-Path -LiteralPath $extracted -PathType Container)) { throw "Expected archive root is missing: $extracted" }
            Move-Item -LiteralPath $extracted -Destination $installPath
            [ordered]@{ version = $entry.version; sha256 = $entry.sha256; source = $entry.url } | ConvertTo-Json | Set-Content -LiteralPath $stampPath -Encoding UTF8
        }

        if ($name -eq 'python') {
            Invoke-Checked (Join-Path $installPath 'python.exe') @('-X', 'utf8', '--version')
        } elseif ($name -eq 'llvm') {
            foreach ($required in @('include/clang/Tooling/Tooling.h', 'lib/cmake/llvm/LLVMConfig.cmake', 'lib/cmake/clang/ClangConfig.cmake', 'lib/clangTooling.lib')) {
                if (-not (Test-Path -LiteralPath (Join-Path $installPath $required) -PathType Leaf)) { throw "LLVM SDK member is missing: $required" }
            }
            Invoke-Checked (Join-Path $installPath 'bin/llvm-config.exe') @('--version')
            Invoke-Checked (Join-Path $installPath 'bin/clang-cl.exe') @('--version')
        }
    }
    if ($Tool -eq 'all' -and -not $VerifyDownloadsOnly) {
        if (-not $VsRoot) {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            $VsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if (-not $VsRoot) { throw 'Visual Studio C++ tools were not found; supply -VsRoot.' }
        }
        . (Join-Path $PSScriptRoot '../Tools/Meta/EnterMsvcEnvironment.ps1')
        Enter-HuaMsvcEnvironment $VsRoot $ToolsRoot
        $ninja = Join-Path $VsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
        $dependencyPrefix = Join-Path $ToolsRoot 'llvm-dependencies'
        $common = @('-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', '-DCMAKE_POLICY_DEFAULT_CMP0091=NEW', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5', "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_INSTALL_PREFIX=$dependencyPrefix")
        $dependencyConfigurations = @(
            @{ name = 'zlib'; source = 'src-zlib-1.3.2'; options = @('-DZLIB_BUILD_TESTING=OFF', '-DZLIB_BUILD_SHARED=OFF', '-DZLIB_BUILD_STATIC=ON', '-DZLIB_INSTALL=ON') },
            @{ name = 'zstd'; source = 'src-zstd-1.5.7/build/cmake'; options = @('-DZSTD_BUILD_PROGRAMS=OFF', '-DZSTD_BUILD_TESTS=OFF', '-DZSTD_BUILD_STATIC=ON', '-DZSTD_BUILD_SHARED=OFF') },
            @{ name = 'libxml2'; source = 'src-libxml2-2.9.12'; options = @('-DBUILD_SHARED_LIBS=OFF', '-DLIBXML2_WITH_PYTHON=OFF', '-DLIBXML2_WITH_ICONV=OFF', '-DLIBXML2_WITH_LZMA=OFF', '-DLIBXML2_WITH_ZLIB=OFF', '-DLIBXML2_WITH_PROGRAMS=OFF', '-DLIBXML2_WITH_TESTS=OFF', '-DLIBXML2_WITH_THREADS=ON') }
        )
        foreach ($dependency in $dependencyConfigurations) {
            $buildPath = Join-Path $ToolsRoot ('build-' + $dependency.name)
            $sourcePath = Join-Path $ToolsRoot $dependency.source
            Invoke-Checked $CMake (@('-S', $sourcePath, '-B', $buildPath) + $common + $dependency.options)
            Invoke-Checked $CMake @('--build', $buildPath, '--target', 'install', '--parallel', '2')
        }
        $report['dependencyPrefix'] = $dependencyPrefix
    }
    $report.succeeded = $true
    Write-Evidence 'Bootstrap completed.'
} catch {
    $report.error = $_.Exception.Message
    Write-Evidence ('FAILED: ' + $report.error)
    throw
} finally {
    $report['finishedUtc'] = [DateTime]::UtcNow.ToString('o')
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Host "Bootstrap evidence: $reportPath"
}
