#!/usr/bin/env python3
"""Generate ordinary C++ from the configured HuaMeta Clang manifest."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any, Dict, List, Optional

SCHEMA_VERSION = 2
CONFIG_VERSION = 1


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def cpp_bool(value: bool) -> str:
    return "true" if value else "false"


def normalized_guid(value: Any) -> Optional[str]:
    if value is None or value == "":
        return None
    if not isinstance(value, str):
        raise ValueError(f"Invalid reflection Guid: {value!r}")
    guid = value.replace("-", "").lower()
    if not re.fullmatch(r"[0-9a-f]{32}", guid) or int(guid, 16) == 0:
        raise ValueError(f"Invalid reflection Guid: {value!r}")
    return guid


def cpp_guid(value: Any) -> str:
    guid = normalized_guid(value)
    if not guid:
        raise ValueError("Generated reflected declarations require a Guid")
    return f"Refl::TypeGuid{{0x{guid[:16]}ULL, 0x{guid[16:]}ULL}}"


def cpp_identifier(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", value)
    return "_" + value if not value or value[0].isdigit() else value


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_if_changed(path: Path, text: str) -> bool:
    data = text.encode("utf-8")
    if path.is_file() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    return True


def write_json(path: Path, value: Any) -> bool:
    return write_if_changed(path, json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def manifest_fingerprint(manifest: Dict[str, Any]) -> str:
    return hashlib.sha256(json.dumps(manifest, sort_keys=True).encode("utf-8")).hexdigest()


def read_json(path: Path) -> Dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"Expected a JSON object: {path}")
    return value


def load_config(path: Optional[str]) -> Dict[str, Any]:
    if not path:
        raise ValueError("Missing --meta-config. Run CMake configure first and pass the selected module/configuration meta-config.json.")
    config_path = Path(path).resolve()
    if not config_path.is_file():
        raise ValueError(f"Meta configuration does not exist: {config_path}. Run CMake configure first.")
    config = read_json(config_path)
    if config.get("config_version") != CONFIG_VERSION:
        raise ValueError(f"Unsupported config_version: {config.get('config_version')!r}; run CMake configure again.")
    for name in ("module", "configuration"):
        if not isinstance(config.get(name), str) or not config[name]:
            raise ValueError(f"Meta configuration requires {name}")
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", config["module"]):
        raise ValueError("The module name must be a C++ namespace identifier")
    for name in ("repository_root", "compile_database", "translation_unit", "entry_header", "hua_meta", "resource_dir", "python", "generator", "manifest", "output_dir"):
        value = config.get(name)
        if not isinstance(value, str) or not Path(value).is_absolute():
            raise ValueError(f"Meta configuration requires an absolute {name} path")
        config[name] = str(Path(value).resolve())
    config["_path"] = str(config_path)
    if not (Path(config["compile_database"]) / "compile_commands.json").is_file():
        raise ValueError("Missing compile_commands.json; run CMake configure for the selected meta configuration first.")
    return config


def verify_manifest(manifest: Dict[str, Any], config: Optional[Dict[str, Any]] = None, verify_dependencies: bool = True) -> None:
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(f"Unsupported manifest schema_version: {manifest.get('schema_version')!r}; Clang manifest v2 is required.")
    producer = manifest.get("producer", {})
    if producer.get("name") != "HuaMeta" or producer.get("version") != "23.1.2-p10":
        raise ValueError("Manifest producer must be HuaMeta 23.1.2-p10")
    for name in ("module", "configuration", "translation_unit", "compile_fingerprint"):
        if not isinstance(manifest.get(name), str) or not manifest[name]:
            raise ValueError(f"Manifest requires {name}")
    for name in ("types", "enums", "queries", "dependencies", "diagnostics"):
        if not isinstance(manifest.get(name), list):
            raise ValueError(f"Manifest requires the {name} array")
    errors = [item for item in manifest["diagnostics"] if item.get("severity") == "error"]
    if errors:
        raise ValueError("Manifest contains error diagnostics; generation refused: " + json.dumps(errors, ensure_ascii=False))
    if config:
        for name in ("module", "configuration"):
            if manifest[name] != config[name]:
                raise ValueError(f"Manifest {name} does not match the selected meta configuration")
        root = Path(config["repository_root"])
        tu = Path(manifest["translation_unit"])
        if not tu.is_absolute():
            tu = root / tu
        if tu.resolve() != Path(config["translation_unit"]):
            raise ValueError("Manifest translation_unit does not match the selected meta configuration")
        if verify_dependencies:
            for dependency in manifest["dependencies"]:
                path = Path(dependency["path"])
                if not path.is_absolute():
                    path = root / path
                if not path.is_file() or sha256(path) != dependency.get("sha256", "").lower():
                    raise ValueError(f"Stale manifest dependency: {path}; run reflection scan with this meta configuration again.")
    seen_names: set[str] = set()
    seen_guids: Dict[str, str] = {}
    guids_by_name: Dict[str, str] = {}
    registered_names: set[str] = set()
    declared_enums = {item["qualified_name"]: item for item in manifest["enums"]}

    def check_metadata(item: Dict[str, Any], label: str, require_scope: bool = False) -> None:
        flags = item.get("flags", [])
        attributes = item.get("attributes", [])
        if not isinstance(flags, list) or any(not isinstance(flag, str) or not flag for flag in flags):
            raise ValueError(f"Invalid reflection flags for {label}")
        if not isinstance(attributes, list) or any(
            not isinstance(attribute, dict) or
            not isinstance(attribute.get("name"), str) or not attribute["name"] or
            not isinstance(attribute.get("value"), str)
            for attribute in attributes
        ):
            raise ValueError(f"Invalid reflection attributes for {label}")
        scope = item.get("reflection_scope")
        if require_scope and scope is None:
            raise ValueError(f"Missing reflection scope for {label}")
        if scope is not None and scope not in {"marked", "full", "disable"}:
            raise ValueError(f"Invalid reflection scope for {label}")

    def check_guid(item: Dict[str, Any], name: str) -> None:
        guid = normalized_guid(item.get("guid"))
        if not guid:
            raise ValueError(f"Missing reflection Guid for {name}")
        if guid:
            previous = seen_guids.setdefault(guid, name)
            if previous != name:
                raise ValueError(f"Duplicate reflection Guid for {previous} and {name}")
            previous_guid = guids_by_name.setdefault(name, guid)
            if previous_guid != guid:
                raise ValueError(f"Conflicting reflection Guid for {name}")

    for item in manifest["types"] + manifest["enums"]:
        name = item.get("qualified_name", "")
        if not name or name in seen_names:
            raise ValueError(f"Duplicate or empty reflected qualified name: {name}")
        seen_names.add(name)
        check_metadata(item, name, require_scope=True)
        check_guid(item, name)
        if item.get("kind") == "component":
            stable_name = item.get("type_name", item.get("name", ""))
            if not stable_name or stable_name in registered_names:
                raise ValueError(f"Invalid or duplicate component TypeName: {stable_name}")
            registered_names.add(stable_name)
            if item.get("tag", False) and item.get("fields"):
                raise ValueError(f"Tag component cannot declare stored fields: {name}")
        for field in item.get("fields", []):
            check_metadata(field, f"{name}.{field.get('name', '<unnamed>')}")
            value_type_guid = field.get("value_type_guid")
            if value_type_guid is not None:
                value_type_guid = normalized_guid(value_type_guid)
            enum_type = field.get("enum_type", "")
            if not enum_type:
                continue
            metadata = field.get("enum_metadata")
            expected_enum = metadata if isinstance(metadata, dict) and metadata.get("qualified_name") == enum_type else declared_enums.get(enum_type)
            if expected_enum and value_type_guid and value_type_guid != normalized_guid(expected_enum.get("guid")):
                raise ValueError(f"Reflected enum field {name}.{field.get('name', '<unnamed>')} has a conflicting value type Guid")
            if isinstance(metadata, dict) and metadata.get("qualified_name") == enum_type and isinstance(metadata.get("values"), list):
                check_metadata(metadata, enum_type, require_scope=True)
                check_guid(metadata, enum_type)
                for value in metadata["values"]:
                    check_metadata(value, f"{enum_type}.{value.get('name', '<unnamed>')}")
                continue
            if enum_type in declared_enums:
                continue
            source = field.get("source", item.get("source", "<unknown>"))
            line = field.get("line", item.get("line", 0))
            column = field.get("column", 0)
            raise ValueError(
                f"{source}:{line}:{column}: reflected field {name}.{field.get('name', '<unnamed>')} "
                f"uses enum {enum_type} without reflection metadata"
            )
        for value in item.get("values", []):
            check_metadata(value, f"{name}.{value.get('name', '<unnamed>')}")
    if manifest["queries"]:
        raise ValueError("Manifest contains Query declarations; only component and reflection metadata is supported")


def load_manifest(path: Path, config: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    manifest = read_json(path)
    verify_manifest(manifest, config)
    if config and manifest.get("generation_inputs") != generation_inputs(config):
        raise ValueError("Missing or stale manifest compilation/tool provenance; run reflection scan with this meta configuration again.")
    return manifest


def generated_include_for_source(source: str) -> str:
    source = source.replace("\\", "/")
    prefix = "HuaEngine/src/"
    return source[len(prefix):] if source.startswith(prefix) else source


def is_editable_runtime_field_type(field_type: str, is_enum: bool) -> bool:
    return is_enum or field_type in {"bool", "int", "int8_t", "int16_t", "int32_t", "int64_t", "long", "long long", "unsigned int", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "unsigned long", "unsigned long long", "float", "double", "std::string", "glm::vec2", "glm::vec3", "glm::vec4"}


def render_reflection(manifest: Dict[str, Any]) -> Dict[str, str]:
    module = manifest["module"]
    manifest_types = manifest["types"]
    manifest_enums = manifest["enums"]
    enum_index_by_qualified_name = {item["qualified_name"]: index for index, item in enumerate(manifest_enums)}
    referenced_enum_metadata: Dict[str, Dict[str, Any]] = {}
    for item in manifest_types:
        for field in item.get("fields", []):
            enum_type = field.get("enum_type", "")
            metadata = field.get("enum_metadata", {})
            if not enum_type or not metadata or enum_type in enum_index_by_qualified_name:
                continue
            if metadata.get("qualified_name") != enum_type:
                raise ValueError(f"Referenced enum metadata does not match {enum_type}")
            previous = referenced_enum_metadata.setdefault(enum_type, metadata)
            if previous != metadata:
                raise ValueError(f"Conflicting referenced enum metadata for {enum_type}")
    referenced_enums = [referenced_enum_metadata[name] for name in sorted(referenced_enum_metadata)]
    referenced_enum_index = {item["qualified_name"]: index for index, item in enumerate(referenced_enums)}

    def enum_pointer_for_field(field: Dict[str, Any]) -> str:
        name = field.get("enum_type", "")
        if not name:
            return "nullptr"
        index = enum_index_by_qualified_name.get(name)
        if index is not None:
            return f"&ModuleRuntimeEnums[{index}]"
        index = referenced_enum_index.get(name)
        return f"&ReferencedRuntimeEnumData[{index}]" if index is not None else "nullptr"

    header_lines = ["#pragma once", "", '#include "HuaEngine/Reflection/ReflectionRegistry.h"', "#include <optional>", "", f"namespace HE::Generated::{module} {{"]
    for type_index, reflected_type in enumerate(manifest_types):
        header_lines.append(f"inline constexpr Refl::TypeGuid TypeGuid_{type_index} = {cpp_guid(reflected_type.get('guid'))};")
    header_lines.extend(["std::span<const Refl::RuntimeTypeDescriptor> RuntimeTypes();", "std::span<const Refl::RuntimeEnumDescriptor> RuntimeEnums();", "std::span<const Refl::RuntimeEnumDescriptor> ReferencedRuntimeEnums();", "std::optional<Refl::RegistrationError> RegisterReflection(Refl::Registry& registry);", "}", ""])
    include_paths = sorted({generated_include_for_source(item["source"]) for item in manifest_types + manifest_enums})
    lines = ['#include "GeneratedReflection.h"', '#include "HuaEngine/Serialization/SerializationCore.h"', '#include "HuaEngine/Serialization/GLMSerializer.h"', "#include <stdexcept>", "#include <type_traits>", "#include <utility>"]
    lines.extend(f'#include "{path}"' for path in include_paths)
    lines.extend(["", f"namespace HE::Generated::{module} {{", "", "template<class T> static void* MutableField(T& value) {", "    if constexpr (std::is_const_v<T>) return nullptr;", "    else return &value;", "}", "template<class T> static bool DeserializeField(Serialization::SerializationBackend& backend, const std::string& name, T& value) {", "    if constexpr (!std::is_const_v<T> && std::is_copy_constructible_v<T> && std::is_move_assignable_v<T>) {", "        T candidate(value);", "        if (!Serialization::DeserializeValue(backend, name, candidate)) return false;", "        value = std::move(candidate);", "        return true;", "    } else if constexpr (!std::is_const_v<T> && std::is_default_constructible_v<T> && std::is_move_assignable_v<T>) {", "        T candidate{};", "        if (!Serialization::DeserializeValue(backend, name, candidate)) return false;", "        value = std::move(candidate);", "        return true;", "    } else return false;", "}", ""])
    lines.extend(["template<class T> static void* DefaultValue() {", "    if constexpr (std::is_default_constructible_v<T>) return new T();", "    else return nullptr;", "}", "template<class T> static void* CopyValue(const void* source) {", "    if constexpr (std::is_copy_constructible_v<T>) return new T(*static_cast<const T*>(source));", "    else return nullptr;", "}", "template<class T> static bool AssignEnum(T& target, int64_t value) {", "    if constexpr (std::is_assignable_v<T&, std::remove_const_t<T>>) {", "        target = static_cast<std::remove_const_t<T>>(value);", "        return true;", "    } else return false;", "}", ""])

    def metadata_spans(item: Dict[str, Any], symbol: str) -> tuple[str, str]:
        flag_span = f"std::span<const std::string_view>{{{symbol}Flags}}" if item.get("flags") else "{}"
        attribute_span = f"std::span<const Refl::RuntimeAttribute>{{{symbol}Attributes}}" if item.get("attributes") else "{}"
        return flag_span, attribute_span

    def emit_metadata(item: Dict[str, Any], symbol: str) -> None:
        flags = item.get("flags", [])
        attributes = item.get("attributes", [])
        if flags:
            lines.append(f"static constexpr std::string_view {symbol}Flags[] = {{")
            lines.extend(f"    {cpp_string(flag)}," for flag in flags)
            lines.extend(["};", ""])
        if attributes:
            lines.append(f"static constexpr Refl::RuntimeAttribute {symbol}Attributes[] = {{")
            lines.extend(
                f"    {{{cpp_string(attribute['name'])}, {cpp_string(attribute['value'])}}},"
                for attribute in attributes
            )
            lines.extend(["};", ""])

    def emit_enums(enums: List[Dict[str, Any]], array_name: str, value_prefix: str) -> None:
        for enum_index, reflected_enum in enumerate(enums):
            values = reflected_enum.get("values", [])
            for value_index, value in enumerate(values):
                emit_metadata(value, f"{value_prefix}{enum_index}Value{value_index}")
            emit_metadata(reflected_enum, f"{value_prefix}{enum_index}")
            if not values:
                continue
            lines.append(f"static constexpr Refl::RuntimeEnumValueDescriptor {value_prefix}{enum_index}Values[] = {{")
            for value_index, value in enumerate(values):
                flag_span, attribute_span = metadata_spans(value, f"{value_prefix}{enum_index}Value{value_index}")
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cpp_string(value.get("name", "")),
                            str(value.get("value", 0)),
                            cpp_string(value.get("display_name", "")),
                            flag_span,
                            attribute_span,
                        ]
                    )
                    + "},"
                )
            lines.extend(["};", ""])

        if not enums:
            return
        lines.append(f"static constexpr Refl::RuntimeEnumDescriptor {array_name}[] = {{")
        for enum_index, reflected_enum in enumerate(enums):
            value_count = len(reflected_enum.get("values", []))
            value_span = (
                f"std::span<const Refl::RuntimeEnumValueDescriptor>{{{value_prefix}{enum_index}Values}}"
                if value_count
                else "std::span<const Refl::RuntimeEnumValueDescriptor>{}"
            )
            flag_span, attribute_span = metadata_spans(reflected_enum, f"{value_prefix}{enum_index}")
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cpp_string(reflected_enum.get("name", "")),
                        cpp_string(reflected_enum.get("qualified_name", "")),
                        cpp_string(reflected_enum.get("underlying_type", "")),
                        value_span,
                        cpp_guid(reflected_enum.get("guid")),
                        flag_span,
                        attribute_span,
                    ]
                )
                + "},"
            )
        lines.extend(["};", ""])

    emit_enums(manifest_enums, "ModuleRuntimeEnums", "RuntimeEnum")
    emit_enums(referenced_enums, "ReferencedRuntimeEnumData", "ReferencedEnum")


    for type_index, reflected_type in enumerate(manifest_types):
        qualified_name = reflected_type.get("qualified_name", "")
        identifier = cpp_identifier(qualified_name)
        fields = reflected_type.get("fields", [])
        lines.append(f"static void* ConstructDefault_{identifier}() {{")
        lines.append(f"    return DefaultValue<{qualified_name}>();")
        lines.append("}")
        lines.append("")
        lines.append(f"static void Destroy_{identifier}(void* object) {{")
        lines.append(f"    delete static_cast<{qualified_name}*>(object);")
        lines.append("}")
        lines.append("")
        lines.append(f"static void* Copy_{identifier}(const void* object) {{")
        lines.append(f"    return CopyValue<{qualified_name}>(object);")
        lines.append("}")
        lines.append("")
        for field in fields:
            field_name = field.get("name", "")
            field_type = field.get("type", "")
            field_identifier = f"{identifier}_{cpp_identifier(field_name)}"
            enum_type = field.get("enum_type", "")
            lines.append(f"static const void* GetConst_{field_identifier}(const void* object) {{")
            lines.append(f"    return &static_cast<const {qualified_name}*>(object)->{field_name};")
            lines.append("}")
            lines.append("")
            lines.append(f"static void* GetMutable_{field_identifier}(void* object) {{")
            lines.append(f"    return MutableField(static_cast<{qualified_name}*>(object)->{field_name});")
            lines.append("}")
            lines.append("")
            lines.append(f"static void Serialize_{field_identifier}(")
            lines.append(f"    const Refl::RuntimeFieldDescriptor&{' field' if enum_type else ''},")
            lines.append("    Serialization::SerializationBackend& backend,")
            lines.append("    const std::string& name,")
            lines.append("    const void* object) {")
            lines.append(f"    const auto& component = *static_cast<const {qualified_name}*>(object);")
            if enum_type:
                lines.append(f"    const auto enumValue = static_cast<int64_t>(component.{field_name});")
                lines.append("    const auto* enumType = field.EnumType;")
                lines.append('    if (!enumType) throw std::logic_error("Reflected enum metadata is unavailable");')
                lines.append("    if (const auto* value = Refl::FindRuntimeEnumValueByValue(*enumType, enumValue)) {")
                lines.append("        backend.Serialize(name, std::string(value->Name));")
                lines.append("    }")
            else:
                lines.append(f"    Serialization::SerializeValue(backend, name, component.{field_name});")
            lines.append("}")
            lines.append("")
            lines.append(f"static bool Deserialize_{field_identifier}(")
            lines.append(f"    const Refl::RuntimeFieldDescriptor&{' field' if enum_type else ''},")
            lines.append("    Serialization::SerializationBackend& backend,")
            lines.append("    const std::string& name,")
            lines.append("    void* object) {")
            lines.append(f"    auto& component = *static_cast<{qualified_name}*>(object);")
            if enum_type:
                lines.append("    std::string enumName;")
                lines.append("    if (!backend.Deserialize(name, enumName)) {")
                lines.append("        return false;")
                lines.append("    }")
                lines.append("    const auto* enumType = field.EnumType;")
                lines.append("    if (!enumType) return false;")
                lines.append("    const auto* value = Refl::FindRuntimeEnumValueByName(*enumType, enumName);")
                lines.append("    if (value == nullptr) {")
                lines.append("        return false;")
                lines.append("    }")
                lines.append(f"    return AssignEnum(component.{field_name}, value->Value);")
            else:
                lines.append(f"    return DeserializeField(backend, name, component.{field_name});")
            lines.append("}")
            lines.append("")
    for type_index, reflected_type in enumerate(manifest_types):
        fields = reflected_type.get("fields", [])
        for field_index, field in enumerate(fields):
            emit_metadata(field, f"RuntimeType{type_index}Field{field_index}")
        emit_metadata(reflected_type, f"RuntimeType{type_index}")
        if fields:
            qualified_name = reflected_type.get("qualified_name", "")
            identifier = cpp_identifier(qualified_name)
            lines.append(f"static const Refl::RuntimeFieldDescriptor RuntimeType{type_index}Fields[] = {{")
            for field_index, field in enumerate(fields):
                field_name = field.get("name", "")
                field_type = field.get("type", "")
                runtime_type = field.get("runtime_type", field_type)
                field_identifier = f"{identifier}_{cpp_identifier(field_name)}"
                enum_pointer = enum_pointer_for_field(field)
                offset = f"offsetof({qualified_name}, {field_name})"
                size = f"sizeof(static_cast<{qualified_name}*>(nullptr)->{field_name})"
                flags = "Refl::RuntimeFieldFlags::None"
                if field.get("serializable", True):
                    flags += " | Refl::RuntimeFieldFlags::Serializable"
                if field.get("read_only", False):
                    flags += " | Refl::RuntimeFieldFlags::ReadOnly"
                if field.get("editable", True) and is_editable_runtime_field_type(runtime_type, bool(field.get("enum_type"))):
                    flags += " | Refl::RuntimeFieldFlags::Editable"
                get_const = f"&GetConst_{field_identifier}"
                get_mutable = "nullptr" if field.get("read_only", False) else f"&GetMutable_{field_identifier}"
                serialize_field = f"&Serialize_{field_identifier}" if field.get("serializable", True) else "nullptr"
                deserialize_field = f"&Deserialize_{field_identifier}" if field.get("serializable", True) and not field.get("read_only", False) else "nullptr"
                metadata_flags, attributes = metadata_spans(field, f"RuntimeType{type_index}Field{field_index}")
                value_type_guid = field.get("value_type_guid")
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cpp_string(field_name),
                            cpp_string(runtime_type),
                            cpp_string(field.get("display_name", "")),
                            cpp_string(field.get("category", "")),
                            offset,
                            size,
                            flags,
                            get_const,
                            get_mutable,
                            serialize_field,
                            deserialize_field,
                            enum_pointer,
                            cpp_string(field.get("enum_type", "")),
                            metadata_flags,
                            attributes,
                            cpp_guid(value_type_guid) if value_type_guid else "{}",
                        ]
                    )
                    + "},"
                )
            lines.append("};")
            lines.append("")

    if manifest_types:
        lines.append("static const Refl::RuntimeTypeDescriptor ModuleRuntimeTypes[] = {")
        for type_index, reflected_type in enumerate(manifest_types):
            field_count = len(reflected_type.get("fields", []))
            field_span = (
                f"std::span<const Refl::RuntimeFieldDescriptor>{{RuntimeType{type_index}Fields}}"
                if field_count
                else "std::span<const Refl::RuntimeFieldDescriptor>{}"
            )
            qualified_name = reflected_type.get("qualified_name", "")
            identifier = cpp_identifier(qualified_name)
            size = f"sizeof({qualified_name})"
            construct = f"&ConstructDefault_{identifier}"
            destroy = f"&Destroy_{identifier}"
            copy = f"&Copy_{identifier}"
            serialize = "nullptr"
            deserialize = "nullptr"
            metadata_flags, attributes = metadata_spans(reflected_type, f"RuntimeType{type_index}")
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cpp_string(reflected_type.get("name", "")),
                        cpp_string(qualified_name),
                        cpp_string(reflected_type.get("kind", "")),
                        cpp_string(reflected_type.get("display_name", "")),
                        cpp_string(reflected_type.get("category", "")),
                        size,
                        field_span,
                        construct,
                        destroy,
                        copy,
                        serialize,
                        deserialize,
                        f"TypeGuid_{type_index}",
                        metadata_flags,
                        attributes,
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")

    exports = (("RuntimeTypes", "Refl::RuntimeTypeDescriptor", "ModuleRuntimeTypes", manifest_types), ("RuntimeEnums", "Refl::RuntimeEnumDescriptor", "ModuleRuntimeEnums", manifest_enums), ("ReferencedRuntimeEnums", "Refl::RuntimeEnumDescriptor", "ReferencedRuntimeEnumData", referenced_enums))
    for function, value_type, storage, values in exports:
        lines.extend([f"std::span<const {value_type}> {function}() {{", f"    return {storage if values else '{}'};", "}", ""])
    lines.extend(["std::optional<Refl::RegistrationError> RegisterReflection(Refl::Registry& registry) {", "    for (const auto& value : RuntimeEnums()) {", "        if (auto error = registry.RegisterEnum(value)) return error;", "    }", "    for (const auto& value : ReferencedRuntimeEnums()) {", "        if (auto error = registry.RegisterEnum(value)) return error;", "    }"])
    for type_index, item in enumerate(manifest_types):
        lines.append(f"    if (auto error = registry.RegisterType(RuntimeTypes()[{type_index}], Refl::NativeTypeKey<::{item['qualified_name']}>())) return error;")
    lines.extend(["    return std::nullopt;", "}", "}", ""])

    ecs_header_lines = ["#pragma once", "", '#include "HuaEngine/ECS/Runtime/TypeRegistry.h"', f'#include "{module}/GeneratedReflection.h"', ""]
    for item in manifest_types:
        if item.get("kind") != "component":
            continue
        qualified = item["qualified_name"]
        namespace, _, name = qualified.rpartition("::")
        declaration = f"{item.get('declaration_kind', 'struct')} {name};"
        ecs_header_lines.append(f"namespace {namespace} {{ {declaration} }}" if namespace else declaration)
    ecs_header_lines.extend(["", f"namespace HE::Generated::{module} {{", "Ecs::Result<void> RegisterComponents(Ecs::TypeRegistry& registry);", "}", "", "namespace HE::Ecs {", ""])
    for type_index, item in enumerate(manifest_types):
        if item.get("kind") == "component":
            ecs_header_lines.extend([f"template<> struct ComponentTraits<::{item['qualified_name']}> {{", f"    static constexpr Refl::TypeGuid Guid = Generated::{module}::TypeGuid_{type_index};", f"    static constexpr std::string_view Name = {cpp_string(item.get('type_name', item['name']))};", f"    static constexpr bool IsTag = {cpp_bool(item.get('tag', False))};", "    static Result<void> RegisterDependencies(TypeRegistry& registry);", "    static TypeDescriptor Describe();", "};", ""])
    ecs_header_lines.extend(["}", ""])

    ecs_lines = ['#include "GeneratedEcs.h"']
    ecs_lines.extend(f'#include "{path}"' for path in include_paths)
    ecs_lines.extend(["", f"namespace HE::Generated::{module} {{", ""])
    for type_index, item in enumerate(manifest_types):
        if item.get("kind") != "component":
            continue
        qualified_name = item["qualified_name"]
        identifier = cpp_identifier(qualified_name)
        ecs_lines.extend([f"static Ecs::TypeDescriptor MakeEcsType_{identifier}() {{", f"    auto descriptor = Ecs::MakeTypeDescriptor<{qualified_name}>(TypeGuid_{type_index}, {cpp_string(item.get('type_name', item['name']))}, {cpp_bool(item.get('tag', False))});", f"    descriptor.QualifiedName = {cpp_string(qualified_name)};", f"    descriptor.Reflection = &RuntimeTypes()[{type_index}];", "    return descriptor;", "}", ""])
    ecs_lines.extend(["Ecs::Result<void> RegisterComponents(Ecs::TypeRegistry& registry) {", "    if (!registry.IsOwnerThread())", "        return Ecs::Error{Ecs::ErrorCode::WrongThread, \"RegisterComponents\", \"Components must be registered on the Context owner thread\"};", "    if (auto error = RegisterReflection(registry.Reflection()))", "        return Ecs::Error{Ecs::ErrorCode::InvalidType, \"RegisterReflection\", error->Message};"])
    for item in manifest_types:
        if item.get("kind") == "component":
            identifier = cpp_identifier(item["qualified_name"])
            ecs_lines.append(f"    if (auto result = registry.Register(MakeEcsType_{identifier}()); !result) return result.GetError();")
    ecs_lines.extend(["    return {};", "}", "}", "", "namespace HE::Ecs {"])
    for item in manifest_types:
        if item.get("kind") != "component":
            continue
        identifier = cpp_identifier(item["qualified_name"])
        ecs_lines.extend([f"Result<void> ComponentTraits<::{item['qualified_name']}>::RegisterDependencies(TypeRegistry& registry) {{", "    if (!registry.IsOwnerThread())", "        return Error{ErrorCode::WrongThread, \"RegisterDependencies\", \"Components must be registered on the Context owner thread\"};", f"    if (auto error = Generated::{module}::RegisterReflection(registry.Reflection()))", "        return Error{ErrorCode::InvalidType, \"RegisterReflection\", error->Message};", "    return {};", "}", "", f"TypeDescriptor ComponentTraits<::{item['qualified_name']}>::Describe() {{", f"    return Generated::{module}::MakeEcsType_{identifier}();", "}", ""])
    ecs_lines.extend(["}", ""])
    return {"GeneratedReflection.h": "\n".join(header_lines), "GeneratedReflection.cpp": "\n".join(lines), "GeneratedEcs.h": "\n".join(ecs_header_lines), "GeneratedEcs.cpp": "\n".join(ecs_lines)}


def render_files(manifest: Dict[str, Any], reflection_only: bool = False) -> Dict[str, str]:
    files = render_reflection(manifest)
    if reflection_only:
        return {name: content for name, content in files.items() if name.startswith("GeneratedReflection.")}
    return files


def write_generated_files(manifest: Dict[str, Any], out_dir: Path, reflection_only: bool = False) -> List[Path]:
    result = render_files(manifest, reflection_only)
    for name in ("GeneratedQueries.h", "GeneratedQueries.cpp", "Queries.h"):
        (out_dir / name).unlink(missing_ok=True)
    if reflection_only:
        for name in ("GeneratedEcs.h", "GeneratedEcs.cpp"):
            (out_dir / name).unlink(missing_ok=True)
    for name, content in result.items():
        write_if_changed(out_dir / name, content)
    return [out_dir / name for name in result]


def generation_inputs(config: Dict[str, Any]) -> Dict[str, str]:
    return {"config_sha256": sha256(Path(config["_path"])), "compile_database_sha256": sha256(Path(config["compile_database"]) / "compile_commands.json"), "frontend_sha256": sha256(Path(config["hua_meta"])), "generator_sha256": sha256(Path(config["generator"]))}


def run_frontend(config: Dict[str, Any], persist_failure: bool = True) -> Dict[str, Any]:
    output_parent = Path(config["manifest"] if persist_failure else config.get("_path", config["manifest"])).parent
    if persist_failure:
        output_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="hua-meta-", dir=output_parent) as directory:
        output = Path(directory) / "manifest.json"
        arguments = [config["hua_meta"], "-p", config["compile_database"], "--output", str(output), "--resource-dir", config["resource_dir"], "--module", config["module"], "--configuration", config["configuration"], "--repository-root", config["repository_root"]]
        arguments.extend(["--entry-header", config["entry_header"]])
        arguments.append(config["translation_unit"])
        result = subprocess.run(arguments, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", check=False)
        if result.stdout:
            print(result.stdout, end="", file=sys.stderr)
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
        if result.returncode:
            if persist_failure and output.exists():
                write_if_changed(Path(config["manifest"]).with_suffix(".failed.json"), output.read_text(encoding="utf-8"))
            raise ValueError(f"HuaMeta failed with exit code {result.returncode}; generation was blocked.")
        manifest = read_json(output)
        verify_manifest(manifest, config)
        manifest["generation_inputs"] = generation_inputs(config)
        return manifest


def configuration_for(args: argparse.Namespace) -> Dict[str, Any]:
    config = load_config(args.meta_config)
    if getattr(args, "entry_header", None):
        requested = str(Path(args.entry_header).resolve())
        if requested != str(Path(config.get("entry_header", "")).resolve()):
            raise ValueError("--entry-header differs from the configured scanning target; run CMake configure with that entry header first.")
    return config


def command_scan(args: argparse.Namespace) -> int:
    config = configuration_for(args)
    manifest = run_frontend(config)
    write_json(Path(args.out or config["manifest"]), manifest)
    return 0


def command_generate(args: argparse.Namespace) -> int:
    config = configuration_for(args)
    manifest = load_manifest(Path(args.manifest or config["manifest"]), config)
    output = Path(args.out_dir or config["output_dir"])
    write_generated_files(manifest, output, config.get("reflection_only", False))
    write_json(output / "generation-stamp.json", {"schema_version": SCHEMA_VERSION, "module": config["module"], "configuration": config["configuration"], "manifest_sha256": manifest_fingerprint(manifest), "generation_inputs": generation_inputs(config)})
    return 0


def command_validate(args: argparse.Namespace) -> int:
    config = configuration_for(args)
    manifest = run_frontend(config, persist_failure=False)
    expected = render_files(manifest, config.get("reflection_only", False))
    diagnostics = []
    current_fingerprint = manifest_fingerprint(manifest)
    manifest_path = Path(config["manifest"])
    try:
        saved_fingerprint = manifest_fingerprint(read_json(manifest_path))
    except (OSError, ValueError):
        saved_fingerprint = None
    if saved_fingerprint != current_fingerprint:
        diagnostics.append({"severity": "error", "code": "manifest.drift", "message": "The configured manifest is missing or stale, including its dependency digests; run reflection scan again.", "source": str(manifest_path), "line": 0, "column": 0})
    for name, text in expected.items():
        path = Path(config["output_dir"]) / name
        if not path.is_file() or path.read_bytes() != text.encode("utf-8"):
            diagnostics.append({"severity": "error", "code": "generated.drift", "message": f"Generated output is missing or out of date: {path}", "source": str(path), "line": 0, "column": 0})
    if config.get("reflection_only", False):
        for name in ("GeneratedEcs.h", "GeneratedEcs.cpp"):
            path = Path(config["output_dir"]) / name
            if path.exists():
                diagnostics.append({"severity": "error", "code": "generated.unexpected_output", "message": f"Reflection-only output contains an ECS adapter: {path}", "source": str(path), "line": 0, "column": 0})
    stamp_path = Path(config["output_dir"]) / "generation-stamp.json"
    try:
        stamp = read_json(stamp_path)
    except (OSError, ValueError):
        stamp = {}
    if stamp.get("generation_inputs") != manifest["generation_inputs"]:
        diagnostics.append({"severity": "error", "code": "generated.stale_inputs", "message": "Generated compilation/tool inputs changed; run reflection generation again.", "source": str(stamp_path), "line": 0, "column": 0})
    if stamp.get("manifest_sha256") != current_fingerprint:
        diagnostics.append({"severity": "error", "code": "generated.stale_manifest", "message": "Generated output was produced from a different manifest or dependency snapshot; run reflection generation again.", "source": str(stamp_path), "line": 0, "column": 0})
    manifest["diagnostics"].extend(diagnostics)
    print(json.dumps(manifest, indent=2, ensure_ascii=False))
    return 1 if diagnostics else 0


def validate_module_set(modules: List[Dict[str, Any]]) -> None:
    identities: Dict[tuple[str, str], str] = {}
    guid_owners: Dict[str, tuple[str, str]] = {}
    enum_metadata: Dict[str, tuple[str, str]] = {}
    configurations = {module["configuration"] for module in modules}
    if len(configurations) > 1:
        raise ValueError("Module validation cannot mix build configurations")

    def enum_signature(item: Dict[str, Any]) -> str:
        def strip_source(value: Any) -> Any:
            if isinstance(value, dict):
                return {
                    key: normalized_guid(nested) if key == "guid" else strip_source(nested)
                    for key, nested in value.items()
                    if key not in {"source", "line", "column"}
                }
            if isinstance(value, list):
                return [strip_source(nested) for nested in value]
            return value

        return json.dumps(strip_source(item), sort_keys=True, ensure_ascii=False, separators=(",", ":"))

    def record_enum(item: Dict[str, Any], module: str) -> None:
        name = item["qualified_name"]
        signature = enum_signature(item)
        previous = enum_metadata.setdefault(name, (signature, module))
        if previous[0] != signature:
            raise ValueError(f"Cross-module conflicting enum metadata {name!r}: {previous[1]} and {module}")

    def record_guid(item: Dict[str, Any], module: str) -> None:
        guid = normalized_guid(item.get("guid"))
        if not guid:
            raise ValueError(f"Cross-module reflected declaration lacks a Guid: {item['qualified_name']}")
        name = item["qualified_name"]
        previous = guid_owners.setdefault(guid, (name, module))
        if previous[0] != name:
            raise ValueError(f"Cross-module duplicate Guid {guid!r}: {previous[0]} in {previous[1]} and {name} in {module}")

    for manifest in modules:
        module = manifest["module"]
        keys = [("module", module)]
        for item in manifest["types"] + manifest["enums"]:
            keys.append(("qualified_name", item["qualified_name"]))
            record_guid(item, module)
            if item.get("kind") == "component":
                keys.append(("TypeName", item.get("type_name", item["name"])))
            for field in item.get("fields", []):
                metadata = field.get("enum_metadata")
                if isinstance(metadata, dict) and metadata.get("qualified_name"):
                    record_guid(metadata, module)
                    record_enum(metadata, module)
        for item in manifest["enums"]:
            record_enum(item, module)
        for key in keys:
            if key in identities:
                raise ValueError(f"Cross-module duplicate {key[0]} {key[1]!r}: {identities[key]} and {module}")
            identities[key] = module


def command_validate_modules(args: argparse.Namespace) -> int:
    modules = []
    for path in args.meta_config:
        config = load_config(path)
        modules.append(load_manifest(Path(config["manifest"]), config))
    validate_module_set(modules)
    print(json.dumps({"schema_version": SCHEMA_VERSION, "modules": [item["module"] for item in modules], "diagnostics": []}))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="HuaEngine Clang manifest v2 reflection generator")
    commands = parser.add_subparsers(dest="command", required=True)
    for command, action in (("scan", command_scan), ("generate", command_generate), ("validate", command_validate)):
        child = commands.add_parser(command)
        child.add_argument("--meta-config", help="CMake-generated module/configuration meta-config.json; run CMake configure first")
        child.add_argument("--entry-header", help="Validate the configured entry header")
        if command == "scan":
            child.add_argument("--out")
        if command == "generate":
            child.add_argument("--manifest")
            child.add_argument("--out-dir")
        child.set_defaults(func=action)
    aggregate = commands.add_parser("validate-modules", help="Validate identity conflicts across explicitly configured modules")
    aggregate.add_argument("--meta-config", action="append", required=True)
    aggregate.set_defaults(func=command_validate_modules)
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(json.dumps({"error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
