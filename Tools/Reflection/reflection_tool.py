#!/usr/bin/env python3
"""MVP source scanner and generator for HuaEngine reflection metadata."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


SCHEMA_VERSION = 1
SOURCE_EXTENSIONS = {".h", ".hpp", ".hh", ".hxx", ".cpp", ".cc", ".cxx"}
SKIPPED_DIRS = {
    ".git",
    ".vs",
    ".workspace",
    "Dependencies",
    "build",
    "out",
    "bin",
    "obj",
    "__pycache__",
}


def make_diagnostic(
    severity: str,
    code: str,
    message: str,
    source: Optional[str] = None,
    line: Optional[int] = None,
) -> Dict[str, Any]:
    diagnostic: Dict[str, Any] = {
        "severity": severity,
        "code": code,
        "message": message,
    }
    if source is not None:
        diagnostic["source"] = source
    if line is not None:
        diagnostic["line"] = line
    return diagnostic


def normalize_path(path: Path) -> str:
    return path.as_posix()


def read_text(path: Path) -> str:
    for encoding in ("utf-8-sig", "utf-8", "cp936"):
        try:
            return path.read_text(encoding=encoding)
        except UnicodeDecodeError:
            continue
    return path.read_text(encoding="utf-8", errors="replace")


def strip_comments_preserve_lines(text: str) -> str:
    result: List[str] = []
    index = 0
    in_line_comment = False
    in_block_comment = False
    in_string: Optional[str] = None
    escaped = False

    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""

        if in_line_comment:
            if char == "\n":
                in_line_comment = False
                result.append(char)
            else:
                result.append(" ")
            index += 1
            continue

        if in_block_comment:
            if char == "*" and next_char == "/":
                result.extend("  ")
                in_block_comment = False
                index += 2
            else:
                result.append("\n" if char == "\n" else " ")
                index += 1
            continue

        if in_string is not None:
            result.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            index += 1
            continue

        if char in ("'", '"'):
            in_string = char
            result.append(char)
            index += 1
            continue

        if char == "/" and next_char == "/":
            in_line_comment = True
            result.extend("  ")
            index += 2
            continue

        if char == "/" and next_char == "*":
            in_block_comment = True
            result.extend("  ")
            index += 2
            continue

        result.append(char)
        index += 1

    return "".join(result)


def line_for_offset(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def split_top_level_arguments(text: str) -> List[str]:
    args: List[str] = []
    start = 0
    depth = 0
    in_string: Optional[str] = None
    escaped = False

    for index, char in enumerate(text):
        if in_string is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            continue

        if char in ("'", '"'):
            in_string = char
            continue
        if char in "([{<":
            depth += 1
            continue
        if char in ")]}>":
            depth = max(0, depth - 1)
            continue
        if char == "," and depth == 0:
            args.append(text[start:index].strip())
            start = index + 1

    tail = text[start:].strip()
    if tail:
        args.append(tail)
    return args


def parse_metadata(args: Sequence[str]) -> Dict[str, str]:
    metadata: Dict[str, str] = {}
    for arg in args:
        match = re.fullmatch(r'(DisplayName|Category)\s*=\s*"((?:\\.|[^"\\])*)"', arg)
        if match:
            metadata[snake_case(match.group(1))] = bytes(
                match.group(2), "utf-8"
            ).decode("unicode_escape")
    return metadata


def snake_case(name: str) -> str:
    chars: List[str] = []
    for index, char in enumerate(name):
        if char.isupper() and index > 0:
            chars.append("_")
        chars.append(char.lower())
    return "".join(chars)


def find_macro_calls(text: str, macro_name: str) -> Iterable[Tuple[int, int, str]]:
    token = macro_name + "("
    search_from = 0
    while True:
        start = text.find(token, search_from)
        if start == -1:
            return
        line_start = text.rfind("\n", 0, start) + 1
        if text[line_start:start].lstrip().startswith("#"):
            search_from = start + len(token)
            continue

        open_paren = start + len(macro_name)
        index = open_paren + 1
        depth = 1
        in_string: Optional[str] = None
        escaped = False

        while index < len(text):
            char = text[index]
            if in_string is not None:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == in_string:
                    in_string = None
                index += 1
                continue

            if char in ("'", '"'):
                in_string = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    yield start, index + 1, text[open_paren + 1 : index]
                    search_from = index + 1
                    break
            index += 1
        else:
            yield start, len(text), text[open_paren + 1 :]
            return


def find_type_declaration(
    text: str, offset: int, expected_name: Optional[str] = None
) -> Optional[Dict[str, Any]]:
    window = text[offset : offset + 4096]
    pattern = re.compile(
        r"\b(?P<kind>class|struct)\s+(?:[A-Za-z_]\w*::)*(?P<name>[A-Za-z_]\w*)\b[^;{]*\{",
        re.MULTILINE,
    )
    for match in pattern.finditer(window):
        declared_name = match.group("name")
        if expected_name and declared_name != expected_name.split("::")[-1]:
            continue
        brace_offset = offset + match.end() - 1
        end_brace = find_matching_brace(text, brace_offset)
        if end_brace is None:
            return None
        return {
            "kind": match.group("kind"),
            "name": declared_name,
            "declaration_start": offset + match.start(),
            "body_start": brace_offset + 1,
            "body_end": end_brace,
        }
    return None


def infer_namespace(text: str, offset: int) -> str:
    pattern = re.compile(
        r"\bnamespace\s+(?P<name>[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)?\s*\{",
        re.MULTILINE,
    )
    active_namespaces: List[str] = []
    for match in pattern.finditer(text, 0, offset):
        namespace_name = match.group("name")
        if not namespace_name:
            continue
        open_brace = match.end() - 1
        close_brace = find_matching_brace(text, open_brace)
        if close_brace is not None and close_brace > offset:
            active_namespaces.extend(namespace_name.split("::"))
    return "::".join(active_namespaces)


def infer_qualified_name(text: str, declaration: Dict[str, Any]) -> str:
    namespace_name = infer_namespace(text, declaration["declaration_start"])
    if namespace_name:
        return f"{namespace_name}::{declaration['name']}"
    return f"HE::{declaration['name']}"


def find_matching_brace(text: str, open_brace: int) -> Optional[int]:
    depth = 0
    in_string: Optional[str] = None
    escaped = False
    for index in range(open_brace, len(text)):
        char = text[index]
        if in_string is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            continue

        if char in ("'", '"'):
            in_string = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def parse_field_statement(statement: str) -> Optional[Tuple[str, str]]:
    clean = " ".join(statement.strip().split())
    if not clean or "(" in clean or clean.startswith(("using ", "typedef ")):
        return None
    clean = clean.split("=", 1)[0].strip()
    clean = clean.split("{", 1)[0].strip()
    clean = clean.rstrip(";").strip()
    clean = re.sub(r"\b(static|mutable|constexpr|inline|volatile)\b", " ", clean)
    clean = " ".join(clean.split())
    match = re.fullmatch(r"(?P<type>.+?[\s*&])(?P<name>[A-Za-z_]\w*)", clean)
    if not match:
        return None
    field_type = match.group("type").strip()
    field_name = match.group("name").strip()
    if not field_type or field_name in {"public", "private", "protected"}:
        return None
    return field_type, field_name


def collect_fields(
    text: str,
    body_start: int,
    body_end: int,
    source: str,
    diagnostics: List[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    fields: List[Dict[str, Any]] = []
    body = text[body_start:body_end]
    for macro_start, macro_end, macro_args in find_macro_calls(body, "HE_REFLECT_FIELD"):
        absolute_macro_start = body_start + macro_start
        args = split_top_level_arguments(macro_args)
        metadata = parse_metadata(args)
        statement_start = body_start + macro_end
        statement_end = text.find(";", statement_start, body_end)
        if statement_end == -1:
            diagnostics.append(
                make_diagnostic(
                    "error",
                    "field.missing_semicolon",
                    "HE_REFLECT_FIELD is not followed by a field declaration ending in ';'.",
                    source,
                    line_for_offset(text, absolute_macro_start),
                )
            )
            continue

        statement = text[statement_start : statement_end + 1]
        parsed = parse_field_statement(statement)
        if parsed is None:
            diagnostics.append(
                make_diagnostic(
                    "error",
                    "field.unparsed_declaration",
                    "HE_REFLECT_FIELD is not followed by a simple field declaration.",
                    source,
                    line_for_offset(text, absolute_macro_start),
                )
            )
            continue

        field_type, field_name = parsed
        field: Dict[str, Any] = {
            "name": field_name,
            "type": field_type,
            "source": source,
            "line": line_for_offset(text, statement_start),
        }
        field.update(metadata)
        fields.append(field)
    return fields


def iter_source_files(root: Path) -> Iterable[Path]:
    for current_root, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in SKIPPED_DIRS]
        current_path = Path(current_root)
        for filename in filenames:
            path = current_path / filename
            if path.suffix.lower() in SOURCE_EXTENSIONS:
                yield path


def scan_file(root: Path, path: Path, diagnostics: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    source = normalize_path(path.relative_to(root))
    raw_text = read_text(path)
    text = strip_comments_preserve_lines(raw_text)
    types: List[Dict[str, Any]] = []

    for macro_start, macro_end, macro_args in find_macro_calls(text, "HE_REFLECT_COMPONENT"):
        line = line_for_offset(text, macro_start)
        args = split_top_level_arguments(macro_args)
        metadata = parse_metadata(args)
        declaration = find_type_declaration(text, macro_end)
        if declaration is None:
            diagnostics.append(
                make_diagnostic(
                    "error",
                    "component.declaration_not_found",
                    "Could not find a class or struct declaration after HE_REFLECT_COMPONENT.",
                    source,
                    line,
                )
            )
            continue

        qualified_name = infer_qualified_name(text, declaration)
        reflected_type: Dict[str, Any] = {
            "name": declaration["name"],
            "qualified_name": qualified_name,
            "kind": "component",
            "declaration_kind": declaration["kind"],
            "source": source,
            "line": line,
            "fields": collect_fields(
                text,
                declaration["body_start"],
                declaration["body_end"],
                source,
                diagnostics,
            ),
        }
        reflected_type.update(metadata)
        types.append(reflected_type)

    return types


def validate_reflected_types(manifest: Dict[str, Any]) -> None:
    diagnostics = manifest.setdefault("diagnostics", [])
    seen_qualified_names: Dict[str, Dict[str, Any]] = {}

    for reflected_type in manifest.get("types", []):
        source = reflected_type.get("source")
        line = reflected_type.get("line")
        qualified_name = reflected_type.get("qualified_name", "")

        if reflected_type.get("kind") == "component":
            if not reflected_type.get("display_name"):
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "component.missing_display_name",
                        "HE_REFLECT_COMPONENT is missing required DisplayName metadata.",
                        source,
                        line,
                    )
                )
            if not reflected_type.get("category"):
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "component.missing_category",
                        "HE_REFLECT_COMPONENT is missing required Category metadata.",
                        source,
                        line,
                    )
                )
            if not reflected_type.get("fields"):
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "component.no_reflected_fields",
                        "HE_REFLECT_COMPONENT types must declare at least one HE_REFLECT_FIELD.",
                        source,
                        line,
                    )
                )

        if qualified_name:
            previous = seen_qualified_names.get(qualified_name)
            if previous is not None:
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "component.duplicate_qualified_name",
                        f"Duplicate reflected type qualified_name: {qualified_name}",
                        source,
                        line,
                    )
                )
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "component.duplicate_qualified_name",
                        f"Duplicate reflected type qualified_name first seen here: {qualified_name}",
                        previous.get("source"),
                        previous.get("line"),
                    )
                )
            else:
                seen_qualified_names[qualified_name] = reflected_type


def scan_root(root: Path) -> Dict[str, Any]:
    diagnostics: List[Dict[str, Any]] = []
    types: List[Dict[str, Any]] = []
    resolved_root = root.resolve()

    if not resolved_root.exists():
        diagnostics.append(
            make_diagnostic(
                "error",
                "root.not_found",
                f"Root path does not exist: {resolved_root}",
            )
        )
    elif not resolved_root.is_dir():
        diagnostics.append(
            make_diagnostic(
                "error",
                "root.not_directory",
                f"Root path is not a directory: {resolved_root}",
            )
        )
    else:
        for path in sorted(iter_source_files(resolved_root)):
            types.extend(scan_file(resolved_root, path, diagnostics))

    types.sort(key=lambda item: (item["qualified_name"], item["source"], item["line"]))
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "types": types,
        "diagnostics": diagnostics,
    }
    validate_reflected_types(manifest)
    return manifest


def add_generated_drift_diagnostics(root: Path, manifest: Dict[str, Any]) -> None:
    generated_dir = root.resolve() / "HuaEngine" / "src" / "HuaEngine" / "Generated"
    generated_reflection_dir = generated_dir / "Reflection"
    baseline_generated_files = [
        generated_dir / "GeneratedReflection.h",
        generated_dir / "GeneratedReflection.cpp",
    ]
    if (
        not generated_dir.exists()
        and not generated_reflection_dir.exists()
        and not any(path.exists() for path in baseline_generated_files)
    ):
        return

    diagnostics = manifest.setdefault("diagnostics", [])
    with tempfile.TemporaryDirectory(prefix="hua_reflection_validate_") as temp_dir:
        expected_dir = Path(temp_dir)
        expected_files = write_generated_files(manifest, expected_dir)
        expected_relative_files = {
            expected_file.relative_to(expected_dir) for expected_file in expected_files
        }
        actual_relative_files = {
            Path("GeneratedReflection.h"),
            Path("GeneratedReflection.cpp"),
        }
        if generated_reflection_dir.exists():
            actual_relative_files.update(
                Path("Reflection") / path.name
                for path in generated_reflection_dir.glob("*.generated.h")
            )

        for relative_file in sorted(expected_relative_files | actual_relative_files):
            generated_file = generated_dir / relative_file
            expected_file = expected_dir / relative_file
            if not generated_file.exists():
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "generated.drift",
                        f"Generated reflection file is missing: {normalize_path(generated_file)}",
                    )
                )
                continue

            if relative_file not in expected_relative_files:
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "generated.drift",
                        f"Generated reflection file is obsolete: {normalize_path(generated_file)}",
                    )
                )
                continue

            if read_text(generated_file) != read_text(expected_file):
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "generated.drift",
                        f"Generated reflection file is out of date: {normalize_path(generated_file)}",
                    )
                )


def has_error_diagnostic(manifest: Dict[str, Any]) -> bool:
    return any(
        diagnostic.get("severity") == "error"
        for diagnostic in manifest.get("diagnostics", [])
    )


def write_json(path: Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def load_manifest(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"Unsupported manifest schema_version: {manifest.get('schema_version')!r}"
        )
    return manifest


def cpp_string(value: str) -> str:
    return json.dumps(value)


def macro_identifier(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9]", "_", value).upper()


def cpp_identifier(value: str) -> str:
    identifier = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not identifier or identifier[0].isdigit():
        identifier = "_" + identifier
    return identifier


def generated_include_for_source(source: str) -> str:
    normalized = source.replace("\\", "/")
    prefix = "HuaEngine/src/"
    if normalized.startswith(prefix):
        return normalized[len(prefix) :]
    return normalized


def cleanup_obsolete_per_source_headers(out_dir: Path) -> None:
    reflection_dir = out_dir / "Reflection"
    if not reflection_dir.exists():
        return

    for path in reflection_dir.glob("*.generated.h"):
        if path.is_file():
            path.unlink()

    try:
        reflection_dir.rmdir()
    except OSError:
        pass


def write_generated_files(manifest: Dict[str, Any], out_dir: Path) -> List[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    cleanup_obsolete_per_source_headers(out_dir)
    header_path = out_dir / "GeneratedReflection.h"
    source_path = out_dir / "GeneratedReflection.cpp"
    manifest_types = manifest.get("types", [])
    written_files = [header_path, source_path]

    header_lines = [
        "#pragma once",
        "",
        "#include <span>",
        "#include <string_view>",
        "",
        "namespace HE {",
        "class ComponentRegistry;",
        "} // namespace HE",
        "",
        "namespace HE::Generated {",
        "",
        "struct ReflectedFieldInfo {",
        "    std::string_view Name;",
        "    std::string_view Type;",
        "};",
        "",
        "struct ReflectedTypeInfo {",
        "    std::string_view Name;",
        "    std::string_view QualifiedName;",
        "    std::string_view Kind;",
        "    std::string_view DisplayName;",
        "    std::string_view Category;",
        "    std::span<const ReflectedFieldInfo> Fields;",
        "};",
        "",
        "std::span<const ReflectedTypeInfo> GetReflectedTypes();",
        "const ReflectedTypeInfo* FindReflectedType(std::string_view qualifiedName);",
        "void RegisterGeneratedComponents(ComponentRegistry& registry);",
        "",
        "} // namespace HE::Generated",
        "",
    ]

    header = "\n".join(header_lines)

    include_paths = sorted(
        {
            generated_include_for_source(reflected_type.get("source", ""))
            for reflected_type in manifest_types
            if reflected_type.get("source", "")
        }
    )

    lines = [
        '#include "GeneratedReflection.h"',
        "",
        "#include <string>",
        "#include <string_view>",
        "#include <vector>",
        "",
        '#include "HuaEngine/ECS/ComponentRegistry.h"',
        '#include "HuaEngine/ECS/ComponentType.h"',
        '#include "HuaEngine/ECS/World.h"',
        '#include "HuaEngine/Reflection/Reflection.h"',
        '#include "HuaEngine/Serialization/Serialization.h"',
    ]
    for include_path in include_paths:
        lines.append(f'#include "{include_path}"')
    lines.extend(
        [
            "",
            "namespace HE::Generated {",
            "",
        ]
    )

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
        lines.append(f"static void* ConstructDefault_{identifier}() {{")
        lines.append(f"    return new {qualified_name}();")
        lines.append("}")
        lines.append("")
        lines.append(f"static void Destroy_{identifier}(void* object) {{")
        lines.append(f"    delete static_cast<{qualified_name}*>(object);")
        lines.append("}")
        lines.append("")
        lines.append(f"static void* Copy_{identifier}(const void* object) {{")
        lines.append(f"    return new {qualified_name}(*static_cast<const {qualified_name}*>(object));")
        lines.append("}")
        lines.append("")
        lines.append(f"static void AddCopyToWorld_{identifier}(World& world, EntityId entity, const void* object) {{")
        lines.append(f"    world.AddComponent<{qualified_name}>(entity, *static_cast<const {qualified_name}*>(object));")
        lines.append("}")
        lines.append("")
        for field in fields:
            field_name = field.get("name", "")
            field_identifier = f"{identifier}_{cpp_identifier(field_name)}"
            lines.append(f"static const void* GetConst_{field_identifier}(const void* object) {{")
            lines.append(f"    return &static_cast<const {qualified_name}*>(object)->{field_name};")
            lines.append("}")
            lines.append("")
            lines.append(f"static void* GetMutable_{field_identifier}(void* object) {{")
            lines.append(f"    return &static_cast<{qualified_name}*>(object)->{field_name};")
            lines.append("}")
            lines.append("")
            lines.append(f"static void Serialize_{field_identifier}(")
            lines.append("    Serialization::SerializationBackend& backend,")
            lines.append("    const std::string& name,")
            lines.append("    const void* object) {")
            lines.append(f"    const auto& component = *static_cast<const {qualified_name}*>(object);")
            lines.append(f"    Serialization::SerializeValue(backend, name, component.{field_name});")
            lines.append("}")
            lines.append("")
            lines.append(f"static bool Deserialize_{field_identifier}(")
            lines.append("    Serialization::SerializationBackend& backend,")
            lines.append("    const std::string& name,")
            lines.append("    void* object) {")
            lines.append(f"    auto& component = *static_cast<{qualified_name}*>(object);")
            lines.append(f"    auto fieldValue = component.{field_name};")
            lines.append("    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {")
            lines.append("        return false;")
            lines.append("    }")
            lines.append(f"    component.{field_name} = fieldValue;")
            lines.append("    return true;")
            lines.append("}")
            lines.append("")
    for type_index, reflected_type in enumerate(manifest_types):
        fields = reflected_type.get("fields", [])
        if fields:
            qualified_name = reflected_type.get("qualified_name", "")
            identifier = cpp_identifier(qualified_name)
            is_component = reflected_type.get("kind", "") == "component"
            lines.append(f"static constexpr Refl::RuntimeFieldDescriptor RuntimeType{type_index}Fields[] = {{")
            for field in fields:
                field_name = field.get("name", "")
                field_identifier = f"{identifier}_{cpp_identifier(field_name)}"
                if is_component:
                    offset = f"offsetof({qualified_name}, {field_name})"
                    size = f"sizeof(static_cast<{qualified_name}*>(nullptr)->{field_name})"
                    flags = "Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::ComponentField"
                    get_const = f"&GetConst_{field_identifier}"
                    get_mutable = f"&GetMutable_{field_identifier}"
                    serialize_field = f"&Serialize_{field_identifier}"
                    deserialize_field = f"&Deserialize_{field_identifier}"
                else:
                    offset = "0"
                    size = "0"
                    flags = "Refl::RuntimeFieldFlags::None"
                    get_const = "nullptr"
                    get_mutable = "nullptr"
                    serialize_field = "nullptr"
                    deserialize_field = "nullptr"
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cpp_string(field_name),
                            cpp_string(field.get("type", "")),
                            cpp_string(field.get("display_name", "")),
                            cpp_string(field.get("category", "")),
                            offset,
                            size,
                            flags,
                            get_const,
                            get_mutable,
                            serialize_field,
                            deserialize_field,
                        ]
                    )
                    + "},"
                )
            lines.append("};")
            lines.append("")

    if manifest_types:
        lines.append("static const Refl::RuntimeTypeDescriptor RuntimeTypes[] = {")
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
                type_id = f"ComponentTypeIdOf<{qualified_name}>()"
                size = f"sizeof({qualified_name})"
                construct = f"&ConstructDefault_{identifier}"
                destroy = f"&Destroy_{identifier}"
                copy = f"&Copy_{identifier}"
                serialize = "nullptr"
                deserialize = "nullptr"
                add_copy_to_world = f"&AddCopyToWorld_{identifier}"
            else:
                type_id = "InvalidComponentTypeId"
                size = "0"
                construct = "nullptr"
                destroy = "nullptr"
                copy = "nullptr"
                serialize = "nullptr"
                deserialize = "nullptr"
                add_copy_to_world = "nullptr"
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cpp_string(reflected_type.get("name", "")),
                        cpp_string(qualified_name),
                        cpp_string(reflected_type.get("kind", "")),
                        cpp_string(reflected_type.get("display_name", "")),
                        cpp_string(reflected_type.get("category", "")),
                        type_id,
                        size,
                        field_span,
                        construct,
                        destroy,
                        copy,
                        serialize,
                        deserialize,
                        add_copy_to_world,
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")

    if manifest_types:
        lines.append("static constexpr ReflectedTypeInfo Types[] = {")
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
    lines.append("std::span<const ReflectedTypeInfo> GetReflectedTypes() {")
    if manifest_types:
        lines.append("    return Types;")
    else:
        lines.append("    return {};")
    lines.append("}")
    lines.append("")
    lines.append("const ReflectedTypeInfo* FindReflectedType(std::string_view qualifiedName) {")
    lines.append("    for (const ReflectedTypeInfo& type : GetReflectedTypes()) {")
    lines.append("        if (type.QualifiedName == qualifiedName) {")
    lines.append("            return &type;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("void RegisterGeneratedComponents(ComponentRegistry& registry) {")
    lines.append("    for (const Refl::RuntimeTypeDescriptor& type : Refl::GetRuntimeTypes()) {")
    lines.append("        if (type.Kind == \"component\") {")
    lines.append("            registry.Register(type);")
    lines.append("        }")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace HE::Generated")
    lines.append("")
    lines.append("namespace HE::Refl {")
    lines.append("")
    lines.append("namespace {")
    lines.append("using RuntimeTypeProvider = std::span<const RuntimeTypeDescriptor> (*)();")
    lines.append("")
    lines.append("std::span<const RuntimeTypeDescriptor> GetGeneratedRuntimeTypes() {")
    if manifest_types:
        lines.append("    return Generated::RuntimeTypes;")
    else:
        lines.append("    return {};")
    lines.append("}")
    lines.append("")
    lines.append("std::span<const RuntimeTypeProvider> GetRuntimeTypeProviders() {")
    lines.append("    static const RuntimeTypeProvider providers[] = {")
    lines.append("        &GetGeneratedRuntimeTypes,")
    lines.append("    };")
    lines.append("    return providers;")
    lines.append("}")
    lines.append("")
    lines.append("const std::vector<RuntimeTypeDescriptor>& GetRuntimeTypeCache() {")
    lines.append("    static const std::vector<RuntimeTypeDescriptor> types = [] {")
    lines.append("        std::vector<RuntimeTypeDescriptor> result;")
    lines.append("        for (RuntimeTypeProvider provider : GetRuntimeTypeProviders()) {")
    lines.append("            const std::span<const RuntimeTypeDescriptor> providedTypes = provider();")
    lines.append("            result.insert(result.end(), providedTypes.begin(), providedTypes.end());")
    lines.append("        }")
    lines.append("        return result;")
    lines.append("    }();")
    lines.append("    return types;")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace")
    lines.append("")
    lines.append("std::span<const RuntimeTypeDescriptor> GetRuntimeTypes() {")
    lines.append("    const std::vector<RuntimeTypeDescriptor>& types = GetRuntimeTypeCache();")
    lines.append("    return std::span<const RuntimeTypeDescriptor>{ types.data(), types.size() };")
    lines.append("}")
    lines.append("")
    lines.append("const RuntimeTypeDescriptor* FindRuntimeType(std::string_view qualifiedName) {")
    lines.append("    for (const RuntimeTypeDescriptor& type : GetRuntimeTypes()) {")
    lines.append("        if (type.QualifiedName == qualifiedName) {")
    lines.append("            return &type;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("const RuntimeTypeDescriptor* FindRuntimeType(ComponentTypeId typeId) {")
    lines.append("    for (const RuntimeTypeDescriptor& type : GetRuntimeTypes()) {")
    lines.append("        if (type.TypeId == typeId) {")
    lines.append("            return &type;")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("void SerializeRuntimeObject(")
    lines.append("    const RuntimeTypeDescriptor& type,")
    lines.append("    Serialization::SerializationBackend& backend,")
    lines.append("    const std::string& name,")
    lines.append("    const void* object) {")
    lines.append("    backend.BeginObject(name);")
    lines.append("    for (const RuntimeFieldDescriptor& field : type.Fields) {")
    lines.append("        if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) || field.Serialize == nullptr) {")
    lines.append("            continue;")
    lines.append("        }")
    lines.append("        field.Serialize(backend, std::string(field.Name), object);")
    lines.append("    }")
    lines.append("    backend.EndObject();")
    lines.append("}")
    lines.append("")
    lines.append("bool DeserializeRuntimeObject(")
    lines.append("    const RuntimeTypeDescriptor& type,")
    lines.append("    Serialization::SerializationBackend& backend,")
    lines.append("    const std::string& name,")
    lines.append("    void* object) {")
    lines.append("    if (!name.empty() && !backend.HasField(name)) {")
    lines.append("        return false;")
    lines.append("    }")
    lines.append("")
    lines.append("    backend.BeginObject(name);")
    lines.append("    bool success = true;")
    lines.append("    for (const RuntimeFieldDescriptor& field : type.Fields) {")
    lines.append("        if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) || field.Deserialize == nullptr) {")
    lines.append("            continue;")
    lines.append("        }")
    lines.append("        const std::string fieldName(field.Name);")
    lines.append("        if (!backend.HasField(fieldName)) {")
    lines.append("            continue;")
    lines.append("        }")
    lines.append("        if (!field.Deserialize(backend, fieldName, object)) {")
    lines.append("            success = false;")
    lines.append("        }")
    lines.append("    }")
    lines.append("    backend.EndObject();")
    lines.append("    return success;")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace HE::Refl")
    lines.append("")

    header_path.write_text(header, encoding="utf-8")
    source_path.write_text("\n".join(lines), encoding="utf-8")
    return written_files


def command_scan(args: argparse.Namespace) -> int:
    manifest = scan_root(Path(args.root))
    write_json(Path(args.out), manifest)
    return 1 if has_error_diagnostic(manifest) else 0


def command_generate(args: argparse.Namespace) -> int:
    try:
        manifest = load_manifest(Path(args.manifest))
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(json.dumps({"error": str(exc)}), file=sys.stderr)
        return 1

    if has_error_diagnostic(manifest):
        print(
            json.dumps(
                {
                    "error": "Manifest contains error diagnostics; generation refused.",
                    "diagnostics": manifest.get("diagnostics", []),
                },
                indent=2,
            ),
            file=sys.stderr,
        )
        return 1

    write_generated_files(manifest, Path(args.out_dir))
    return 0


def command_validate(args: argparse.Namespace) -> int:
    root = Path(args.root)
    manifest = scan_root(root)
    add_generated_drift_diagnostics(root, manifest)
    print(json.dumps(manifest, indent=2))
    return 1 if has_error_diagnostic(manifest) else 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="HuaEngine reflection tool MVP")
    subparsers = parser.add_subparsers(dest="command", required=True)

    scan_parser = subparsers.add_parser("scan", help="scan source files into a manifest")
    scan_parser.add_argument("--root", required=True, help="repository root to scan")
    scan_parser.add_argument("--out", required=True, help="manifest output path")
    scan_parser.set_defaults(func=command_scan)

    generate_parser = subparsers.add_parser(
        "generate", help="generate C++ reflection files from a manifest"
    )
    generate_parser.add_argument("--manifest", required=True, help="manifest input path")
    generate_parser.add_argument("--out-dir", required=True, help="generated output directory")
    generate_parser.set_defaults(func=command_generate)

    validate_parser = subparsers.add_parser(
        "validate", help="scan source files and print validation JSON"
    )
    validate_parser.add_argument("--root", required=True, help="repository root to scan")
    validate_parser.set_defaults(func=command_validate)

    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
