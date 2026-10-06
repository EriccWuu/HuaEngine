"""Validate the production module manifests without changing their outputs."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import runpy
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--core", required=True)
parser.add_argument("--rendering", required=True)
args = parser.parse_args()


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


for path in (args.core, args.rendering):
    config = read(path)
    output = Path(config["output_dir"])
    files = list(output.glob("*")) + [Path(config["manifest"])]
    before = {str(item): (hashlib.sha256(item.read_bytes()).hexdigest(), item.stat().st_mtime_ns) for item in files}
    subprocess.run([config["python"], config["generator"], "validate", "--meta-config", path], check=True, timeout=90)
    after = {str(item): (hashlib.sha256(item.read_bytes()).hexdigest(), item.stat().st_mtime_ns) for item in files}
    require(before == after, "Validation must leave generated output and manifest unchanged")
    manifest = read(config["manifest"])
    require(manifest["schema_version"] == 2 and manifest["module"] == config["module"], "Expected matching v2 module identity")
    require(manifest["generation_inputs"] and manifest["dependencies"], "Expected compiler, tool and dependency fingerprints")
    for name in ("GeneratedReflection.h", "GeneratedReflection.cpp", "GeneratedEcs.h", "GeneratedEcs.cpp"):
        require((output / name).is_file(), f"Expected generated module output: {name}")
    generated = (output / "GeneratedReflection.cpp").read_text(encoding="utf-8")
    require("srefl_class(" not in generated and "Serialize_HE__TransformComponent(" not in generated,
            "Expected descriptor generation without old component-level serializer wrappers")
    reflection_header = (output / "GeneratedReflection.h").read_text(encoding="utf-8")
    require("Ecs::" not in generated and "Ecs::" not in reflection_header and
            "/ECS/Runtime/" not in generated and "/ECS/Runtime/" not in reflection_header,
            "Generated reflection outputs must not depend on ECS runtime APIs")
    if config["module"] == "Core":
        types = {item["qualified_name"]: item for item in manifest["types"]}
        fields = {item["name"]: item for item in types["HE::TransformComponent"]["fields"]}
        require(fields["Position"]["runtime_type"] == "glm::vec3", "Clang aliases must preserve supported editable glm vector semantics")
    if config["module"] == "Rendering":
        types = {item["qualified_name"]: item for item in manifest["types"]}
        material_fields = {item["name"]: item for item in types["HE::Rendering::MaterialComponent"]["fields"]}
        mesh_fields = {item["name"]: item for item in types["HE::Rendering::MeshComponent"]["fields"]}
        require(mesh_fields["Mesh"]["value_type_guid"] == "4f745e86ab69460db41dac991f79c007" and
                material_fields["Material"]["value_type_guid"] == "4f745e86ab69460db41dac991f79c008" and
                material_fields["Overrides"]["value_type_guid"] == "4f745e86ab69460db41dac991f79c00b",
                "Clang must resolve reflected field value type Guids across included declarations")
    database = read(Path(config["compile_database"]) / "compile_commands.json")
    command = database[0]["command"]
    require("/Yu" not in command and "cmake_pch" not in command, "The actual scan compile command must not use PCH")
    require(("HE_DEBUG" if config["configuration"] == "Debug" else "HE_RELEASE") in command,
            "The scan command must carry the selected configuration macro")
config = read(args.core)
subprocess.run([config["python"], config["generator"], "validate-modules", "--meta-config", args.core, "--meta-config", args.rendering], check=True, timeout=90)

backend = runpy.run_path(config["generator"])
probe = {
    "schema_version": 2,
    "producer": {"name": "HuaMeta", "version": "23.1.2-p10"},
    "module": "Probe", "configuration": "Debug", "translation_unit": "Probe.cpp",
    "compile_fingerprint": "probe", "dependencies": [], "diagnostics": [], "queries": [],
    "types": [
        {"name": "Plain", "qualified_name": "Probe::Plain", "kind": "type",
         "source": "Tests/Meta/Fixtures/PlainReflection.h", "guid": "11000000000000000000000000000001",
         "reflection_scope": "marked", "flags": ["ScriptVisible"],
         "attributes": [{"name": "Editor.Group", "value": "Properties"}],
         "fields": [{"name": "Value", "type": "int", "runtime_type": "int",
                     "flags": ["ReadOnly"], "attributes": [{"name": "Editor.Unit", "value": "degrees"}]}]},
        {"name": "Component", "qualified_name": "Probe::Component", "kind": "component",
         "source": "Tests/Meta/Fixtures/ComponentModule.h", "guid": "11000000000000000000000000000002",
         "reflection_scope": "marked", "type_name": "ProbeComponent", "fields": []},
    ],
    "enums": [{"name": "Mode", "qualified_name": "Probe::Mode", "underlying_type": "int",
               "source": "Tests/Meta/Fixtures/FixtureEnums.h", "guid": "11000000000000000000000000000003",
               "reflection_scope": "full",
               "flags": ["ScriptVisible"], "attributes": [{"name": "Editor.Group", "value": "Modes"}],
               "values": [{"name": "Idle", "value": 0, "display_name": "Idle",
                           "flags": ["Default"], "attributes": [{"name": "Editor.Icon", "value": "pause"}]}]}],
}
backend["verify_manifest"](probe)
for collection in ("types", "enums"):
    missing_guid = copy.deepcopy(probe)
    missing_guid[collection][0].pop("guid")
    try:
        backend["verify_manifest"](missing_guid)
    except ValueError as error:
        require("Missing reflection Guid" in str(error), f"{collection} without a Guid must be diagnosed")
    else:
        raise AssertionError(f"{collection} without a Guid must fail validation")
    missing_scope = copy.deepcopy(probe)
    missing_scope[collection][0].pop("reflection_scope")
    try:
        backend["verify_manifest"](missing_scope)
    except ValueError as error:
        require("Missing reflection scope" in str(error), f"{collection} without a scope must be diagnosed")
    else:
        raise AssertionError(f"{collection} without a scope must fail validation")
generated = backend["render_files"](probe)
reflection = generated["GeneratedReflection.cpp"]
ecs_header = generated["GeneratedEcs.h"]
ecs_source = generated["GeneratedEcs.cpp"]
typed_field = copy.deepcopy(probe)
typed_field["types"][0]["fields"].append(
    {"name": "Nested", "type": "Probe::Component", "runtime_type": "Probe::Component",
     "value_type_guid": probe["types"][1]["guid"]}
)
backend["verify_manifest"](typed_field)
typed_reflection = backend["render_files"](typed_field)["GeneratedReflection.cpp"]
require("Refl::TypeGuid{0x1100000000000000ULL, 0x0000000000000002ULL}" in typed_reflection,
        "A reflected record field must generate its value type Guid")
invalid_field_guid = copy.deepcopy(typed_field)
invalid_field_guid["types"][0]["fields"][1]["value_type_guid"] = "not-a-guid"
try:
    backend["verify_manifest"](invalid_field_guid)
except ValueError as error:
    require("Invalid reflection Guid" in str(error), "A malformed field value type Guid must be diagnosed")
else:
    raise AssertionError("A malformed field value type Guid must fail validation")
require("inline constexpr Refl::TypeGuid TypeGuid_0" in generated["GeneratedReflection.h"] and
        "TypeGuid_0" in reflection and "Refl::TypeGuid{0x1100000000000000ULL, 0x0000000000000001ULL}" in generated["GeneratedReflection.h"],
        "Ordinary reflected types must generate stable GUIDs without ECS")
require("Refl::TypeGuid{0x1100000000000000ULL, 0x0000000000000003ULL}" in reflection,
        "Reflected enums must generate their stable GUIDs")
require("RuntimeType0Field0Flags" in reflection and "RuntimeType0Field0Attributes" in reflection and
        '"Editor.Unit", "degrees"' in reflection and "RuntimeEnum0Value0Attributes" in reflection,
        "Generated descriptors must retain extensible field and enum value metadata")
require("static constexpr Refl::TypeGuid Guid = Generated::Probe::TypeGuid_1" in ecs_header and
        "MakeTypeDescriptor<Probe::Component>(TypeGuid_1" in ecs_source and
        "0x1100000000000000ULL" not in ecs_header + ecs_source,
        "The ECS adapter must reuse the reflection type GUID without another literal")
duplicate = copy.deepcopy(probe)
duplicate["enums"][0]["guid"] = duplicate["types"][0]["guid"]
try:
    backend["verify_manifest"](duplicate)
except ValueError as error:
    require("Duplicate reflection Guid" in str(error), "A reflected type/enum GUID collision must be diagnosed")
else:
    raise AssertionError("A reflected type/enum GUID collision must fail validation")
invalid_attribute = copy.deepcopy(probe)
invalid_attribute["types"][0]["fields"][0]["attributes"] = [{"name": "", "value": "degrees"}]
try:
    backend["verify_manifest"](invalid_attribute)
except ValueError as error:
    require("Invalid reflection attributes" in str(error), "Malformed attributes must be diagnosed")
else:
    raise AssertionError("Malformed attributes must fail validation")
other = copy.deepcopy(probe)
other["module"] = "Other"
other["types"] = [{**other["types"][0], "qualified_name": "Other::Plain"}]
other["enums"] = []
try:
    backend["validate_module_set"]([probe, other])
except ValueError as error:
    require("duplicate Guid" in str(error), "Cross-module GUID reuse must be diagnosed")
else:
    raise AssertionError("Cross-module GUID reuse must fail validation")
consumer = copy.deepcopy(probe)
consumer["module"] = "Consumer"
consumer["types"] = [{"name": "Holder", "qualified_name": "Consumer::Holder", "kind": "type",
                      "source": "Tests/Meta/Fixtures/PlainReflection.h",
                      "guid": "11000000000000000000000000000004",
                      "reflection_scope": "marked",
                      "fields": [{"name": "Mode", "type": "Probe::Mode", "enum_type": "Probe::Mode",
                                  "value_type_guid": probe["enums"][0]["guid"],
                                  "enum_metadata": copy.deepcopy(probe["enums"][0])}]}]
consumer["enums"] = []
consumer["types"][0]["fields"][0]["enum_metadata"]["source"] = "Consumer/FixtureEnums.h"
backend["verify_manifest"](consumer)
backend["validate_module_set"]([probe, consumer])
require("Refl::TypeGuid{0x1100000000000000ULL, 0x0000000000000003ULL}" in
        backend["render_files"](consumer)["GeneratedReflection.cpp"],
        "A cross-module reflected enum field must generate its value type Guid")
conflicting_enum_guid = copy.deepcopy(consumer)
conflicting_enum_guid["types"][0]["fields"][0]["value_type_guid"] = probe["types"][1]["guid"]
try:
    backend["verify_manifest"](conflicting_enum_guid)
except ValueError as error:
    require("conflicting value type Guid" in str(error),
            "A reflected enum field must use the enum declaration's Guid")
else:
    raise AssertionError("A reflected enum field with an inconsistent Guid must fail validation")
for change in ("value", "flag", "attribute"):
    conflicting = copy.deepcopy(consumer)
    metadata = conflicting["types"][0]["fields"][0]["enum_metadata"]
    if change == "value":
        metadata["values"][0]["value"] = 1
    elif change == "flag":
        metadata["flags"].append("Hidden")
    else:
        metadata["values"][0]["attributes"][0]["value"] = "play"
    try:
        backend["validate_module_set"]([probe, conflicting])
    except ValueError as error:
        require("conflicting enum metadata" in str(error),
                f"Cross-module enum {change} mismatch must be diagnosed")
    else:
        raise AssertionError(f"Cross-module enum {change} mismatch must fail validation")
print("Production Clang reflection manifests and generated files are current")
