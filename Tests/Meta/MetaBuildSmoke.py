"""Build isolated real CMake modules and verify generation dependency behavior."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--meta-config", required=True)
parser.add_argument("--cmake", required=True)
parser.add_argument("--visual-studio", required=True)
parser.add_argument("--work-dir", required=True)
args = parser.parse_args()
config = json.loads(Path(args.meta_config).read_text(encoding="utf-8-sig"))
run_root = Path(args.work_dir) / datetime.now(timezone.utc).strftime("r%H%M%S%f")
root = run_root / "ascii"
root.mkdir(parents=True, exist_ok=True)
prior_logs = list(root.glob("command-*.log"))
if prior_logs:
    history = root / "history" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    history.mkdir(parents=True)
    for prior in prior_logs:
        shutil.copyfile(prior, history / prior.name)
source, build = root / "s", root / "b"
source.mkdir(exist_ok=True)
repo = Path(config["repository_root"])
configuration = config["configuration"]
commands = []
cases = []


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(arguments, success=True):
    name = f"command-{len(commands):02}.log"
    with (root / name).open("wb") as log:
        process = subprocess.Popen(arguments, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        try:
            status = process.wait(timeout=180)
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, check=False)
            process.wait(timeout=10)
            raise AssertionError(f"Build command timed out; see {root / name}")
    commands.append({"arguments": arguments, "exit": status, "log": str(root / name)})
    require((status == 0) == success, f"Unexpected exit {status}; see {root / name}")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def case(identity, start, **evidence):
    cases.append({"id": identity, "passed": True, "commands": commands[start:], "evidence": evidence})


def snapshot():
    paths = list((build / "generated" / configuration).rglob("*")) + list((build / "meta" / configuration).glob("*/manifest.json"))
    return {str(path.relative_to(build)): (sha(path), path.stat().st_mtime_ns) for path in paths if path.is_file()}


backend = source / "reflection_tool.py"
shutil.copyfile(config["generator"], backend)
nested = source / "Nested.h"
nested.write_text("#pragma once\nusing FieldType = int;\n", encoding="utf-8")
entry = source / "Entry.h"
body = '''#pragma once
#include "Nested.h"
#include "HuaEngine/Reflection/ReflectionMarkers.h"
HE_REFLECT_COMPONENT(Guid="b6000000000000000000000000000001", DisplayName="Build fixture", Category="Tests")
struct BuildComponent {
    HE_REFLECT_FIELD() FieldType Value = 0;
#if FIXTURE_EXTRA
    HE_REFLECT_FIELD() int Extra = 0;
#endif
};
'''
entry.write_text(body, encoding="utf-8")
(source / "Second.h").write_text(body.replace("BuildComponent", "SecondComponent").replace("0001", "0002"), encoding="utf-8")
(source / "consumer.cpp").write_text('#include <BuildA/GeneratedReflection.h>\nint GeneratedConsumer() { return 7; }\n', encoding="utf-8")
cmake_text = '''cmake_minimum_required(VERSION 3.24)
project(MetaBuildFixture LANGUAGES CXX)
set(HUAENGINE_SOURCE_ROOT "@REPO@" CACHE INTERNAL "Repository")
include("@REPO@/cmake/HuaMeta.cmake")
hua_meta_initialize()
add_library(MetaBuildFixture STATIC consumer.cpp)
target_compile_features(MetaBuildFixture PRIVATE cxx_std_20)
target_compile_definitions(MetaBuildFixture PRIVATE HUA_META_NO_LEGACY HE_PLATFORM_WINDOWS _CRT_SECURE_NO_WARNINGS
    FIXTURE_EXTRA=${FIXTURE_EXTRA} $<$<CONFIG:Debug>:HE_DEBUG> $<$<CONFIG:Release>:HE_RELEASE>)
target_compile_options(MetaBuildFixture PRIVATE /utf-8)
set_property(TARGET MetaBuildFixture PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_include_directories(MetaBuildFixture PRIVATE "@REPO@" "@REPO@/HuaEngine/src"
    "@REPO@/Dependencies/glm" "@REPO@/Dependencies/spdlog/include"
    "@REPO@/Dependencies/imgui" "@REPO@/Dependencies/glfw/include" "@REPO@/Dependencies/glad/include"
    "@REPO@/Dependencies/yaml-cpp/include")
hua_add_meta_module(NAME BuildA TARGET MetaBuildFixture ENTRY_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Entry.h")
set(modules BuildA)
if(FIXTURE_SECOND)
    hua_add_meta_module(NAME BuildB TARGET MetaBuildFixture ENTRY_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/Second.h")
    list(APPEND modules BuildB)
endif()
hua_add_meta_umbrella(MetaBuildFixture ${modules})
'''.replace("@REPO@", repo.as_posix())
(source / "CMakeLists.txt").write_text(cmake_text, encoding="utf-8")
configure = [args.cmake, "-S", str(source), "-B", str(build), "-G", "Visual Studio 18 2026", "-A", "x64",
             "-DHUAENGINE_META_EXECUTABLE=" + config["hua_meta"], "-DHUAENGINE_META_GENERATOR=" + str(backend),
             "-DHUAENGINE_PYTHON_EXECUTABLE=" + config["python"], "-DHUAENGINE_META_RESOURCE_DIR=" + config["resource_dir"],
             "-DHUAENGINE_META_VISUAL_STUDIO=" + args.visual_studio]
compile_command = [args.cmake, "--build", str(build), "--config", configuration, "--target", "MetaBuildFixture", "--parallel", "2"]
run(configure + ["-DFIXTURE_EXTRA=0", "-DFIXTURE_SECOND=OFF"])
require(not (build / "generated" / configuration / "BuildA" / "GeneratedReflection.h").exists(), "Cold build starts without a generated module header")
require(not (repo / "HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h").exists(), "Old source generated headers must not be available as fallback")
run(compile_command)
case("cold_build", 0, initially_missing_generated_header=True)
case("source_fallback_absent", 0, source_generated_header_absent=True)
before = snapshot()
start = len(commands)
run(compile_command)
require(snapshot() == before, "A no-op build must preserve generated bytes and timestamps")
case("incremental_noop", start, before=before, after=snapshot())

manifest_path = build / "meta" / configuration / "BuildA" / "manifest.json"
manifest_before = sha(manifest_path)
nested.write_text("#pragma once\nusing FieldType = long long;\n", encoding="utf-8")
start = len(commands)
run(compile_command)
require(sha(manifest_path) != manifest_before, "Transitive header changes must invalidate the real scan")
require("long long" in manifest_path.read_text(encoding="utf-8"), "The new AST field type must reach the manifest")
case("transitive_dependency", start, previous_manifest_sha=manifest_before, current_manifest_sha=sha(manifest_path))

start = len(commands)
run(configure + ["-DFIXTURE_EXTRA=1", "-DFIXTURE_SECOND=OFF"])
run(compile_command)
require('"Extra"' in manifest_path.read_text(encoding="utf-8"), "Consumer compile definitions must invalidate the scan database")
case("compile_definition_change", start, ast_extra_field=True)
start = len(commands)
run(configure + ["-DFIXTURE_EXTRA=1", "-DFIXTURE_SECOND=ON"])
run(compile_command)
require((build / "generated" / configuration / "BuildB" / "GeneratedReflection.h").is_file(), "Adding an explicit module must create its generated output")
case("module_list_change", start, modules=["BuildA", "BuildB"])

previous = sha(manifest_path)
backend.write_text(backend.read_text(encoding="utf-8") + "\n# Tool dependency mutation.\n", encoding="utf-8")
start = len(commands)
run(compile_command)
require(sha(manifest_path) != previous, "Backend content changes must invalidate provenance and the scan")
case("tool_generator_change", start, previous_manifest_sha=previous, current_manifest_sha=sha(manifest_path))

start = len(commands)
current_before = snapshot()
opposite = "Release" if configuration == "Debug" else "Debug"
opposite_command = list(compile_command)
opposite_command[opposite_command.index("--config") + 1] = opposite
run(opposite_command)
require(snapshot() == current_before, "Building another configuration must leave current artifacts unchanged")
for selected in (configuration, opposite):
    selected_root = build / "meta" / selected / "BuildA"
    selected_manifest = json.loads((selected_root / "manifest.json").read_text(encoding="utf-8"))
    database = json.loads((selected_root / "scan/compile_commands.json").read_text(encoding="utf-8"))
    require(selected_manifest["configuration"] == selected, "Manifest configuration must match its isolated directory")
    require(("HE_DEBUG" if selected == "Debug" else "HE_RELEASE") in database[0]["command"], "Each scan uses its actual configuration defines")
case("config_isolation", start, current_before=current_before, current_after=snapshot(), configurations=[configuration, opposite])

before_failure = snapshot()
entry.write_text(body + "\nvoid Invalid( ;\n", encoding="utf-8")
start = len(commands)
run(compile_command, False)
require(snapshot() == before_failure, "A rejected scan must block consumption while leaving old successful outputs intact")
case("invalid_declaration_blocks", start, generated_unchanged=True)
entry.write_text(body, encoding="utf-8")
run(compile_command)

start = len(commands)
backend_before = backend.read_bytes()
before_failure = snapshot()
backend.write_bytes(backend_before + b"\nnot valid Python !!!\n")
run(compile_command, False)
require(snapshot() == before_failure, "Broken generation tools must reject a build even when old artifacts exist")
case("stale_output_blocks", start, old_artifacts_present=True, generated_unchanged=True)
backend.write_bytes(backend_before)
run(compile_command)

missing = build / "generated" / configuration / "BuildA" / "GeneratedReflection.h"
missing.unlink()
entry.write_text(body + "\nvoid Invalid( ;\n", encoding="utf-8")
run(compile_command, False)
require(not missing.exists(), "Missing generated headers plus a failed scan must never fall back to source-tree artifacts")
entry.write_text(body, encoding="utf-8")
run(compile_command)
require(missing.exists(), "Repair must regenerate missing output and resume compilation")
start = len(commands)
run([config["python"], config["generator"], "scan"], False)
require("configure" in Path(commands[-1]["log"]).read_text(encoding="utf-8").lower(), "Missing meta-config must explain the required configure step")
case("missing_meta_config", start, configure_guidance=True)

# Keep failure builds outside Unicode paths: Windows CMD can corrupt MSBuild's
# generated batch labels on its nonzero branch under a non-UTF-8 code page.
special_root = run_root / "spaces & (空间)"
special_source, special_build = special_root / "s", special_root / "b"
shutil.copytree(source, special_source)
special_configure = list(configure)
special_configure[special_configure.index("-S") + 1] = str(special_source)
special_configure[special_configure.index("-B") + 1] = str(special_build)
special_configure = ["-DHUAENGINE_META_GENERATOR=" + str(special_source / "reflection_tool.py")
                     if value.startswith("-DHUAENGINE_META_GENERATOR=") else value for value in special_configure]
special_compile = list(compile_command)
special_compile[special_compile.index("--build") + 1] = str(special_build)
start = len(commands)
run(special_configure + ["-DFIXTURE_EXTRA=1", "-DFIXTURE_SECOND=ON"])
run(special_compile)
require((special_build / "generated" / configuration / "BuildA" / "GeneratedReflection.h").is_file(),
        "A real Unicode and shell-special path must complete generation and MSVC compilation")
case("special_path_build", start, path=str(special_root),
     failure_path_constraint="Negative MSBuild builds use ASCII paths because Windows CMD corrupts generated batch labels under the active non-UTF-8 code page.")
(root / "results.json").write_text(json.dumps({"passed": True, "configuration": configuration,
    "checks": ["fresh", "special-path", "no-op", "transitive-header", "compile-definition", "module-list", "tool", "stale-block", "missing-block", "repair"],
    "commands": commands}, indent=2), encoding="utf-8")
evidence_root = Path(os.environ.get("HUA_META_EVIDENCE_ROOT", str(Path(args.work_dir) / "evidence")))
evidence_root.mkdir(parents=True, exist_ok=True)
(evidence_root / "meta-build-cases.json").write_text(json.dumps({"schema_version": 1, "configuration": configuration,
    "cases": cases, "fixture_root": str(root)}, indent=2), encoding="utf-8")
print("MetaBuildSmoke passed: fresh, incremental, failure blocking and repair")
