param(
    [Parameter(Mandatory = $true)][string]$ToolsRoot,
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$EvidenceRoot,
    [string]$VsRoot = '',
    [string]$CMake = 'cmake'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($PSVersionTable.PSEdition -ne 'Desktop' -or $PSVersionTable.PSVersion.Major -ne 5) { throw 'System Windows PowerShell 5.1 is required.' }
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Force -Path $BuildRoot, $EvidenceRoot | Out-Null
$runId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
$steps = @()
function Invoke-ProbeStep([string]$Name, [string]$Executable, [string[]]$Arguments) {
    $log = Join-Path $EvidenceRoot ($Name + '-' + $runId + '.log')
    ($Executable + ' ' + ($Arguments -join ' ')) | Set-Content -LiteralPath $log -Encoding UTF8
    $savedErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Executable @Arguments 2>&1 | Tee-Object -FilePath $log -Append
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorAction
    $script:steps += [ordered]@{ name = $Name; executable = $Executable; arguments = $Arguments; exitCode = $exitCode; log = $log }
    if ($exitCode -ne 0) { throw "$Name failed with exit code $exitCode" }
}
try {
    Write-Host 'Resolving CMake and Visual Studio tools.'
    $CMake = (Get-Command $CMake -CommandType Application -ErrorAction Stop).Source
    if (-not $VsRoot) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $VsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $VsRoot) { throw 'Visual Studio C++ tools were not found; supply -VsRoot.' }
    }
    Write-Host "Entering the Visual Studio developer environment: $VsRoot"
    . (Join-Path $PSScriptRoot 'EnterMsvcEnvironment.ps1')
    Enter-HuaMsvcEnvironment $VsRoot $BuildRoot
    $llvmRoot = Join-Path $ToolsRoot 'llvm-23.1.2'
    $ninja = Join-Path $VsRoot 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
    Invoke-ProbeStep 'meta-configure' $CMake @('-S', $PSScriptRoot, '-B', $BuildRoot, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_MAKE_PROGRAM=$ninja", "-DLLVM_DIR=$llvmRoot/lib/cmake/llvm", "-DClang_DIR=$llvmRoot/lib/cmake/clang", "-DCMAKE_PREFIX_PATH=$ToolsRoot/llvm-dependencies", "-DZLIB_LIBRARY=$ToolsRoot/llvm-dependencies/lib/zs.lib", "-DZLIB_INCLUDE_DIR=$ToolsRoot/llvm-dependencies/include", "-DHUA_META_EVIDENCE_ROOT=$EvidenceRoot")
    Invoke-ProbeStep 'meta-build' $CMake @('--build', $BuildRoot, '--target', 'HuaMeta', '--parallel', '2', '--verbose')
    $ctest = Join-Path (Split-Path $CMake) 'ctest.exe'
    Invoke-ProbeStep 'meta-ctest' $ctest @('--test-dir', $BuildRoot, '-C', 'Release', '--output-on-failure', '--no-tests=error', '--timeout', '180', '--output-junit', (Join-Path $EvidenceRoot 'meta-junit.xml'))
} finally {
    [ordered]@{ schemaVersion = 1; utc = [DateTime]::UtcNow.ToString('o'); buildRoot = $BuildRoot; steps = $steps } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot ('meta-run-' + $runId + '.json')) -Encoding UTF8
}
