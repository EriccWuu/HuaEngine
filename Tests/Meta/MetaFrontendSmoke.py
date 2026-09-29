"""Exercise real Clang scans and read-only backend validation in isolated files."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2), encoding="utf-8")


def run(arguments, expected=True):
    result = subprocess.run(arguments, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
    log = root / f"command-{len(operations):02}.log"
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    operations.append({"arguments": arguments, "exit": result.returncode, "log": str(log)})
    write(root / "commands.json", operations)
    require((result.returncode == 0) == expected,
            f"Unexpected exit {result.returncode}: {arguments}\n{result.stdout[-5000:]}\n{result.stderr[-5000:]}")
    return result


def fingerprint(directory):
    return {str(path.relative_to(directory)): (hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_mtime_ns)
            for path in directory.rglob("*") if path.is_file()}


parser = argparse.ArgumentParser()
parser.add_argument("--meta-config", required=True)
parser.add_argument("--work-dir", required=True)
args = parser.parse_args()
base = read(args.meta_config)
root = Path(args.work_dir)
root.mkdir(parents=True, exist_ok=True)
results = []
operations = []
prefix = '#include <string>\n#include "HuaEngine/Reflection/ReflectionMarkers.h"\n'


def fixture(name, body):
    directory = root / name
    directory.mkdir(parents=True, exist_ok=True)
    header = directory / "Entry.h"
    header.write_text(prefix + body, encoding="utf-8")
    tu = directory / "Scan.cpp"
    tu.write_text('#include "Entry.h"\n#include "Entry.h"\n', encoding="utf-8")
    header.write_text("#pragma once\n" + header.read_text(encoding="utf-8"), encoding="utf-8")
    entry = read(Path(base["compile_database"]) / "compile_commands.json")[0]
    old = entry["file"]
    command = entry["command"].replace(old.replace("/", "\\"), '"' + str(tu).replace("/", "\\") + '"')
    command = command.replace(old, '"' + tu.as_posix() + '"')
    require(str(tu) in command or tu.as_posix() in command, "Expected copied real compiler command to select the fixture TU")
    entry.update(file=tu.as_posix(), command=command)
    write(directory / "compile_commands.json", [entry])
    config = dict(base, module=name, compile_database=directory.as_posix(), translation_unit=tu.as_posix(),
                  entry_header=header.as_posix(), manifest=(directory / "manifest.json").as_posix(),
                  output_dir=(directory / "generated").as_posix())
    config_path = directory / "meta-config.json"
    write(config_path, config)
    return config, config_path, header


def scan(name, body, expected=True, code=None):
    config, config_path, header = fixture(name, body)
    run([base["python"], base["generator"], "scan", "--meta-config", str(config_path)], expected)
    manifest = read(config["manifest"] if expected else Path(config["manifest"]).with_suffix(".failed.json"))
    require(manifest["schema_version"] == 2, "Expected v2 manifest")
    if expected:
        require(not manifest["diagnostics"], "Expected clean successful scan")
        require(manifest["dependencies"] and manifest["compile_fingerprint"], "Expected compiler and transitive header evidence")
    else:
        errors = [item for item in manifest["diagnostics"] if item["severity"] == "error"]
        require(errors and all(item["source"] and item["line"] > 0 and item["column"] > 0 for item in errors),
                "Every rejected signature must carry an actual source, line and column")
        if code:
            require(any(item["code"] == code for item in errors), f"Expected diagnostic {code}: {errors}")
    results.append({"name": name, "expected_success": expected, "diagnostics": manifest["diagnostics"]})
    return config, config_path, header, manifest


component = 'HE_REFLECT_COMPONENT(Guid="d6000000000000000000000000000001", DisplayName="Component", Category="Tests") struct Component { HE_REFLECT_FIELD() int Value=0; };\n'
positive = component + '''
HE_REFLECT_COMPONENT(Guid="d6000000000000000000000000000002", DisplayName="Global disabled", Category="Tests", Tag=true) struct Disabled {};
namespace A {
HE_REFLECT_COMPONENT(Guid="d6000000000000000000000000000003", DisplayName="Nested component", Category="Tests", TypeName="A.Nested") struct Nested { HE_REFLECT_FIELD() std::string Text; };
using Alias = ::Component;
void Process(Alias& value);
}
'''
config, path, header, manifest = scan("Positive", positive)
require(len(manifest["types"]) == 3, "Repeated entry includes must be idempotent")
require(manifest["queries"] == [], "The scanner must not produce Query declarations")
types = {item["qualified_name"]: item for item in manifest["types"]}
require(types["A::Nested"]["type_name"] == "A.Nested", "A component must keep its stable TypeName")
require(types["A::Nested"]["fields"][0]["name"] == "Text", "Nested reflection fields must retain source metadata")

scan("DuplicateGuid", component + component.replace("struct Component", "struct Other"), False, "component.duplicate_guid")
scan("InvalidCpp", component + 'void Broken( ;\n', False)
scan("EmptyGuid", component.replace('Guid="d6000000000000000000000000000001"', 'Guid=""'), False)
scan("MalformedGuid", component.replace('Guid="d6000000000000000000000000000001"', 'Guid="not-a-guid"'), False)
scan("MissingGuid", component.replace('Guid="d6000000000000000000000000000001", ', ''), False)
scan("DetachedMarker", component + 'HE_REFLECT_COMPONENT(Guid="d6000000000000000000000000000004", DisplayName="Detached", Category="Tests")\n', False, "marker.unbound")

first, first_path, _, _ = scan("First", component)
second, second_path, _, _ = scan("Second", component.replace("struct Component", "struct Other"))
result = run([base["python"], base["generator"], "validate-modules", "--meta-config", str(first_path), "--meta-config", str(second_path)], False)
require("guid" in (result.stdout + result.stderr).lower(), "Cross-module failure must identify the Guid conflict")

run([base["python"], base["generator"], "generate", "--meta-config", str(path)])
generated_names = {item.name for item in Path(config["output_dir"]).iterdir()}
require(generated_names == {"GeneratedReflection.h", "GeneratedReflection.cpp", "generation-stamp.json"},
        "Generation must produce only component registration and reflection outputs")
before = fingerprint(Path(config["output_dir"]))
run([base["python"], base["generator"], "validate", "--meta-config", str(path)])
require(fingerprint(Path(config["output_dir"])) == before, "Successful validation must not write generated output")
header.write_text(header.read_text(encoding="utf-8") + "\n// Drift must invalidate input fingerprints.\n", encoding="utf-8")
manifest_before = Path(config["manifest"]).read_bytes()
run([base["python"], base["generator"], "validate", "--meta-config", str(path)], False)
require(fingerprint(Path(config["output_dir"])) == before and Path(config["manifest"]).read_bytes() == manifest_before,
        "Drift validation must not rewrite output or the original manifest")
run([base["python"], base["generator"], "generate", "--meta-config", str(path)], False)
require(fingerprint(Path(config["output_dir"])) == before, "Stale generation must leave old files intact and fail")

malformed = read(first["manifest"])
malformed["schema_version"] = 999
write(first["manifest"], malformed)
run([base["python"], base["generator"], "generate", "--meta-config", str(first_path)], False)
run([base["python"], base["generator"], "scan"], False)
write(root / "results.json", {"passed": True, "cases": results, "cross_module_duplicate_guid": True,
      "validate_read_only": True, "drift_blocks_generation": True, "unknown_schema_rejected": True})
print("MetaFrontendSmoke passed: real Clang positive/negative scans, identities and read-only validation")
