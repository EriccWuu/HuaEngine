param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$BuildRoot,
    [Parameter(Mandatory=$true)][string]$EvidenceRoot
)
. (Join-Path $PSScriptRoot 'EcsValidationSupport.ps1')
$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$build = (Resolve-Path -LiteralPath $BuildRoot).Path
$evidence = [System.IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $evidence) { throw 'Use a new migration audit directory.' }
New-Item -ItemType Directory -Path $evidence | Out-Null
$sourceHash = Get-EcsSourceReceipt $source $evidence
$rg = (Get-Command rg -CommandType Application).Source
$roots = @('HuaEngine/src','Editor/src','CLI/src','ProjectHub/src','Tests','cmake','Tools/Meta','Tools/Reflection') | ForEach-Object { Join-Path $source $_ }
$arguments = @('--files') + $roots + @('-g','*.cpp','-g','*.h','-g','*.cmake','-g','*.in','-g','*.py','-g','CMakeLists.txt')
$receipt = Invoke-EcsLogged $rg $arguments (Join-Path $evidence 'source-enumeration.log') -Quiet
if ($receipt.exit_code -ne 0) { throw 'Source audit enumeration failed.' }
$files = @([System.IO.File]::ReadAllLines($receipt.log))
$files += @(Join-Path $source 'CMakeLists.txt'; Join-Path $source 'HuaEngine/CMakeLists.txt')
$legacy = New-Object 'System.Collections.Generic.List[object]'
$sourcePattern = '#\s*include\s*[<"](?:HuaEngine/)?ECS/(?:World|Entity|Query|Scheduler|System|Syetem|CommandBuffer|ComponentType|FrameContext)\.h|\bentt\b|\bENTT_INCLUDE_DIR\b|\bHE::World\b|\bHE::Entity\b|#\s*include\s*[<"][^"<>]*ComponentRegistry\.h'
foreach ($file in $files) {
    $lines = Get-Content -LiteralPath $file
    for ($line = 0; $line -lt $lines.Count; ++$line) {
        if ($lines[$line] -match $sourcePattern) { $legacy.Add([pscustomobject]@{kind='source'; path=$file; line=$line+1; text=$lines[$line]}) }
    }
}
$buildReceipt=Invoke-EcsLogged $rg @('--files','--hidden','--no-ignore',$build,'-g','*.vcxproj','-g','compile_commands.json') (Join-Path $evidence 'build-input-enumeration.log') -Quiet
if ($buildReceipt.exit_code -ne 0) { throw 'Build input audit enumeration failed.' }
$buildFiles=@([System.IO.File]::ReadAllLines($buildReceipt.log))
$projects=@($buildFiles | Where-Object { $_ -like '*.vcxproj' -and $_ -notmatch '[/\\]Dependencies[/\\]' })
$databases=@($buildFiles | Where-Object { $_ -like '*compile_commands.json' -and $_ -match '[/\\]meta[/\\]' })
if ($projects.Count -eq 0 -or $databases.Count -eq 0) { throw 'Actual compile projects or scan databases are absent.' }
$compilePattern = '[/\\]Dependencies[/\\]entt|[/\\]ECS[/\\](?:World|Entity|Query|Scheduler|System|Syetem|CommandBuffer|ComponentType|FrameContext)\.(?:h|cpp)|ComponentRegistry\.(?:cpp|h)|LegacyEcsContext\.cpp|LegacyComponentBridge\.h'
foreach ($file in @($projects)+@($databases)) {
    if ((Get-Content -Raw -LiteralPath $file) -match $compilePattern) { $legacy.Add([pscustomobject]@{kind='build_input'; path=$file; match=$Matches[0]}) }
}
$removed = @('World.h','World.cpp','Entity.h','Entity.cpp','Query.h','Scheduler.h','Scheduler.cpp','System.h','Syetem.h','CommandBuffer.h','ComponentType.h','FrameContext.h','ComponentRegistry.h','ComponentRegistry.cpp')
$removedPaths=@()
foreach ($name in $removed) {
    $path = Join-Path $source ('HuaEngine/src/HuaEngine/ECS/'+$name)
    $removedPaths+=$path
    if (Test-Path -LiteralPath $path) { $legacy.Add([pscustomobject]@{kind='retired_file_present'; path=$path}) }
}
$ctest=(Get-Command ctest -CommandType Application).Source
$testReceipt=Invoke-EcsLogged $ctest @('--test-dir',$build,'-C','Debug','--show-only=json-v1') (Join-Path $evidence 'test-registration.log') -Quiet
if ($testReceipt.exit_code -ne 0) { throw 'Cannot verify preserved regression registration.' }
$registration=Get-Content -LiteralPath $testReceipt.log -Raw | ConvertFrom-Json
$report = [ordered]@{schema_version=1; source_manifest_sha256=$sourceHash; commands=@($receipt,$buildReceipt,$testReceipt); source_files_checked=$files;
    compile_projects_checked=$projects; scan_databases_checked=$databases; legacy_dependencies=$legacy.ToArray();
    removed_paths=$removedPaths; preserved_regressions=@($registration.tests.name)}
Write-EcsJson (Join-Path $evidence 'migration-audit.json') $report
if ($legacy.Count -ne 0) { throw 'Legacy dependencies remain; inspect migration-audit.json.' }
Write-Output 'Source, compile projects and scan databases contain no retired ECS dependency.'
