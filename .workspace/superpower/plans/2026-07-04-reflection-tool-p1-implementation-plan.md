# HuaEngine Reflection Tool P1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立 Reflection Tool P1 闭环：空宏标记组件字段，Python 扫描/生成/校验，生成 C++ 反射与组件注册代码，并通过正式 operation 与 CLI 暴露。

**Architecture:** 源码用 `HE_REFLECT_COMPONENT` / `HE_REFLECT_FIELD` 作为唯一生成源；`Tools/Reflection/reflection_tool.py` 输出 JSON manifest 并生成 `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.*`；引擎通过 generated manifest 注册核心组件；CLI 通过 `ApplicationOperations` 调用 reflection service，不直接执行散落逻辑。

**Tech Stack:** Python 3 标准库、C++20、CMake、HuaEngine `ApplicationOperations` / `ResultEnvelope` / `ComponentRegistry`、Windows smoke executables。

---

## File Structure

- Create: `Tools/Reflection/reflection_tool.py`
  - Python CLI，支持 `scan`、`generate`、`validate`。
- Create: `HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h`
  - 定义空宏 `HE_REFLECT_COMPONENT(...)` / `HE_REFLECT_FIELD(...)`。
- Modify: `HuaEngine/src/HuaEngine/ECS/Components.h`
  - 标记 `NameComponent`、`TransformComponent`，移除对应手写 `srefl_class`。
- Modify: `HuaEngine/src/Module/Rendering/RenderingComponent.h`
  - 标记 `CameraComponent`、`MeshComponent`、`MaterialComponent`，移除对应手写 `srefl_class`。
- Create: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- Create: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
  - checked-in generated files，包含 generated `srefl_class`、manifest 查询和 `RegisterGeneratedComponents`。
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`
  - `RegisterCoreComponents` 调用 generated registration。
- Create: `HuaEngine/src/HuaEngine/Reflection/ReflectionToolService.h`
- Create: `HuaEngine/src/HuaEngine/Reflection/ReflectionToolService.cpp`
  - C++ operation 层封装 Python 工具执行和 manifest 结果摘要。
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationServices.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
  - 新增 reflection scan/generate/validate 正式 operation。
- Modify: `CLI/src/CLICommandCatalog.cpp`
- Create: `CLI/src/CLIReflectionCommands.h`
- Create: `CLI/src/CLIReflectionCommands.cpp`
- Modify: `CLI/src/CLICommandRunner.cpp`
- Modify: `CLI/CMakeLists.txt`
  - 新增 CLI reflection command handler。
- Create: `Tests/ReflectionToolSmoke.cpp`
- Create: `Tests/ReflectionGeneratedSmoke.cpp`
- Create: `Tests/CLIReflectionSmoke.cpp`
- Modify: `CMakeLists.txt`
  - 新增 smoke targets。
- Modify: `docs/development-guidelines.md` or `docs/huaengine-cli.md`
  - 补充 reflection tool 使用说明；文档主语言中文。

---

### Task 1: Python Reflection Tool MVP

**Files:**
- Create: `Tools/Reflection/reflection_tool.py`

- [ ] **Step 1: Implement scanner data model and command shell**

Create `Tools/Reflection/reflection_tool.py` with standard-library-only code:

```python
#!/usr/bin/env python3
import argparse
import json
import re
import sys
from dataclasses import dataclass, asdict
from pathlib import Path

SCHEMA_VERSION = 1
SUPPORTED_EXTENSIONS = {".h", ".hpp", ".cpp"}

@dataclass
class Diagnostic:
    severity: str
    code: str
    message: str
    source: str = ""

@dataclass
class ReflectedField:
    name: str
    type: str

@dataclass
class ReflectedType:
    name: str
    qualified_name: str
    kind: str
    display_name: str
    category: str
    source: str
    fields: list[ReflectedField]

def parse_marker_arguments(text: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for key, value in re.findall(r'([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*"([^"]*)"', text):
        result[key] = value
    return result

def strip_initializer(declaration: str) -> str:
    return declaration.split("=", 1)[0].strip()

def parse_field_declaration(line: str) -> tuple[str, str] | None:
    declaration = strip_initializer(line.strip())
    if not declaration.endswith(";"):
        return None
    declaration = declaration[:-1].strip()
    if "(" in declaration or ")" in declaration:
        return None
    parts = declaration.rsplit(None, 1)
    if len(parts) != 2:
        return None
    field_type, field_name = parts
    field_name = field_name.strip()
    if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", field_name):
        return None
    return field_type.strip(), field_name

def find_namespace(lines: list[str], line_index: int) -> str:
    namespace = ""
    for index in range(0, line_index + 1):
        match = re.match(r"\\s*namespace\\s+([A-Za-z_][A-Za-z0-9_:]*)\\s*\\{", lines[index])
        if match:
            namespace = match.group(1)
    return namespace

def scan_file(path: Path, root: Path) -> tuple[list[ReflectedType], list[Diagnostic]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    types: list[ReflectedType] = []
    diagnostics: list[Diagnostic] = []
    index = 0
    while index < len(lines):
        line = lines[index]
        marker = re.search(r"HE_REFLECT_COMPONENT\\((.*)\\)", line)
        if not marker:
            index += 1
            continue
        args = parse_marker_arguments(marker.group(1))
        display_name = args.get("DisplayName", "")
        category = args.get("Category", "")
        source = path.relative_to(root).as_posix()
        if not display_name:
            diagnostics.append(Diagnostic("error", "reflection.component.missing_display_name", "Component marker requires DisplayName", source))
        if not category:
            diagnostics.append(Diagnostic("error", "reflection.component.missing_category", "Component marker requires Category", source))
        type_line_index = index + 1
        while type_line_index < len(lines) and not lines[type_line_index].strip():
            type_line_index += 1
        if type_line_index >= len(lines):
            diagnostics.append(Diagnostic("error", "reflection.component.missing_type", "Component marker is not followed by a type declaration", source))
            break
        type_match = re.match(r"\\s*(struct|class)\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*(?::[^\\{]+)?\\{", lines[type_line_index])
        if not type_match:
            diagnostics.append(Diagnostic("error", "reflection.component.invalid_type", "Component marker must be followed by a simple struct/class declaration", source))
            index += 1
            continue
        type_name = type_match.group(2)
        namespace = find_namespace(lines, type_line_index)
        qualified_name = f"{namespace}::{type_name}" if namespace else f"HE::{type_name}"
        fields: list[ReflectedField] = []
        body_index = type_line_index + 1
        while body_index < len(lines):
            if re.match(r"\\s*};", lines[body_index]):
                break
            field_marker = re.search(r"HE_REFLECT_FIELD\\(", lines[body_index])
            if field_marker:
                declaration_index = body_index + 1
                while declaration_index < len(lines) and not lines[declaration_index].strip():
                    declaration_index += 1
                parsed = parse_field_declaration(lines[declaration_index]) if declaration_index < len(lines) else None
                if not parsed:
                    diagnostics.append(Diagnostic("error", "reflection.field.invalid_declaration", f"Field marker in {type_name} is not followed by a supported field declaration", source))
                else:
                    field_type, field_name = parsed
                    fields.append(ReflectedField(field_name, field_type))
                body_index = declaration_index
            body_index += 1
        if not fields:
            diagnostics.append(Diagnostic("error", "reflection.component.empty", f"{type_name} must expose at least one reflected field", source))
        types.append(ReflectedType(type_name, qualified_name, "component", display_name, category, source, fields))
        index = body_index + 1
    return types, diagnostics

def scan_root(root: Path) -> dict:
    all_types: list[ReflectedType] = []
    diagnostics: list[Diagnostic] = []
    seen: set[str] = set()
    for path in sorted(root.rglob("*")):
        if path.suffix not in SUPPORTED_EXTENSIONS:
            continue
        if any(part in {".git", "build", "Dependencies"} for part in path.parts):
            continue
        types, file_diagnostics = scan_file(path, root)
        diagnostics.extend(file_diagnostics)
        for reflected_type in types:
            if reflected_type.qualified_name in seen:
                diagnostics.append(Diagnostic("error", "reflection.type.duplicate", f"Duplicate reflected type {reflected_type.qualified_name}", reflected_type.source))
                continue
            seen.add(reflected_type.qualified_name)
            all_types.append(reflected_type)
    return {
        "schema_version": SCHEMA_VERSION,
        "types": [
            {**asdict(reflected_type), "fields": [asdict(field) for field in reflected_type.fields]}
            for reflected_type in all_types
        ],
        "diagnostics": [asdict(diagnostic) for diagnostic in diagnostics],
    }
```

- [ ] **Step 2: Implement generation and validation**

In the same file add:

```python
def has_errors(manifest: dict) -> bool:
    return any(item.get("severity") == "error" for item in manifest.get("diagnostics", []))

def write_manifest(manifest: dict, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\\n", encoding="utf-8")

def load_manifest(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))

def generated_header() -> str:
    return """#pragma once

#include <span>
#include <string_view>

#include "HuaEngine/ECS/ComponentRegistry.h"

namespace HE::Generated {
    struct ReflectedFieldInfo {
        std::string_view Name;
        std::string_view Type;
    };

    struct ReflectedTypeInfo {
        std::string_view Name;
        std::string_view QualifiedName;
        std::string_view Kind;
        std::string_view DisplayName;
        std::string_view Category;
        std::span<const ReflectedFieldInfo> Fields;
    };

    std::span<const ReflectedTypeInfo> GetReflectedTypes();
    const ReflectedTypeInfo* FindReflectedType(std::string_view qualifiedName);
    void RegisterGeneratedComponents(ComponentRegistry& registry);
}
"""

def cpp_string(value: str) -> str:
    return json.dumps(value)

def generate_cpp(manifest: dict) -> str:
    includes = [
        '#include "HuaEngine/Generated/GeneratedReflection.h"',
        '#include "HuaEngine/ECS/Components.h"',
        '#include "Module/Rendering/RenderingComponent.h"',
        '#include "HuaEngine/Reflection/Reflection.h"',
        "",
    ]
    reflected_types = manifest.get("types", [])
    srefl_blocks: list[str] = []
    arrays: list[str] = []
    type_entries: list[str] = []
    for index, item in enumerate(reflected_types):
        qualified = item["qualified_name"]
        fields = item.get("fields", [])
        srefl_blocks.append(f"srefl_class({qualified},\\n\\tfields(\\n" + ",\\n".join(f"\\t\\tfield({field['name']})" for field in fields) + "\\n\\t)\\n)")
        field_array_name = f"s_Fields_{item['name']}"
        arrays.append(f"static constexpr ReflectedFieldInfo {field_array_name}[] = {{" + "".join(f"\\n\\t{{ {cpp_string(field['name'])}, {cpp_string(field['type'])} }}," for field in fields) + "\\n};")
        type_entries.append(f"\\t{{ {cpp_string(item['name'])}, {cpp_string(qualified)}, {cpp_string(item['kind'])}, {cpp_string(item['display_name'])}, {cpp_string(item['category'])}, {field_array_name} }},")
    registration_lines = []
    for item in reflected_types:
        if item.get("kind") == "component":
            registration_lines.append(f"\\tregistry.Register<{item['qualified_name']}>({{ .TypeName = {cpp_string(item['name'])}, .DisplayName = {cpp_string(item['display_name'])}, .Category = {cpp_string(item['category'])} }});")
    return "\\n".join(includes + srefl_blocks + ["", "namespace HE::Generated {", *arrays, "static constexpr ReflectedTypeInfo s_Types[] = {", *type_entries, "};", "std::span<const ReflectedTypeInfo> GetReflectedTypes() { return s_Types; }", "const ReflectedTypeInfo* FindReflectedType(std::string_view qualifiedName) { for (const auto& type : s_Types) { if (type.QualifiedName == qualifiedName) { return &type; } } return nullptr; }", "void RegisterGeneratedComponents(ComponentRegistry& registry) {", *registration_lines, "}", "}"]) + "\\n"

def generate_files(manifest: dict, out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "GeneratedReflection.h").write_text(generated_header(), encoding="utf-8")
    (out_dir / "GeneratedReflection.cpp").write_text(generate_cpp(manifest), encoding="utf-8")
```

Then add `argparse` handlers:

```python
def command_scan(args: argparse.Namespace) -> int:
    manifest = scan_root(Path(args.root).resolve())
    write_manifest(manifest, Path(args.out))
    return 1 if has_errors(manifest) else 0

def command_generate(args: argparse.Namespace) -> int:
    manifest = load_manifest(Path(args.manifest))
    if has_errors(manifest):
        print("Manifest contains errors; refusing to generate", file=sys.stderr)
        return 1
    generate_files(manifest, Path(args.out_dir))
    return 0

def command_validate(args: argparse.Namespace) -> int:
    manifest = scan_root(Path(args.root).resolve())
    print(json.dumps(manifest, indent=2, ensure_ascii=False))
    return 1 if has_errors(manifest) else 0

def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="HuaEngine reflection tool")
    subparsers = parser.add_subparsers(dest="command", required=True)
    scan = subparsers.add_parser("scan")
    scan.add_argument("--root", required=True)
    scan.add_argument("--out", required=True)
    scan.set_defaults(func=command_scan)
    generate = subparsers.add_parser("generate")
    generate.add_argument("--manifest", required=True)
    generate.add_argument("--out-dir", required=True)
    generate.set_defaults(func=command_generate)
    validate = subparsers.add_parser("validate")
    validate.add_argument("--root", required=True)
    validate.set_defaults(func=command_validate)
    args = parser.parse_args(argv)
    return args.func(args)

if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
```

- [ ] **Step 3: Run scanner on current unmarked code**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
```

Expected before Task 2: success with zero reflected types because components are not marked yet. The command must not crash.

- [ ] **Step 4: Commit Python MVP**

```powershell
git add Tools/Reflection/reflection_tool.py
git commit -m "feat(reflection): add python reflection tool"
```

---

### Task 2: Add Markers, Annotate Core Components, and Generate Files

**Files:**
- Create: `HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h`
- Modify: `HuaEngine/src/HuaEngine/ECS/Components.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- Create: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- Create: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`

- [ ] **Step 1: Add marker header**

Create `HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h`:

```cpp
#pragma once

#define HE_REFLECT_COMPONENT(...)
#define HE_REFLECT_FIELD(...)
```

- [ ] **Step 2: Annotate core ECS components**

Modify `HuaEngine/src/HuaEngine/ECS/Components.h`:

```cpp
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Reflection/ReflectionMarkers.h"
```

Place markers directly before the two target component structs and fields:

```cpp
HE_REFLECT_COMPONENT(DisplayName="Name", Category="Core")
struct NameComponent : Component {
    HE_REFLECT_FIELD()
    std::string Name = "Entity";
    ...
};

HE_REFLECT_COMPONENT(DisplayName="Transform", Category="Core")
struct TransformComponent : Component {
    HE_REFLECT_FIELD()
    glm::vec3 Position = {0.0f, 0.0f, 0.0f};
    HE_REFLECT_FIELD()
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    HE_REFLECT_FIELD()
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};
    ...
};
```

Remove the hand-written `srefl_class(NameComponent, ...)` and `srefl_class(TransformComponent, ...)` blocks at the bottom of the file.

- [ ] **Step 3: Annotate rendering components**

Modify `HuaEngine/src/Module/Rendering/RenderingComponent.h`:

```cpp
#include "HuaEngine/Reflection/ReflectionMarkers.h"
```

Add markers:

```cpp
HE_REFLECT_COMPONENT(DisplayName="Camera", Category="Rendering")
struct CameraComponent : Component {
    HE_REFLECT_FIELD()
    bool Primary = true;
    HE_REFLECT_FIELD()
    bool FixedAspectRatio = false;
};

HE_REFLECT_COMPONENT(DisplayName="Material", Category="Rendering")
struct MaterialComponent : Component {
    HE_REFLECT_FIELD()
    Ref<HE::Rendering::MaterialInstance> MaterialInstance;
};

HE_REFLECT_COMPONENT(DisplayName="Mesh", Category="Rendering")
struct MeshComponent : Component {
    HE_REFLECT_FIELD()
    std::string MeshAssetName;
    Ref<VertexArray> m_CachedVertexArray;
};
```

Do not mark `Camera`, `m_CachedVertexArray`, `RendererComponent`, or `NativeScriptComponent`. Remove hand-written `srefl_class` blocks for `CameraComponent`, `MaterialComponent`, and `MeshComponent`.

- [ ] **Step 4: Generate checked-in files**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
```

Expected manifest has five types:

```text
HE::NameComponent
HE::TransformComponent
HE::Rendering::CameraComponent
HE::Rendering::MaterialComponent
HE::Rendering::MeshComponent
```

- [ ] **Step 5: Build reflection smoke**

Run:

```powershell
cmake --build build --config Debug --target ReflectionSmoke
& .\build\bin\Debug-Windows-x64\smoke\ReflectionSmoke.exe
```

Expected: `ReflectionSmoke passed`. If the target does not pick up generated files due to stale CMake glob, run:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
```

then rerun the build command.

- [ ] **Step 6: Commit markers and generated files**

```powershell
git add HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h HuaEngine/src/HuaEngine/ECS/Components.h HuaEngine/src/Module/Rendering/RenderingComponent.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp
git commit -m "feat(reflection): generate core component reflection"
```

---

### Task 3: Use Generated Component Registration

**Files:**
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`
- Create: `Tests/ReflectionGeneratedSmoke.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Switch core registration to generated registration**

Modify `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`:

```cpp
#include "HuaEngine/Generated/GeneratedReflection.h"
```

Replace the body of `RegisterCoreComponents` with:

```cpp
void RegisterCoreComponents(ComponentRegistry& registry) {
    Generated::RegisterGeneratedComponents(registry);
}
```

- [ ] **Step 2: Add generated reflection smoke**

Create `Tests/ReflectionGeneratedSmoke.cpp`:

```cpp
#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/Generated/GeneratedReflection.h"

namespace {
    void Require(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "[ReflectionGeneratedSmoke] " << message << std::endl;
            std::exit(1);
        }
    }

    bool HasField(const HE::Generated::ReflectedTypeInfo& type, std::string_view fieldName) {
        for (const auto& field : type.Fields) {
            if (field.Name == fieldName) {
                return true;
            }
        }
        return false;
    }
}

int main() {
    const auto types = HE::Generated::GetReflectedTypes();
    Require(types.size() == 5, "Expected five generated reflected component types");

    const auto* transform = HE::Generated::FindReflectedType("HE::TransformComponent");
    Require(transform != nullptr, "Expected TransformComponent generated metadata");
    Require(HasField(*transform, "Position"), "Expected TransformComponent Position field");
    Require(HasField(*transform, "Rotation"), "Expected TransformComponent Rotation field");
    Require(HasField(*transform, "Scale"), "Expected TransformComponent Scale field");

    const auto* mesh = HE::Generated::FindReflectedType("HE::Rendering::MeshComponent");
    Require(mesh != nullptr, "Expected MeshComponent generated metadata");
    Require(HasField(*mesh, "MeshAssetName"), "Expected MeshComponent MeshAssetName field");
    Require(!HasField(*mesh, "m_CachedVertexArray"), "Runtime mesh cache must not be reflected");

    HE::ComponentRegistry registry;
    HE::Generated::RegisterGeneratedComponents(registry);
    Require(registry.FindByName("NameComponent") != nullptr, "Expected NameComponent registration");
    Require(registry.FindByName("TransformComponent") != nullptr, "Expected TransformComponent registration");
    Require(registry.FindByName("CameraComponent") != nullptr, "Expected CameraComponent registration");
    Require(registry.FindByName("MeshComponent") != nullptr, "Expected MeshComponent registration");
    Require(registry.FindByName("MaterialComponent") != nullptr, "Expected MaterialComponent registration");

    std::cout << "ReflectionGeneratedSmoke passed" << std::endl;
    return 0;
}
```

- [ ] **Step 3: Add CMake target**

Add target near `ReflectionSmoke` in root `CMakeLists.txt`:

```cmake
add_executable(ReflectionGeneratedSmoke Tests/ReflectionGeneratedSmoke.cpp)
target_include_directories(ReflectionGeneratedSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ReflectionGeneratedSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ReflectionGeneratedSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ReflectionGeneratedSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ReflectionGeneratedSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ReflectionGeneratedSmoke)
```

Add folder property:

```cmake
set_property(TARGET ReflectionGeneratedSmoke PROPERTY FOLDER "Tests")
```

- [ ] **Step 4: Run generated smoke and registry regressions**

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
& .\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
cmake --build build --config Debug --target ECSCoreSmoke
& .\build\bin\Debug-Windows-x64\smoke\ECSCoreSmoke.exe
```

Expected: both pass.

- [ ] **Step 5: Commit generated registration**

```powershell
git add HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp Tests/ReflectionGeneratedSmoke.cpp CMakeLists.txt
git commit -m "feat(reflection): register generated components"
```

---

### Task 4: Reflection Operations and CLI Commands

**Files:**
- Create: `HuaEngine/src/HuaEngine/Reflection/ReflectionToolService.h`
- Create: `HuaEngine/src/HuaEngine/Reflection/ReflectionToolService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationServices.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `CLI/src/CLICommandCatalog.cpp`
- Create: `CLI/src/CLIReflectionCommands.h`
- Create: `CLI/src/CLIReflectionCommands.cpp`
- Modify: `CLI/src/CLICommandRunner.cpp`
- Modify: `CLI/CMakeLists.txt`

- [ ] **Step 1: Add service request/response API**

Create `HuaEngine/src/HuaEngine/Reflection/ReflectionToolService.h`:

```cpp
#pragma once

#include <filesystem>
#include <string>

#include "HuaEngine/Core/ResultEnvelope.h"

namespace HE {
    struct ReflectionToolRequest {
        std::filesystem::path RootPath;
        std::filesystem::path ManifestPath;
        std::filesystem::path OutputDirectory;
    };

    class ReflectionToolService {
    public:
        [[nodiscard]] ResultEnvelope Scan(const ReflectionToolRequest& request) const;
        [[nodiscard]] ResultEnvelope Generate(const ReflectionToolRequest& request) const;
        [[nodiscard]] ResultEnvelope Validate(const ReflectionToolRequest& request) const;

    private:
        [[nodiscard]] ResultEnvelope RunTool(const std::string& operation, const ReflectionToolRequest& request, const std::string& command) const;
    };
}
```

Implement `ReflectionToolService.cpp` using `_popen` on Windows to run Python:

```cpp
#include "enginepch.h"
#include "ReflectionToolService.h"

#include <array>
#include <cstdio>
#include <sstream>

namespace HE {
    namespace {
        std::string Quote(const std::filesystem::path& path) {
            return "\"" + path.string() + "\"";
        }

        std::filesystem::path DefaultManifestPath(const std::filesystem::path& root) {
            return root / ".workspace" / "reflection" / "reflection_manifest.json";
        }
    }

    ResultEnvelope ReflectionToolService::Scan(const ReflectionToolRequest& request) const {
        const auto manifest = request.ManifestPath.empty() ? DefaultManifestPath(request.RootPath) : request.ManifestPath;
        return RunTool("reflection.scan", request, "python Tools/Reflection/reflection_tool.py scan --root " + Quote(request.RootPath) + " --out " + Quote(manifest));
    }

    ResultEnvelope ReflectionToolService::Generate(const ReflectionToolRequest& request) const {
        const auto manifest = request.ManifestPath.empty() ? DefaultManifestPath(request.RootPath) : request.ManifestPath;
        return RunTool("reflection.generate", request, "python Tools/Reflection/reflection_tool.py scan --root " + Quote(request.RootPath) + " --out " + Quote(manifest) + " && python Tools/Reflection/reflection_tool.py generate --manifest " + Quote(manifest) + " --out-dir " + Quote(request.OutputDirectory));
    }

    ResultEnvelope ReflectionToolService::Validate(const ReflectionToolRequest& request) const {
        return RunTool("reflection.validate", request, "python Tools/Reflection/reflection_tool.py validate --root " + Quote(request.RootPath));
    }
}
```

Complete `RunTool` with `_popen`, capture stdout, call `_pclose`, return `Success` on exit code 0 and `Failure` otherwise. Set payload keys `root`, `tool_output`, and for success `reflected_type_count` by counting `"qualified_name"` in output. If `_popen` fails, return failure with detail `reflection.tool.launch_failed`.

- [ ] **Step 2: Wire service into ApplicationServices and ApplicationOperations**

Add `ReflectionToolService m_ReflectionTools;` and accessor `ReflectionToolService& ReflectionTools();` to `ApplicationServices`.

Add to `ApplicationOperations.h`:

```cpp
[[nodiscard]] ResultEnvelope ScanReflection(const ReflectionToolRequest& request) const;
[[nodiscard]] ResultEnvelope GenerateReflection(const ReflectionToolRequest& request) const;
[[nodiscard]] ResultEnvelope ValidateReflection(const ReflectionToolRequest& request) const;
```

Register operations:

```cpp
m_Registry.Register({ "reflection.scan", OperationDomain::Validation, "Scan reflection markers and emit a manifest" });
m_Registry.Register({ "reflection.generate", OperationDomain::Validation, "Generate C++ reflection files from markers" });
m_Registry.Register({ "reflection.validate", OperationDomain::Validation, "Validate reflection markers and generated files" });
```

Implement methods by forwarding to `m_Services->ReflectionTools()`.

- [ ] **Step 3: Add CLI catalog entries and handler**

Add entries to `CLI/src/CLICommandCatalog.cpp`:

```cpp
Register({ { "reflection", "scan" }, CLICommandDomain::Validation, "reflection.scan", "Scan reflection markers.", "reflection scan --root <path> [--out <manifest>]", { ValueOption("--root", "Repository root.", true), ValueOption("--out", "Manifest output path.") } });
Register({ { "reflection", "generate" }, CLICommandDomain::Validation, "reflection.generate", "Generate reflection sources.", "reflection generate --root <path> --out-dir <path>", { ValueOption("--root", "Repository root.", true), ValueOption("--out-dir", "Generated output directory.", true) } });
Register({ { "reflection", "validate" }, CLICommandDomain::Validation, "reflection.validate", "Validate reflection markers.", "reflection validate --root <path>", { ValueOption("--root", "Repository root.", true) } });
```

Create `CLI/src/CLIReflectionCommands.h/.cpp` with `RunReflectionCommand(...)`. Build `ReflectionToolRequest` from parsed options and call the corresponding `ApplicationOperations` method.

Modify runner switch to dispatch `reflection` commands before generic validation or by adding `CLICommandDomain::Reflection`. If adding a new enum value, update `CLICommandCatalog.h` and `CLICommandRunner.cpp`.

Update `CLI/CMakeLists.txt` to include new handler files.

- [ ] **Step 4: Build CLI**

Run:

```powershell
cmake --build build --config Debug --target HuaEngineCLI
```

Expected: build passes.

- [ ] **Step 5: Commit operation and CLI integration**

```powershell
git add HuaEngine/src/HuaEngine/Reflection/ReflectionToolService.* HuaEngine/src/HuaEngine/Application/ApplicationServices.h HuaEngine/src/HuaEngine/Application/ApplicationOperations.* CLI/src/CLICommandCatalog.* CLI/src/CLICommandRunner.cpp CLI/src/CLIReflectionCommands.* CLI/CMakeLists.txt
git commit -m "feat(reflection): add reflection operations and cli commands"
```

---

### Task 5: Reflection Tool and CLI Smokes

**Files:**
- Create: `Tests/ReflectionToolSmoke.cpp`
- Create: `Tests/CLIReflectionSmoke.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/huaengine-cli.md`

- [ ] **Step 1: Add ReflectionToolSmoke**

Create `Tests/ReflectionToolSmoke.cpp` as a small process smoke:

```cpp
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
    void Require(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "[ReflectionToolSmoke] " << message << std::endl;
            std::exit(1);
        }
    }
}

int main() {
    const auto manifest = std::filesystem::path(".workspace") / "reflection" / "reflection_tool_smoke_manifest.json";
    const int scanResult = std::system(("python Tools/Reflection/reflection_tool.py scan --root . --out " + manifest.string()).c_str());
    Require(scanResult == 0, "reflection scan should succeed");
    Require(std::filesystem::exists(manifest), "manifest should be written");
    const int validateResult = std::system("python Tools/Reflection/reflection_tool.py validate --root . > .workspace/reflection/reflection_tool_validate.json");
    Require(validateResult == 0, "reflection validate should succeed");
    std::cout << "ReflectionToolSmoke passed" << std::endl;
    return 0;
}
```

- [ ] **Step 2: Add CLIReflectionSmoke**

Follow `Tests/CLIContractSmoke.cpp` helper style. Run:

```text
HuaEngineCLI.exe reflection scan --root <repo>
HuaEngineCLI.exe reflection validate --root <repo>
```

Assert:

- exit code 0
- `"operation":"reflection.scan"` / `"operation":"reflection.validate"`
- `"status":"success"`
- `"reflected_type_count":"5"` appears in payload

- [ ] **Step 3: Add CMake targets**

Add `ReflectionToolSmoke` linked to `HuaEngine` and `CLIReflectionSmoke` depending on `HuaEngineCLI`, using existing smoke target patterns. Call `copy_cli_host_to_smoke(CLIReflectionSmoke)`.

- [ ] **Step 4: Update CLI docs**

Add Chinese section in `docs/huaengine-cli.md`:

```markdown
## Reflection 命令

`reflection scan`、`reflection generate`、`reflection validate` 用于自动化检查和生成反射元数据。CLI 只调用正式 `ApplicationOperations`，具体扫描和生成逻辑由 Reflection Tool 服务执行。
```

- [ ] **Step 5: Run new smokes**

Run:

```powershell
cmake --build build --config Debug --target ReflectionToolSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target CLIReflectionSmoke
& .\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIReflectionSmoke.exe
```

Expected: all pass.

- [ ] **Step 6: Commit smokes and docs**

```powershell
git add Tests/ReflectionToolSmoke.cpp Tests/CLIReflectionSmoke.cpp CMakeLists.txt docs/huaengine-cli.md
git commit -m "test(reflection): add tool and cli smokes"
```

---

### Task 6: Full Verification and Cleanup

**Files:**
- Modify only if verification exposes small issues in files touched by Tasks 1-5.

- [ ] **Step 1: Regenerate and validate**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
git diff -- HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp
```

Expected: validate exits 0; generated files have no diff.

- [ ] **Step 2: Run regression smoke set**

Run:

```powershell
cmake --build build --config Debug --target ReflectionSmoke
cmake --build build --config Debug --target SerializationSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target CLIContractSmoke
cmake --build build --config Debug --target CLIHostSmoke
& .\build\bin\Debug-Windows-x64\smoke\ReflectionSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\SerializationSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIContractSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIHostSmoke.exe
```

Expected: all pass.

- [ ] **Step 3: Run reflection smoke set**

Run:

```powershell
cmake --build build --config Debug --target ReflectionToolSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target CLIReflectionSmoke
& .\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIReflectionSmoke.exe
```

Expected: all pass.

- [ ] **Step 4: Check status**

Run:

```powershell
git status --short --branch
git diff --stat HEAD
```

Expected: no tracked diff; only pre-existing untracked `.workspace/cli-p0-handoff.md` and `.workspace/engine_roadmap.md` may remain.

- [ ] **Step 5: Commit verification fixes only if needed**

If verification required fixes, add the exact files changed by those fixes and commit:

```powershell
git status --short
git add path\\to\\fixed_file_1 path\\to\\fixed_file_2
git commit -m "fix(reflection): address p1 verification issues"
```

If no fixes were required, do not create an empty commit.

---

## Self-Review Notes

- Spec coverage: tasks cover marker macros, Python scan/generate/validate, checked-in generated files, generated component registration, reflection operations, CLI commands, tool/generated/CLI smokes, and regression validation.
- Scope: P1 migrates only the five agreed core components and does not migrate `NativeScriptComponent` or deprecated `RendererComponent`.
- CLI boundary: CLI command handlers call `ApplicationOperations`; Python execution is encapsulated in reflection service.
- Build strategy: generated files are checked in and compiled normally, not generated at build time.
