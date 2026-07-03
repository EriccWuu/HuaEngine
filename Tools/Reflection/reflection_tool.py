#!/usr/bin/env python3
"""MVP source scanner and generator for HuaEngine reflection metadata."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


SCHEMA_VERSION = 1
SOURCE_EXTENSIONS = {".h", ".hpp", ".hh", ".hxx", ".cpp", ".cc", ".cxx"}
SKIPPED_DIRS = {
    ".git",
    ".vs",
    ".workspace",
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


def find_type_declaration(text: str, offset: int, expected_name: str) -> Optional[Dict[str, Any]]:
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
            "body_start": brace_offset + 1,
            "body_end": end_brace,
        }
    return None


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
                    "warning",
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
        if not args:
            diagnostics.append(
                make_diagnostic(
                    "error",
                    "component.missing_type",
                    "HE_REFLECT_COMPONENT requires a type name as its first argument.",
                    source,
                    line,
                )
            )
            continue

        qualified_name = args[0].strip()
        metadata = parse_metadata(args[1:])
        declaration = find_type_declaration(text, macro_end, qualified_name)
        if declaration is None:
            diagnostics.append(
                make_diagnostic(
                    "error",
                    "component.declaration_not_found",
                    f"Could not find a class or struct declaration for '{qualified_name}' after HE_REFLECT_COMPONENT.",
                    source,
                    line,
                )
            )
            continue

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
    return {
        "schema_version": SCHEMA_VERSION,
        "types": types,
        "diagnostics": diagnostics,
    }


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


def write_generated_files(manifest: Dict[str, Any], out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    header_path = out_dir / "GeneratedReflection.h"
    source_path = out_dir / "GeneratedReflection.cpp"

    header = """#pragma once

#include <cstddef>

namespace HE::Reflection::Generated {

struct GeneratedFieldInfo {
    const char* Name;
    const char* Type;
    const char* DisplayName;
    const char* Category;
};

struct GeneratedTypeInfo {
    const char* Name;
    const char* QualifiedName;
    const char* DisplayName;
    const char* Category;
    const char* Source;
    const GeneratedFieldInfo* Fields;
    std::size_t FieldCount;
};

const GeneratedTypeInfo* GetGeneratedReflectionTypes();
std::size_t GetGeneratedReflectionTypeCount();

} // namespace HE::Reflection::Generated
"""

    lines = [
        '#include "GeneratedReflection.h"',
        "",
        "namespace HE::Reflection::Generated {",
        "",
    ]

    for type_index, reflected_type in enumerate(manifest.get("types", [])):
        fields = reflected_type.get("fields", [])
        if fields:
            lines.append(f"static constexpr GeneratedFieldInfo Type{type_index}Fields[] = {{")
            for field in fields:
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cpp_string(field.get("name", "")),
                            cpp_string(field.get("type", "")),
                            cpp_string(field.get("display_name", "")),
                            cpp_string(field.get("category", "")),
                        ]
                    )
                    + "},"
                )
            lines.append("};")
            lines.append("")

    manifest_types = manifest.get("types", [])
    if manifest_types:
        lines.append("static constexpr GeneratedTypeInfo Types[] = {")
        for type_index, reflected_type in enumerate(manifest_types):
            field_count = len(reflected_type.get("fields", []))
            field_pointer = f"Type{type_index}Fields" if field_count else "nullptr"
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cpp_string(reflected_type.get("name", "")),
                        cpp_string(reflected_type.get("qualified_name", "")),
                        cpp_string(reflected_type.get("display_name", "")),
                        cpp_string(reflected_type.get("category", "")),
                        cpp_string(reflected_type.get("source", "")),
                        field_pointer,
                        str(field_count),
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")
    lines.append("const GeneratedTypeInfo* GetGeneratedReflectionTypes() {")
    if manifest_types:
        lines.append("    return Types;")
    else:
        lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("std::size_t GetGeneratedReflectionTypeCount() {")
    if manifest_types:
        lines.append("    return sizeof(Types) / sizeof(Types[0]);")
    else:
        lines.append("    return 0;")
    lines.append("}")
    lines.append("")
    lines.append("} // namespace HE::Reflection::Generated")
    lines.append("")

    header_path.write_text(header, encoding="utf-8")
    source_path.write_text("\n".join(lines), encoding="utf-8")


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
    manifest = scan_root(Path(args.root))
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
