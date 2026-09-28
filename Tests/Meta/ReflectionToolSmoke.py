"""Validate the production module manifests without changing their outputs."""
import argparse
import hashlib
import json
from pathlib import Path
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
    generated = (output / "GeneratedReflection.cpp").read_text(encoding="utf-8")
    require("srefl_class(" not in generated and "Serialize_HE__TransformComponent(" not in generated,
            "Expected descriptor generation without old component-level serializer wrappers")
    if config["module"] == "Core":
        types = {item["qualified_name"]: item for item in manifest["types"]}
        fields = {item["name"]: item for item in types["HE::TransformComponent"]["fields"]}
        require(fields["Position"]["runtime_type"] == "glm::vec3", "Clang aliases must preserve supported editable glm vector semantics")
    database = read(Path(config["compile_database"]) / "compile_commands.json")
    command = database[0]["command"]
    require("/Yu" not in command and "cmake_pch" not in command, "The actual scan compile command must not use PCH")
    require(("HE_DEBUG" if config["configuration"] == "Debug" else "HE_RELEASE") in command,
            "The scan command must carry the selected configuration macro")
config = read(args.core)
subprocess.run([config["python"], config["generator"], "validate-modules", "--meta-config", args.core, "--meta-config", args.rendering], check=True, timeout=90)
print("Production Clang reflection manifests and generated files are current")
