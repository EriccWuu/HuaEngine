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
OUTPUT_NAMES = ("GeneratedReflection.h", "GeneratedReflection.cpp", "GeneratedQueries.h", "GeneratedQueries.cpp", "Queries.h")
PARAMETER_CATEGORIES = {"required_read", "required_write", "optional_read", "optional_write", "value", "random_read", "random_write", "resource_read", "resource_write", "commands", "entity", "output"}


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def cpp_bool(value: bool) -> str:
    return "true" if value else "false"


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
    if producer.get("name") != "HuaMeta" or producer.get("version") != "23.1.2-p6":
        raise ValueError("Manifest producer must be HuaMeta 23.1.2-p6")
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
    seen_guids: set[str] = set()
    registered_names: set[str] = set()
    for item in manifest["types"] + manifest["enums"]:
        name = item.get("qualified_name", "")
        if not name or name in seen_names:
            raise ValueError(f"Duplicate or empty reflected qualified name: {name}")
        seen_names.add(name)
        if item.get("kind") == "component":
            guid = item.get("guid", "").replace("-", "").lower()
            if not re.fullmatch(r"[0-9a-f]{32}", guid) or int(guid, 16) == 0 or guid in seen_guids:
                raise ValueError(f"Invalid or duplicate component Guid for {name}")
            seen_guids.add(guid)
            stable_name = item.get("type_name", item.get("name", ""))
            if not stable_name or stable_name in registered_names:
                raise ValueError(f"Invalid or duplicate component TypeName: {stable_name}")
            registered_names.add(stable_name)
            if item.get("tag", False) and item.get("fields"):
                raise ValueError(f"Tag component cannot declare stored fields: {name}")
    query_names: set[str] = set()
    for query in manifest["queries"]:
        name = query.get("qualified_name", "")
        if not name or name in query_names:
            raise ValueError(f"Duplicate or empty Query qualified name: {name}")
        query_names.add(name)
        if sum(parameter.get("category") == "output" for parameter in query.get("parameters", [])) > 1:
            raise ValueError(f"A query may declare only one BatchOutput parameter: {name}")
        for parameter in query.get("parameters", []):
            category = parameter.get("category")
            if category not in PARAMETER_CATEGORIES:
                raise ValueError(f"Unsupported Query parameter category: {category}")
            if category != "commands" and not parameter.get("component_type"):
                raise ValueError(f"Query parameter requires canonical component_type: {name}")


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

    def enum_pointer_for_field(field: Dict[str, Any]) -> str:
        name = field.get("enum_type", "")
        if not name:
            return "nullptr"
        index = enum_index_by_qualified_name.get(name)
        fallback = f"&ModuleRuntimeEnums[{index}]" if index is not None else "nullptr"
        return f"ResolveEnum({cpp_string(name)}, {fallback})"

    header_lines = ["#pragma once", "", '#include "HuaEngine/ECS/Runtime/TypeRegistry.h"', '#include "HuaEngine/Reflection/Reflection.h"', "", "// Include this module before Register<T> for generated types in every translation unit.", ""]
    for item in manifest_types:
        qualified = item["qualified_name"]
        namespace, _, name = qualified.rpartition("::")
        declaration = f"{item.get('declaration_kind', 'struct')} {name};"
        header_lines.append(f"namespace {namespace} {{ {declaration} }}" if namespace else declaration)
    header_lines.extend(["", f"namespace HE::Generated::{module} {{", "std::span<const Refl::RuntimeTypeDescriptor> RuntimeTypes();", "std::span<const ReflectedTypeInfo> ReflectedTypes();", "std::span<const Refl::RuntimeEnumDescriptor> RuntimeEnums();", "std::span<const ReflectedEnumInfo> ReflectedEnums();", "Ecs::Result<void> RegisterComponents(Ecs::TypeRegistry& registry);", "}", "", "namespace HE::Ecs {", ""])
    for item in manifest_types:
        if item.get("kind") != "component":
            continue
        guid = item["guid"].replace("-", "").lower()
        header_lines.extend([f"template<> struct ComponentTraits<::{item['qualified_name']}> {{", f"    static constexpr TypeGuid Guid{{0x{guid[:16]}ULL, 0x{guid[16:]}ULL}};", f"    static constexpr std::string_view Name = {cpp_string(item.get('type_name', item['name']))};", f"    static constexpr bool IsTag = {cpp_bool(item.get('tag', False))};", "    static TypeDescriptor Describe();", "};", ""])
    header_lines.extend(["}", ""])
    include_paths = sorted({generated_include_for_source(item["source"]) for item in manifest_types + manifest_enums})
    lines = ['#include "GeneratedReflection.h"', '#include "HuaEngine/Serialization/Serialization.h"', "#include <type_traits>", "#include <utility>"]
    lines.extend(f'#include "{path}"' for path in include_paths)
    lines.extend(["", f"namespace HE::Generated::{module} {{", "", "template<class T> static void* MutableField(T& value) {", "    if constexpr (std::is_const_v<T>) return nullptr;", "    else return &value;", "}", "template<class T> static bool DeserializeField(Serialization::SerializationBackend& backend, const std::string& name, T& value) {", "    if constexpr (!std::is_const_v<T> && std::is_copy_constructible_v<T> && std::is_move_assignable_v<T>) {", "        T candidate(value);", "        if (!Serialization::DeserializeValue(backend, name, candidate)) return false;", "        value = std::move(candidate);", "        return true;", "    } else if constexpr (!std::is_const_v<T> && std::is_default_constructible_v<T> && std::is_move_assignable_v<T>) {", "        T candidate{};", "        if (!Serialization::DeserializeValue(backend, name, candidate)) return false;", "        value = std::move(candidate);", "        return true;", "    } else return false;", "}", "static const Refl::RuntimeEnumDescriptor* ResolveEnum(std::string_view name, const Refl::RuntimeEnumDescriptor* local) {", "    const auto* shared = Refl::FindRuntimeEnum(name);", "    return shared ? shared : local;", "}", ""])
    lines.extend(["template<class T> static void* DefaultValue() {", "    if constexpr (std::is_default_constructible_v<T>) return new T();", "    else return nullptr;", "}", "template<class T> static void* CopyValue(const void* source) {", "    if constexpr (std::is_copy_constructible_v<T>) return new T(*static_cast<const T*>(source));", "    else return nullptr;", "}", "template<class T> static bool AssignEnum(T& target, int64_t value) {", "    if constexpr (std::is_assignable_v<T&, std::remove_const_t<T>>) {", "        target = static_cast<std::remove_const_t<T>>(value);", "        return true;", "    } else return false;", "}", ""])

    for enum_index, reflected_enum in enumerate(manifest_enums):
        values = reflected_enum.get("values", [])
        if values:
            lines.append(f"static constexpr Refl::RuntimeEnumValueDescriptor RuntimeEnum{enum_index}Values[] = {{")
            for value in values:
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cpp_string(value.get("name", "")),
                            str(value.get("value", 0)),
                            cpp_string(value.get("display_name", "")),
                        ]
                    )
                    + "},"
                )
            lines.append("};")
            lines.append("")
            lines.append(f"static constexpr ReflectedEnumValueInfo ReflectedEnum{enum_index}Values[] = {{")
            for value in values:
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cpp_string(value.get("name", "")),
                            str(value.get("value", 0)),
                            cpp_string(value.get("display_name", "")),
                        ]
                    )
                    + "},"
                )
            lines.append("};")
            lines.append("")

    if manifest_enums:
        lines.append("static constexpr Refl::RuntimeEnumDescriptor ModuleRuntimeEnums[] = {")
        for enum_index, reflected_enum in enumerate(manifest_enums):
            value_count = len(reflected_enum.get("values", []))
            value_span = (
                f"std::span<const Refl::RuntimeEnumValueDescriptor>{{RuntimeEnum{enum_index}Values}}"
                if value_count
                else "std::span<const Refl::RuntimeEnumValueDescriptor>{}"
            )
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cpp_string(reflected_enum.get("name", "")),
                        cpp_string(reflected_enum.get("qualified_name", "")),
                        cpp_string(reflected_enum.get("underlying_type", "")),
                        value_span,
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")

        lines.append("static constexpr ReflectedEnumInfo ModuleReflectedEnums[] = {")
        for enum_index, reflected_enum in enumerate(manifest_enums):
            value_count = len(reflected_enum.get("values", []))
            value_span = (
                f"std::span<const ReflectedEnumValueInfo>{{ReflectedEnum{enum_index}Values}}"
                if value_count
                else "std::span<const ReflectedEnumValueInfo>{}"
            )
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cpp_string(reflected_enum.get("name", "")),
                        cpp_string(reflected_enum.get("qualified_name", "")),
                        cpp_string(reflected_enum.get("underlying_type", "")),
                        value_span,
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")

    for type_index, reflected_type in enumerate(manifest_types):
        fields = reflected_type.get("fields", [])
        if fields:
            lines.append(f"static constexpr ReflectedFieldInfo Type{type_index}Fields[] = {{")
            for field in fields:
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cpp_string(field.get("name", "")),
                            cpp_string(field.get("type", "")),
                        ]
                    )
                    + "},"
                )
            lines.append("};")
            lines.append("")

    for type_index, reflected_type in enumerate(manifest_types):
        if reflected_type.get("kind", "") != "component":
            continue

        qualified_name = reflected_type.get("qualified_name", "")
        identifier = cpp_identifier(qualified_name)
        fields = reflected_type.get("fields", [])
        lines.append(f"static Ecs::TypeDescriptor MakeEcsType_{identifier}();")
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
            enum_pointer = enum_pointer_for_field(field)
            lines.append(f"static const void* GetConst_{field_identifier}(const void* object) {{")
            lines.append(f"    return &static_cast<const {qualified_name}*>(object)->{field_name};")
            lines.append("}")
            lines.append("")
            lines.append(f"static void* GetMutable_{field_identifier}(void* object) {{")
            lines.append(f"    return MutableField(static_cast<{qualified_name}*>(object)->{field_name});")
            lines.append("}")
            lines.append("")
            lines.append(f"static void Serialize_{field_identifier}(")
            lines.append("    Serialization::SerializationBackend& backend,")
            lines.append("    const std::string& name,")
            lines.append("    const void* object) {")
            lines.append(f"    const auto& component = *static_cast<const {qualified_name}*>(object);")
            if enum_pointer != "nullptr":
                lines.append(f"    const auto enumValue = static_cast<int64_t>(component.{field_name});")
                lines.append(f"    const auto* enumType = {enum_pointer};")
                lines.append('    if (!enumType) throw std::logic_error("Reflected enum metadata is unavailable");')
                lines.append("    if (const auto* value = Refl::FindRuntimeEnumValueByValue(*enumType, enumValue)) {")
                lines.append("        backend.Serialize(name, std::string(value->Name));")
                lines.append("    }")
            else:
                lines.append(f"    Serialization::SerializeValue(backend, name, component.{field_name});")
            lines.append("}")
            lines.append("")
            lines.append(f"static bool Deserialize_{field_identifier}(")
            lines.append("    Serialization::SerializationBackend& backend,")
            lines.append("    const std::string& name,")
            lines.append("    void* object) {")
            lines.append(f"    auto& component = *static_cast<{qualified_name}*>(object);")
            if enum_pointer != "nullptr":
                lines.append("    std::string enumName;")
                lines.append("    if (!backend.Deserialize(name, enumName)) {")
                lines.append("        return false;")
                lines.append("    }")
                lines.append(f"    const auto* enumType = {enum_pointer};")
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
        if fields:
            qualified_name = reflected_type.get("qualified_name", "")
            identifier = cpp_identifier(qualified_name)
            is_component = reflected_type.get("kind", "") == "component"
            lines.append(f"static const Refl::RuntimeFieldDescriptor RuntimeType{type_index}Fields[] = {{")
            for field in fields:
                field_name = field.get("name", "")
                field_type = field.get("type", "")
                runtime_type = field.get("runtime_type", field_type)
                field_identifier = f"{identifier}_{cpp_identifier(field_name)}"
                enum_pointer = enum_pointer_for_field(field)
                if is_component:
                    offset = f"offsetof({qualified_name}, {field_name})"
                    size = f"sizeof(static_cast<{qualified_name}*>(nullptr)->{field_name})"
                    flags = "Refl::RuntimeFieldFlags::ComponentField"
                    if field.get("serializable", True):
                        flags += " | Refl::RuntimeFieldFlags::Serializable"
                    if field.get("read_only", False):
                        flags += " | Refl::RuntimeFieldFlags::ReadOnly"
                    if field.get("editable", True) and is_editable_runtime_field_type(runtime_type, enum_pointer != "nullptr"):
                        flags += " | Refl::RuntimeFieldFlags::Editable"
                    get_const = f"&GetConst_{field_identifier}"
                    get_mutable = "nullptr" if field.get("read_only", False) else f"&GetMutable_{field_identifier}"
                    serialize_field = f"&Serialize_{field_identifier}" if field.get("serializable", True) else "nullptr"
                    deserialize_field = f"&Deserialize_{field_identifier}" if field.get("serializable", True) and not field.get("read_only", False) else "nullptr"
                else:
                    offset = "0"
                    size = "0"
                    flags = "Refl::RuntimeFieldFlags::None"
                    get_const = "nullptr"
                    get_mutable = "nullptr"
                    serialize_field = "nullptr"
                    deserialize_field = "nullptr"
                    enum_pointer = "nullptr"
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
            if reflected_type.get("kind", "") == "component":
                size = f"sizeof({qualified_name})"
                construct = f"&ConstructDefault_{identifier}"
                destroy = f"&Destroy_{identifier}"
                copy = f"&Copy_{identifier}"
                serialize = "nullptr"
                deserialize = "nullptr"
            else:
                size = "0"
                construct = "nullptr"
                destroy = "nullptr"
                copy = "nullptr"
                serialize = "nullptr"
                deserialize = "nullptr"
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
                        f"&MakeEcsType_{identifier}" if reflected_type.get("kind") == "component" else "nullptr",
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")

    for type_index, reflected_type in enumerate(manifest_types):
        if reflected_type.get("kind") != "component":
            continue
        qualified_name = reflected_type["qualified_name"]
        identifier = cpp_identifier(qualified_name)
        guid = reflected_type["guid"].replace("-", "").lower()
        lines.append(f"static Ecs::TypeDescriptor MakeEcsType_{identifier}() {{")
        lines.append(f"    auto descriptor = Ecs::MakeTypeDescriptor<{qualified_name}>(Ecs::TypeGuid{{0x{guid[:16]}ULL, 0x{guid[16:]}ULL}}, {cpp_string(reflected_type.get('type_name', reflected_type['name']))}, {cpp_bool(reflected_type.get('tag', False))});")
        lines.append(f"    descriptor.QualifiedName = {cpp_string(qualified_name)};")
        lines.append(f"    descriptor.Reflection = Refl::FindRuntimeType({cpp_string(qualified_name)});")
        lines.append(f"    if (!descriptor.Reflection) descriptor.Reflection = &ModuleRuntimeTypes[{type_index}];")
        lines.append("    return descriptor;")
        lines.append("}")
        lines.append("")

    if manifest_types:
        lines.append("static constexpr ReflectedTypeInfo ModuleReflectedTypes[] = {")
        for type_index, reflected_type in enumerate(manifest_types):
            field_count = len(reflected_type.get("fields", []))
            field_span = (
                f"std::span<const ReflectedFieldInfo>{{Type{type_index}Fields}}"
                if field_count
                else "std::span<const ReflectedFieldInfo>{}"
            )
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cpp_string(reflected_type.get("name", "")),
                        cpp_string(reflected_type.get("qualified_name", "")),
                        cpp_string(reflected_type.get("kind", "")),
                        cpp_string(reflected_type.get("display_name", "")),
                        cpp_string(reflected_type.get("category", "")),
                        field_span,
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")

    exports = (("RuntimeTypes", "Refl::RuntimeTypeDescriptor", "ModuleRuntimeTypes", manifest_types), ("ReflectedTypes", "ReflectedTypeInfo", "ModuleReflectedTypes", manifest_types), ("RuntimeEnums", "Refl::RuntimeEnumDescriptor", "ModuleRuntimeEnums", manifest_enums), ("ReflectedEnums", "ReflectedEnumInfo", "ModuleReflectedEnums", manifest_enums))
    for function, value_type, storage, values in exports:
        lines.extend([f"std::span<const {value_type}> {function}() {{", f"    return {storage if values else '{}'};", "}", ""])
    lines.extend(["Ecs::Result<void> RegisterComponents(Ecs::TypeRegistry& registry) {", "    for (const auto& type : RuntimeTypes()) {", "        if (!type.MakeEcsType) continue;", "        auto result = registry.Register(type.MakeEcsType());", "        if (!result) return result.GetError();", "    }", "    return {};", "}", "}", "", "namespace HE::Ecs {"])
    for item in manifest_types:
        if item.get("kind") == "component":
            lines.extend([f"TypeDescriptor ComponentTraits<::{item['qualified_name']}>::Describe() {{", f"    return Generated::{module}::MakeEcsType_{cpp_identifier(item['qualified_name'])}();", "}", ""])
    lines.extend(["}", ""])
    return {"GeneratedReflection.h": "\n".join(header_lines), "GeneratedReflection.cpp": "\n".join(lines)}


def render_queries(manifest: Dict[str, Any]) -> Dict[str, str]:
    module = manifest["module"]
    header = ["#pragma once", "", '#include "GeneratedReflection.h"', '#include "HuaEngine/ECS/Runtime/GeneratedQuery.h"', '#include "HuaEngine/ECS/Runtime/Timeline.h"']
    header.extend(f'#include "{path}"' for path in sorted({generated_include_for_source(query["source"]) for query in manifest["queries"]}))
    header.extend(["", f"namespace HE::Generated::{module} {{"])
    source = ['#include "GeneratedQueries.h"', "#include <utility>", "", f"namespace HE::Generated::{module} {{"]
    public_header = ["#pragma once", "", '#include "GeneratedQueries.h"', "#include <exception>", "#include <utility>", ""]
    for query in manifest["queries"]:
        parameters = query["parameters"]
        outputs = [parameter for parameter in parameters if parameter["category"] == "output"]
        if len(outputs) > 1:
            raise ValueError("A query may declare only one BatchOutput parameter")
        output_type = outputs[0]["component_type"] if outputs else None
        filters = query.get("filters", {})
        name = "Submit_" + cpp_identifier(query["qualified_name"])
        arguments = ["Ecs::Timeline& timeline", "Ecs::World& world"]
        public_arguments = ["::HE::Ecs::Timeline& timeline", "::HE::Ecs::World& world"]
        forwarded_arguments = ["timeline", "world"]
        captures: List[str] = []
        for index, parameter in enumerate(parameters):
            category = parameter["category"]
            value_type = parameter.get("component_type", "")
            if category == "value":
                arguments.append(f"{value_type} arg_{index}")
                public_arguments.append(f"{value_type} arg_{index}")
                forwarded_arguments.append(f"std::move(arg_{index})")
                captures.append(f"payload_{index} = std::move(arg_{index})")
            elif category.startswith("random_"):
                arguments.append(f"Ecs::World& arg_{index}")
                public_arguments.append(f"::HE::Ecs::World& arg_{index}")
                forwarded_arguments.append(f"arg_{index}")
                captures.append(f"world_{index} = arg_{index}.Id()")
            elif category.startswith("resource_"):
                arguments.append(f"Ecs::ResourceHandle arg_{index}")
                public_arguments.append(f"::HE::Ecs::ResourceHandle arg_{index}")
                forwarded_arguments.append(f"arg_{index}")
                captures.append(f"resource_{index} = arg_{index}")
        for index, _ in enumerate(filters.get("shared", [])):
            arguments.append(f"Ecs::ResourceHandle shared_{index}")
            public_arguments.append(f"::HE::Ecs::ResourceHandle shared_{index}")
            forwarded_arguments.append(f"shared_{index}")
        has_changed = bool(filters.get("changed"))
        if has_changed:
            arguments.append("Ecs::ChangedState& changed")
            public_arguments.append("::HE::Ecs::ChangedState& changed")
            forwarded_arguments.append("changed")
        result_type = f"Ecs::GeneratedOutputTask<{output_type}>" if output_type else "Ecs::TaskHandle"
        declaration = f"Ecs::Result<{result_type}> {name}({', '.join(arguments)})"
        header.append("[[nodiscard]] " + declaration + ";")
        query_namespace, _, query_name = query["qualified_name"].rpartition("::")
        public_namespace = f"{query_namespace}::Queries" if query_namespace else "Queries"
        public_result_type = result_type.replace("Ecs::", "::HE::Ecs::", 1)
        public_header.extend([
            f"namespace {public_namespace} {{",
            f"[[nodiscard]] inline ::HE::Ecs::Result<{public_result_type}> {query_name}({', '.join(public_arguments)}) {{",
            "    try {",
            f"        return ::HE::Generated::{module}::{name}({', '.join(forwarded_arguments)});",
            "    } catch (const std::exception& exception) {",
            "        return ::HE::Ecs::Error{::HE::Ecs::ErrorCode::ConstructionFailed, \"GeneratedQuery\", exception.what()};",
            "    } catch (...) {",
            "        return ::HE::Ecs::Error{::HE::Ecs::ErrorCode::ConstructionFailed, \"GeneratedQuery\", \"Query argument transfer failed\"};",
            "    }",
            "}", "}", "",
        ])
        source.extend(["", declaration + " {", "    try {", "    auto& context = world.Context();", "    if (!context.IsMainThread()) return Ecs::Error{Ecs::ErrorCode::WrongThread, \"GeneratedQuery\", \"Submission requires the Context owner thread\"};", "    Ecs::QuerySpec spec;"])
        component_names = [parameter["component_type"] for parameter in parameters if parameter["category"] not in {"value", "commands", "resource_read", "resource_write", "entity", "output"}]
        for filter_name in ("without", "required", "tag", "changed", "shared"):
            component_names.extend(filters.get(filter_name, []))
        type_variables = {value_type: f"type_{index}" for index, value_type in enumerate(dict.fromkeys(component_names))}
        for value_type, variable in type_variables.items():
            source.extend([f"    const auto* {variable} = context.Types().Find<{value_type}>();", f"    if (!{variable}) return Ecs::Error{{Ecs::ErrorCode::InvalidType, \"GeneratedQuery\", {cpp_string('Unregistered Query component: ' + value_type)}}};"])
        column_arguments: Dict[int, int] = {}
        for index, parameter in enumerate(parameters):
            category = parameter["category"]
            value_type = parameter.get("component_type", "")
            access = "Write" if category.endswith("write") else "Read"
            if category.startswith("required_") or category.startswith("optional_"):
                column_arguments[index] = len(column_arguments)
                presence = "Optional" if category.startswith("optional_") else "Required"
                source.append(f"    spec.Columns.push_back({{{type_variables[value_type]}->Id, Ecs::Presence::{presence}, Ecs::AccessMode::{access}}});")
            elif category.startswith("random_"):
                source.extend([f"    if (&arg_{index}.Context() != &context) return Ecs::Error{{Ecs::ErrorCode::InvalidArgument, \"GeneratedQuery\", \"Random target belongs to another Context\"}};", f"    spec.RandomAccesses.push_back({{&arg_{index}, {type_variables[value_type]}->Id, Ecs::AccessMode::{access}}});"])
            elif category.startswith("resource_"):
                source.extend([f"    const auto* resource_{index} = context.Resources().Find(arg_{index});", f"    if (!resource_{index} || resource_{index}->NativeKey != Ecs::NativeTypeKey<{value_type}>()) return Ecs::Error{{Ecs::ErrorCode::InvalidType, \"GeneratedQuery\", \"Resource binding has the wrong Context or native type\"}};", f"    spec.ResourceAccesses.push_back({{arg_{index}, Ecs::AccessMode::{access}}});"])
        for field, member in (("without", "Exclude"), ("required", "Required"), ("tag", "Required"), ("changed", "ChangedTypes")):
            for value_type in filters.get(field, []):
                source.append(f"    spec.{member}.push_back({type_variables[value_type]}->Id);")
        for index, value_type in enumerate(filters.get("shared", [])):
            source.extend([f"    spec.SharedBindings.push_back({{{type_variables[value_type]}->Id, shared_{index}}});", f"    spec.ResourceAccesses.push_back({{shared_{index}, Ecs::AccessMode::Read}});"])
        source.extend([f"    spec.IncludeDisabledEntities = {cpp_bool(filters.get('include_disabled', False))};", f"    spec.IgnoreComponentEnabled = {cpp_bool(filters.get('ignore_component_enabled', False))};", f"    auto query = context.FindOrCreateGeneratedQuery({cpp_string(module + '::' + query['qualified_name'])}, std::move(spec));", "    if (!query) return query.GetError();"])
        if output_type:
            source.append(f"    auto output = std::make_shared<Ecs::Detail::GeneratedOutputState<{output_type}>>();")
            captures.append("output")
        submission = "auto submitted = " if output_type else "return "
        source.extend([f"    {submission}timeline.Submit(*query.Value(), world, [{', '.join(captures)}](Ecs::TaskBatch& task) -> Ecs::Result<void> {{", "        auto& batch = task.View();"])
        if output_type:
            source.append(f"        std::vector<{output_type}> batchOutput;")
        call_arguments: List[str] = []
        for index, parameter in enumerate(parameters):
            category = parameter["category"]
            value_type = parameter.get("component_type", "")
            const_type = "const " + value_type if category.endswith("read") else value_type
            if index in column_arguments:
                source.extend([f"        auto column_{index} = batch.Column<{const_type}>({column_arguments[index]});", f"        if (!column_{index}) return column_{index}.GetError();"])
                if category.startswith("optional_"):
                    call_arguments.append(f"column_{index}.Value().TryGet(row)")
                else:
                    source.append(f"        auto span_{index} = column_{index}.Value().TryAsSpan();")
                    call_arguments.append(f"(span_{index} ? (*span_{index})[row] : column_{index}.Value().At(row))")
            elif category == "value":
                call_arguments.append(f"Ecs::Value<{value_type}>(payload_{index})")
            elif category.startswith("random_"):
                source.extend([f"        auto random_{index} = batch.Random<{const_type}>(world_{index});", f"        if (!random_{index}) return random_{index}.GetError();"])
                wrapper = "RandomRead" if category.endswith("read") else "RandomWrite"
                call_arguments.append(f"Ecs::{wrapper}<{value_type}>(random_{index}.Value())")
            elif category.startswith("resource_"):
                source.extend([f"        auto resource_view_{index} = batch.Resource<{const_type}>(resource_{index});", f"        if (!resource_view_{index}) return resource_view_{index}.GetError();"])
                wrapper = "ResourceRead" if category.endswith("read") else "ResourceWrite"
                call_arguments.append(f"Ecs::{wrapper}<{value_type}>(resource_view_{index}.Value().get())")
            elif category == "commands":
                call_arguments.append("task.Commands()")
            elif category == "entity":
                call_arguments.append("batch.Entity(row)")
            elif category == "output":
                call_arguments.append(f"Ecs::BatchOutput<{value_type}>(batchOutput)")
        source.extend(["        const size_t batchRows = batch.Size();", "        for (size_t row = 0; row < batchRows; ++row) {", "            if (!batch.Valid()) return Ecs::Error{Ecs::ErrorCode::InvalidState, \"GeneratedQuery\", \"The query batch expired\"};", f"            ::{query['qualified_name']}({', '.join(call_arguments)});", "        }", "        if (!batch.Valid()) return Ecs::Error{Ecs::ErrorCode::InvalidState, \"GeneratedQuery\", \"The query batch expired\"};"])
        source.append("        return output->Publish(task.BatchIndex(), std::move(batchOutput));" if output_type else "        return {};")
        source.append("    }, " + ("&changed" if has_changed else "nullptr") + ");")
        if output_type:
            source.extend(["    if (!submitted) return submitted.GetError();", f"    return Ecs::GeneratedOutputTask<{output_type}>(std::move(submitted).Value(), std::move(output));"])
        source.extend(["    } catch (const std::exception& exception) {", "        return Ecs::Error{Ecs::ErrorCode::ConstructionFailed, \"GeneratedQuery\", exception.what()};", "    } catch (...) {", "        return Ecs::Error{Ecs::ErrorCode::ConstructionFailed, \"GeneratedQuery\", \"Query payload construction failed\"};", "    }", "}"])
    header.extend(["}", ""])
    source.extend(["}", ""])
    return {"GeneratedQueries.h": "\n".join(header), "GeneratedQueries.cpp": "\n".join(source),
            "Queries.h": "\n".join(public_header)}


def render_files(manifest: Dict[str, Any]) -> Dict[str, str]:
    result = render_reflection(manifest)
    result.update(render_queries(manifest))
    return result


def write_generated_files(manifest: Dict[str, Any], out_dir: Path) -> List[Path]:
    result = render_files(manifest)
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
    write_generated_files(manifest, output)
    write_json(output / "generation-stamp.json", {"schema_version": SCHEMA_VERSION, "module": config["module"], "configuration": config["configuration"], "manifest_sha256": manifest_fingerprint(manifest), "generation_inputs": generation_inputs(config)})
    return 0


def command_validate(args: argparse.Namespace) -> int:
    config = configuration_for(args)
    manifest = run_frontend(config, persist_failure=False)
    expected = render_files(manifest)
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
    configurations = {module["configuration"] for module in modules}
    if len(configurations) > 1:
        raise ValueError("Module validation cannot mix build configurations")
    for manifest in modules:
        module = manifest["module"]
        keys = [("module", module)]
        for item in manifest["types"] + manifest["enums"] + manifest["queries"]:
            keys.append(("qualified_name", item["qualified_name"]))
            if item.get("kind") == "component":
                keys.append(("Guid", item["guid"].replace("-", "").lower()))
                keys.append(("TypeName", item.get("type_name", item["name"])))
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
