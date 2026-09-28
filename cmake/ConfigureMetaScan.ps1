param(
    [Parameter(Mandatory = $true)][string]$CMake,
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$Settings,
    [Parameter(Mandatory = $true)][string]$Configuration,
    [Parameter(Mandatory = $true)][string]$Ninja,
    [Parameter(Mandatory = $true)][string]$Compiler,
    [Parameter(Mandatory = $true)][string]$VisualStudio
)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop' -or $PSVersionTable.PSVersion.Major -ne 5) { throw 'System Windows PowerShell 5.1 is required.' }
New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
. (Join-Path $SourceRoot 'Tools/Meta/EnterMsvcEnvironment.ps1')
Enter-HuaMsvcEnvironment $VisualStudio $BuildRoot
$arguments = @('-S', (Join-Path $SourceRoot 'cmake/MetaScan'), '-B', $BuildRoot, '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_CXX_COMPILER=$Compiler",
    "-DHUA_META_SCAN_SETTINGS=$Settings")
& $CMake @arguments
exit $LASTEXITCODE
