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


def fixture(name, body, reflection_only=False):
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
                  output_dir=(directory / "generated").as_posix(), reflection_only=reflection_only)
    config_path = directory / "meta-config.json"
    write(config_path, config)
    return config, config_path, header


def scan(name, body, expected=True, code=None, reflection_only=False):
    config, config_path, header = fixture(name, body, reflection_only)
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


component = 'struct [[sattr(guid="d6000000000000000000000000000001"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Component","Category=Tests"])]] Component { [[sattr()]] int Value=0; };\n'
positive = component + '''
struct [[sattr(guid="d6000000000000000000000000000002"; reflect=@disable; flags=["Component","Tag"]; attrs=["DisplayName=Global disabled","Category=Tests"])]] Disabled {};
struct [[sattr(guid="d6000000000000000000000000000006"; reflect=@marked; attrs=["DisplayName=Plain value","Category=Tests"])]] PlainValue {
    [[sattr()]] std::string Label;
    [[sattr()]] int Count = 0;
};
namespace A {
struct [[sattr(guid="d6000000000000000000000000000003"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Nested component","Category=Tests","TypeName=A.Nested"])]] Nested { [[sattr()]] std::string Text; };
using Alias = ::Component;
void Process(Alias& value);
}
'''
config, path, header, manifest = scan("Positive", positive)
require(len(manifest["types"]) == 4, "Repeated entry includes must be idempotent")
require(manifest["queries"] == [], "The scanner must not produce Query declarations")
types = {item["qualified_name"]: item for item in manifest["types"]}
require(types["A::Nested"]["type_name"] == "A.Nested", "A component must keep its stable TypeName")
require(types["A::Nested"]["fields"][0]["name"] == "Text", "Nested reflection fields must retain source metadata")
require(types["PlainValue"]["kind"] == "type" and
        types["PlainValue"]["guid"] == "d6000000000000000000000000000006" and
        [field["name"] for field in types["PlainValue"]["fields"]] == ["Label", "Count"],
        "Ordinary sattr types must retain stable identity and marked fields")

sattr_positive = '''
enum class [[sattr(guid="d6000000000000000000000000000101"; reflect=@marked; attrs=["DisplayName=Scan mode"])]] ScanMode {
    Hidden,
    Visible [[sattr(flags=["ScriptVisible"]; attrs=["DisplayName=Visible state"])]]
};
struct [[sattr(guid="d6000000000000000000000000000102"; reflect=@marked; attrs=["DisplayName=Ordinary","Category=Tests"])]] Ordinary {
    int Hidden = 0;
    [[sattr(flags=["ReadOnly"]; attrs=["DisplayName=Current value","Editor.Unit=m/s","Editor.Note=one,two=three"])]]
    int Value = 1;
    [[sattr()]] ScanMode Mode = ScanMode::Visible;
};
struct [[sattr(guid="d6000000000000000000000000000103"; reflect=@full; flags=["Component"]; attrs=["DisplayName=Full component","Category=Tests","TypeName=Stable.Full"])]] FullComponent {
    int First = 0;
    [[sattr(flags=["ReadOnly"])]] float Second = 0;
};
struct [[sattr(guid="d6000000000000000000000000000104"; reflect=@disable; flags=["Component","Tag"]; attrs=["DisplayName=Disabled tag","Category=Tests"])]] DisabledTag {
};
struct [[sattr(guid="d6000000000000000000000000000109"; reflect=@marked)]] BareType {
    [[sattr()]] int Value = 0;
};
'''
_, _, _, sattr_manifest = scan("SattrPositive", sattr_positive, reflection_only=True)
sattr_types = {item["qualified_name"]: item for item in sattr_manifest["types"]}
ordinary = sattr_types["Ordinary"]
require(ordinary["guid"] == "d6000000000000000000000000000102" and ordinary["kind"] == "type" and
        ordinary["reflection_scope"] == "marked" and [field["name"] for field in ordinary["fields"]] == ["Value", "Mode"],
        "sattr ordinary types must carry stable identity and only marked fields")
value = ordinary["fields"][0]
require(value["read_only"] and value["flags"] == ["ReadOnly"] and
        {item["name"]: item["value"] for item in value["attributes"]}["Editor.Note"] == "one,two=three",
        "sattr field flags and extensible attributes must survive parsing")
require(ordinary["fields"][1]["enum_metadata"]["guid"] == "d6000000000000000000000000000101",
        "sattr enum identity must be available to fields using the enum")
require(sattr_types["FullComponent"]["kind"] == "component" and
        sattr_types["FullComponent"]["type_name"] == "Stable.Full" and
        [field["name"] for field in sattr_types["FullComponent"]["fields"]] == ["First", "Second"],
        "Component flag and full scope must determine ECS classification and complete field coverage")
require(sattr_types["DisabledTag"]["tag"] and not sattr_types["DisabledTag"]["fields"],
        "Tag flag and disabled reflection must preserve ECS identity without reflected fields")
require(sattr_types["BareType"]["display_name"] == "BareType" and sattr_types["BareType"]["category"] == "" and
        sattr_types["BareType"]["guid"] == "d6000000000000000000000000000109",
        "Ordinary reflected types need only a Guid and reflection scope")
sattr_enum = sattr_manifest["enums"][0]
require(sattr_enum["guid"] == "d6000000000000000000000000000101" and
        sattr_enum["reflection_scope"] == "marked" and
        [item["name"] for item in sattr_enum["values"]] == ["Visible"] and
        sattr_enum["values"][0]["display_name"] == "Visible state",
        "sattr enum value metadata must obey marked scope")

scan("SattrMissingGuid", 'struct [[sattr(reflect=@marked; attrs=["DisplayName=Missing","Category=Tests"])]] Missing {};\n',
     False, "type.invalid_guid")
scan("SattrMalformedGuid", 'struct [[sattr(guid="wrong"; reflect=@marked; attrs=["DisplayName=Wrong","Category=Tests"])]] Wrong {};\n',
     False, "type.invalid_guid")
scan("SattrEnumMissingGuid", 'enum class [[sattr(reflect=@full)]] MissingEnum { One };\n',
     False, "enum.invalid_guid")
scan("SattrDuplicateGuid", '''
struct [[sattr(guid="d6000000000000000000000000000105"; reflect=@marked; attrs=["DisplayName=First","Category=Tests"])]] First {};
enum class [[sattr(guid="d6000000000000000000000000000105"; reflect=@full)]] Second { One };
''', False, "enum.duplicate_guid")
scan("SattrMalformedAttrs", 'struct [[sattr(guid="d6000000000000000000000000000106"; reflect=@marked; attrs=[bad])]] Bad {};\n',
     False, "sattr.invalid")
scan("SattrUnknownScope", 'struct [[sattr(guid="d6000000000000000000000000000107"; reflect=@all)]] Bad {};\n',
     False, "sattr.invalid")
scan("SattrMissingScope", 'struct [[sattr(guid="d6000000000000000000000000000108"; attrs=["DisplayName=Missing","Category=Tests"])]] Missing {};\n',
     False, "sattr.missing_reflect")
scan("SattrFullPrivate", '''
class [[sattr(guid="d6000000000000000000000000000110"; reflect=@full)]] PrivateFull {
    int Hidden = 0;
public:
    int Visible = 1;
};
''', False, "field.inaccessible")
scan("SattrMarkedProtected", '''
struct [[sattr(guid="d6000000000000000000000000000111"; reflect=@marked)]] ProtectedMarked {
protected:
    [[sattr()]] int Hidden = 0;
};
''', False, "field.inaccessible")
scan("SattrBitField", '''
struct [[sattr(guid="d6000000000000000000000000000112"; reflect=@full)]] Bits {
    unsigned Flags : 2;
};
''', False, "field.bit_field")
scan("SattrNonEmptyTag", '''
struct [[sattr(guid="d6000000000000000000000000000113"; reflect=@disable; flags=["Component","Tag"]; attrs=["DisplayName=Invalid tag","Category=Tests"])]] InvalidTag {
    int Hidden = 0;
};
''', False, "component.nonempty_tag")

external_config, external_path, external_header = fixture("SattrExternalEnumMissingGuid", '''
#include "External.h"
struct [[sattr(guid="d6000000000000000000000000000114"; reflect=@marked)]] EnumOwner {
    [[sattr()]] ExternalMode Mode = ExternalMode::One;
};
''', reflection_only=True)
(external_header.parent / "External.h").write_text('''#pragma once
enum class [[sattr(reflect=@full)]] ExternalMode { One };
''', encoding="utf-8")
run([base["python"], base["generator"], "scan", "--meta-config", str(external_path)], False)
external_errors = read(Path(external_config["manifest"]).with_suffix(".failed.json"))["diagnostics"]
require(any(item["code"] == "enum.invalid_guid" and item["source"].endswith("External.h") and
            item["line"] == 2 and item["column"] > 0 for item in external_errors),
        "Referenced enums outside the entry header must validate stable identity at their own source")

duplicate_config, duplicate_path, duplicate_header = fixture("SattrExternalEnumDuplicateGuid", '''
#include "External.h"
struct [[sattr(guid="d6000000000000000000000000000115"; reflect=@marked)]] EnumOwner {
    [[sattr()]] ExternalMode Mode = ExternalMode::One;
};
''', reflection_only=True)
(duplicate_header.parent / "External.h").write_text('''#pragma once
enum class [[sattr(guid="d6000000000000000000000000000115"; reflect=@full)]] ExternalMode { One };
''', encoding="utf-8")
run([base["python"], base["generator"], "scan", "--meta-config", str(duplicate_path)], False)
duplicate_errors = read(Path(duplicate_config["manifest"]).with_suffix(".failed.json"))["diagnostics"]
require(any(item["code"] == "enum.duplicate_guid" and item["source"].endswith("External.h") and
            item["line"] == 2 for item in duplicate_errors),
        "Referenced external enum GUIDs must conflict with local reflected type GUIDs")

plain_config, plain_path, _, plain_manifest = scan("PlainReflectionOnly", '''
struct [[sattr(guid="d6000000000000000000000000000007"; reflect=@marked; attrs=["DisplayName=Standalone","Category=Tests"])]] Standalone {
    [[sattr()]] std::string Label;
};
''', reflection_only=True)
require(len(plain_manifest["types"]) == 1 and plain_manifest["types"][0]["kind"] == "type" and
        plain_manifest["types"][0]["guid"] == "d6000000000000000000000000000007" and
        [field["name"] for field in plain_manifest["types"][0]["fields"]] == ["Label"],
        "A reflected ordinary type must scan with only the reflection marker header")
run([base["python"], base["generator"], "generate", "--meta-config", str(plain_path)])
require({item.name for item in Path(plain_config["output_dir"]).iterdir()} ==
        {"GeneratedReflection.h", "GeneratedReflection.cpp", "generation-stamp.json"},
        "A reflection-only module must not publish ECS adapter files")
run([base["python"], base["generator"], "validate", "--meta-config", str(plain_path)])

unmarked_enum = '''enum class UnmarkedMode { Idle, Active };
struct [[sattr(guid="d6000000000000000000000000000005"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Enum consumer","Category=Tests"])]] EnumConsumer {
    [[sattr()]] UnmarkedMode Mode = UnmarkedMode::Idle;
};
'''
unmarked_config, unmarked_path, unmarked_header = fixture("UnmarkedEnum", unmarked_enum)
unmarked_result = run([base["python"], base["generator"], "scan", "--meta-config", str(unmarked_path)], False)
unmarked_line = next(index for index, text in enumerate(unmarked_header.read_text(encoding="utf-8").splitlines(), 1)
                     if "UnmarkedMode Mode" in text)
unmarked_diagnostic = unmarked_result.stdout + unmarked_result.stderr
require(f"Entry.h:{unmarked_line}:" in unmarked_diagnostic and "reflection metadata" in unmarked_diagnostic,
        "An unmarked enum field must report its source location and the required annotation")
require(not Path(unmarked_config["manifest"]).exists(), "An invalid enum field must not publish a manifest")

missing_enum_manifest = read(config["manifest"])
missing_enum_field = missing_enum_manifest["types"][0]["fields"][0]
missing_enum_field["enum_type"] = "MissingMode"
missing_enum_field["enum_metadata"] = {}
missing_enum_path = Path(config["manifest"]).with_name("missing-enum-manifest.json")
write(missing_enum_path, missing_enum_manifest)
missing_enum_output = root / "missing-enum-generated"
missing_enum_before = fingerprint(missing_enum_output)
missing_enum_result = run([base["python"], base["generator"], "generate", "--meta-config", str(path),
                           "--manifest", str(missing_enum_path), "--out-dir", str(missing_enum_output)], False)
missing_enum_diagnostic = missing_enum_result.stdout + missing_enum_result.stderr
require(f"Entry.h:{missing_enum_field['line']}:" in missing_enum_diagnostic and
        "reflection metadata" in missing_enum_diagnostic,
        "Generation must reject unresolved enum metadata at the reflected field location")
require(fingerprint(missing_enum_output) == missing_enum_before,
        "Invalid enum metadata must not publish or change generated code")

scan("DuplicateGuid", component + component.replace(" Component {", " Other {"), False, "component.duplicate_guid")
scan("InvalidCpp", component + 'void Broken( ;\n', False)
scan("EmptyGuid", component.replace('guid="d6000000000000000000000000000001"', 'guid=""'),
     False, "component.invalid_guid")
scan("MalformedGuid", component.replace('guid="d6000000000000000000000000000001"', 'guid="not-a-guid"'),
     False, "component.invalid_guid")
scan("MissingGuid", component.replace('guid="d6000000000000000000000000000001"; ', ''),
     False, "component.invalid_guid")
scan("DuplicateSattr", '''
struct [[sattr(guid="d6000000000000000000000000000008"; reflect=@marked)]]
       [[sattr(guid="d6000000000000000000000000000008"; reflect=@marked)]] Duplicated {};
''', False, "sattr.duplicate")
scan("UnboundSattr", component + '[[sattr(guid="d6000000000000000000000000000004"; reflect=@marked)]];\n',
     False, "sattr.unbound")

first, first_path, _, _ = scan("First", component)
second, second_path, _, _ = scan("Second", component.replace(" Component {", " Other {"))
result = run([base["python"], base["generator"], "validate-modules", "--meta-config", str(first_path), "--meta-config", str(second_path)], False)
require("guid" in (result.stdout + result.stderr).lower(), "Cross-module failure must identify the Guid conflict")

run([base["python"], base["generator"], "generate", "--meta-config", str(path)])
generated_names = {item.name for item in Path(config["output_dir"]).iterdir()}
require(generated_names == {"GeneratedReflection.h", "GeneratedReflection.cpp",
                            "GeneratedEcs.h", "GeneratedEcs.cpp", "generation-stamp.json"},
        "Generation must produce reflection and ECS adapter outputs")
generated_reflection = (Path(config["output_dir"]) / "GeneratedReflection.cpp").read_text(encoding="utf-8")
require("sizeof(PlainValue)" in generated_reflection and "GetConst_PlainValue_Label" in generated_reflection and
        "Serialize_PlainValue_Label" in generated_reflection and "Deserialize_PlainValue_Label" in generated_reflection,
        "A plain reflected type must receive complete object and field metadata")
require("Ecs::" not in generated_reflection and all(
        "PlainValue" not in (Path(config["output_dir"]) / name).read_text(encoding="utf-8")
        for name in ("GeneratedEcs.h", "GeneratedEcs.cpp")),
        "The pure reflection source must not reference ECS, and plain types must not register as components")
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
      "unmarked_enum_diagnostic": True,
      "validate_read_only": True, "drift_blocks_generation": True, "unknown_schema_rejected": True})
print("MetaFrontendSmoke passed: real Clang positive/negative scans, identities and read-only validation")
