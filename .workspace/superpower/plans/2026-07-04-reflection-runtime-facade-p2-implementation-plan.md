# HuaEngine Reflection Runtime Facade P2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将组件反射统一到 `HE::Refl` runtime facade，让组件序列化不再依赖 `srefl_class`，并在 A/B 后立即整理序列化语义。

**Architecture:** P2-A/B 新增 runtime descriptor，生成器输出 typed serializer，`ComponentRegistry` 从 descriptor 注册组件；P2-C 在同一阶段后续任务中整理字段缺失、未知组件、`Ref<T>`、Scene 输出稳定性等序列化问题。

**Tech Stack:** C++20、Python 3 标准库、CMake/MSBuild、HuaEngine `SerializationBackend`、`ComponentRegistry`、generated reflection tool。

---

## File Structure

- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
  - 增加 runtime descriptor 类型和查询 API 声明，保留 legacy `reflect<T>()`。
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`
  - 增加 `Register(const Refl::TypeDescriptor&)` 或等价 descriptor 注册入口。
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`
  - 实现 descriptor 注册，并继续调用 `Generated::RegisterGeneratedComponents`。
- Modify: `Tools/Reflection/reflection_tool.py`
  - 生成 runtime descriptors、typed serializer、typed deserializer，停止生成 per-source `.generated.h`。
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
  - checked-in generated output。
- Modify: `HuaEngine/src/HuaEngine/ECS/Components.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderingComponent.h`
  - 删除 generated include。
- Modify: `Tests/ReflectionSmoke.cpp`
  - 改为验证 `HE::Refl` runtime descriptor。
- Modify: `Tests/ReflectionGeneratedSmoke.cpp`
  - 验证 descriptor registration 和 generated serializer。
- Modify: `Tests/SerializationSmoke.cpp`
- Modify: `Tests/ECSSceneSerializationSmoke.cpp`
  - 增加组件不依赖 legacy `srefl_class` 的 round-trip 覆盖。
- Create or Modify: `Tests/SerializationPolicySmoke.cpp`
  - P2-C 覆盖字段缺失、未知字段、未知组件、`Ref<T>`、Scene 输出稳定性。
- Modify: `CMakeLists.txt`
  - 如新增 smoke target，加入构建。

---

### Task 1: Runtime Reflection Descriptor API

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`

- [ ] **Step 1: Add failing runtime descriptor assertions**

Update `Tests/ReflectionGeneratedSmoke.cpp` to require runtime reflection API:

```cpp
const auto* transformRuntime = HE::Refl::FindRuntimeType("HE::TransformComponent");
Require(transformRuntime != nullptr, "Expected runtime reflection descriptor for TransformComponent");
Require(transformRuntime->Fields.size() == 3, "Expected TransformComponent runtime fields");
```

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: fail because `FindRuntimeType` / runtime descriptor API does not exist.

- [ ] **Step 2: Add runtime descriptor declarations**

Add descriptor declarations in `HuaEngine/src/HuaEngine/Reflection/Reflection.h`:

```cpp
namespace HE {
    using ComponentTypeId = std::uint64_t;
    class World;
    using EntityId = std::uint32_t;
}

namespace HE::Serialization {
    class SerializationBackend;
}

namespace HE::Refl {
    struct RuntimeFieldDescriptor {
        std::string_view Name;
        std::string_view Type;
        std::string_view DisplayName;
        std::string_view Category;
    };

    struct RuntimeTypeDescriptor {
        std::string_view Name;
        std::string_view QualifiedName;
        std::string_view Kind;
        std::string_view DisplayName;
        std::string_view Category;
        ComponentTypeId TypeId;
        std::span<const RuntimeFieldDescriptor> Fields;
        void* (*ConstructDefault)();
        void (*Destroy)(void*);
        void* (*Copy)(const void*);
        void (*Serialize)(Serialization::SerializationBackend&, const std::string&, const void*);
        bool (*Deserialize)(Serialization::SerializationBackend&, const std::string&, void*);
        void (*AddCopyToWorld)(World&, EntityId, const void*);
    };

    std::span<const RuntimeTypeDescriptor> GetRuntimeTypes();
    const RuntimeTypeDescriptor* FindRuntimeType(std::string_view qualifiedName);
    const RuntimeTypeDescriptor* FindRuntimeType(ComponentTypeId typeId);
}
```

Use the repo’s real `ComponentTypeId`/`EntityId` typedefs instead of duplicating if needed.

- [ ] **Step 3: Run compile check**

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: still fail at link/generator implementation because runtime functions are declared but not defined.

- [ ] **Step 4: Commit descriptor API**

```powershell
git add HuaEngine/src/HuaEngine/Reflection/Reflection.h Tests/ReflectionGeneratedSmoke.cpp
git commit -m "feat(reflection): add runtime descriptor api"
```

---

### Task 2: Generate Runtime Descriptors and Typed Component Serializers

**Files:**
- Modify: `Tools/Reflection/reflection_tool.py`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Delete generated per-source headers under `HuaEngine/src/HuaEngine/Generated/Reflection/`

- [ ] **Step 1: Extend generator output**

Change `write_generated_files()` so generated `.cpp` includes component headers and emits:

```cpp
namespace HE::Generated {
    static void Serialize_HE_TransformComponent(
        Serialization::SerializationBackend& backend,
        const std::string& name,
        const void* object) {
        const auto& component = *static_cast<const HE::TransformComponent*>(object);
        backend.BeginObject(name);
        Serialization::SerializeValue(backend, "Position", component.Position);
        Serialization::SerializeValue(backend, "Rotation", component.Rotation);
        Serialization::SerializeValue(backend, "Scale", component.Scale);
        backend.EndObject();
    }

    static bool Deserialize_HE_TransformComponent(
        Serialization::SerializationBackend& backend,
        const std::string& name,
        void* object) {
        auto& component = *static_cast<HE::TransformComponent*>(object);
        bool success = true;
        backend.BeginObject(name);
        success &= Serialization::DeserializeValue(backend, "Position", component.Position);
        success &= Serialization::DeserializeValue(backend, "Rotation", component.Rotation);
        success &= Serialization::DeserializeValue(backend, "Scale", component.Scale);
        backend.EndObject();
        return success;
    }
}
```

Generate equivalent functions for all five components.

- [ ] **Step 2: Emit runtime descriptor table**

Emit `RuntimeFieldDescriptor` arrays and `RuntimeTypeDescriptor` array in generated `.cpp`.

Each descriptor must contain:

- type metadata
- field metadata
- `ComponentTypeIdOf<T>()`
- `ConstructDefault`
- `Destroy`
- `Copy`
- typed serializer/deserializer
- `AddCopyToWorld`

- [ ] **Step 3: Implement runtime query functions**

Implement:

```cpp
std::span<const Refl::RuntimeTypeDescriptor> Refl::GetRuntimeTypes();
const Refl::RuntimeTypeDescriptor* Refl::FindRuntimeType(std::string_view qualifiedName);
const Refl::RuntimeTypeDescriptor* Refl::FindRuntimeType(ComponentTypeId typeId);
```

These may live in generated `.cpp` for now.

- [ ] **Step 4: Stop generating per-source headers**

Remove per-source `.generated.h` generation from `reflection_tool.py`.

Update generated drift validation so obsolete `Generated/Reflection/*.generated.h` files are reported as drift.

- [ ] **Step 5: Regenerate**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
```

Expected:

- `GeneratedReflection.h/.cpp` updated.
- `HuaEngine/src/HuaEngine/Generated/Reflection/*.generated.h` removed.

- [ ] **Step 6: Commit generator runtime output**

```powershell
git add Tools/Reflection/reflection_tool.py HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp
git rm HuaEngine/src/HuaEngine/Generated/Reflection/HuaEngine_ECS_Components.generated.h HuaEngine/src/HuaEngine/Generated/Reflection/Module_Rendering_RenderingComponent.generated.h
git commit -m "feat(reflection): generate runtime descriptors"
```

---

### Task 3: Register Components from Runtime Descriptors

**Files:**
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`

- [ ] **Step 1: Add failing descriptor registration assertion**

In `Tests/ReflectionGeneratedSmoke.cpp`, after registration:

```cpp
const auto* transformMetadata = registry.FindByName("TransformComponent");
Require(transformMetadata != nullptr, "Expected TransformComponent registry metadata");
Require(transformMetadata->Serialize != nullptr, "Expected generated serializer");
Require(transformMetadata->Deserialize != nullptr, "Expected generated deserializer");
```

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: fail until registry accepts runtime descriptor serializer.

- [ ] **Step 2: Add descriptor registration overload**

Add to `ComponentRegistry`:

```cpp
bool Register(const Refl::RuntimeTypeDescriptor& descriptor);
```

Implementation copies descriptor fields into `ComponentMetadata`, including function pointers:

```cpp
metadata.Serialize = descriptor.Serialize;
metadata.Deserialize = descriptor.Deserialize;
metadata.ConstructDefault = descriptor.ConstructDefault;
metadata.Destroy = descriptor.Destroy;
metadata.Copy = descriptor.Copy;
metadata.AddCopyToWorld = descriptor.AddCopyToWorld;
```

- [ ] **Step 3: Update generated registration**

Change generated `RegisterGeneratedComponents()`:

```cpp
for (const auto& type : HE::Refl::GetRuntimeTypes()) {
    if (type.Kind == "component") {
        registry.Register(type);
    }
}
```

- [ ] **Step 4: Verify**

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
```

Expected: pass.

- [ ] **Step 5: Commit registry integration**

```powershell
git add HuaEngine/src/HuaEngine/ECS/ComponentRegistry.* HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp Tests/ReflectionGeneratedSmoke.cpp
git commit -m "feat(reflection): register components from runtime descriptors"
```

---

### Task 4: Remove Component Generated Includes

**Files:**
- Modify: `HuaEngine/src/HuaEngine/ECS/Components.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- Modify: `Tests/ReflectionToolSmoke.cpp`
- Modify: `Tests/ReflectionSmoke.cpp`

- [ ] **Step 1: Add smoke assertions**

Ensure `Tests/ReflectionToolSmoke.cpp` asserts:

```cpp
Expect(ReadTextFile(repositoryRoot / "HuaEngine/src/HuaEngine/ECS/Components.h").find("Generated/Reflection") == std::string::npos, "Components.h should not include generated reflection");
Expect(ReadTextFile(repositoryRoot / "HuaEngine/src/Module/Rendering/RenderingComponent.h").find("Generated/Reflection") == std::string::npos, "RenderingComponent.h should not include generated reflection");
```

- [ ] **Step 2: Remove includes**

Remove:

```cpp
#include "HuaEngine/Generated/Reflection/HuaEngine_ECS_Components.generated.h"
#include "HuaEngine/Generated/Reflection/Module_Rendering_RenderingComponent.generated.h"
```

- [ ] **Step 3: Rewrite ReflectionSmoke**

Change `Tests/ReflectionSmoke.cpp` from `Refl::reflect<TransformComponent>()` validation to runtime descriptor validation:

```cpp
const auto* transform = HE::Refl::FindRuntimeType("HE::TransformComponent");
Require(transform != nullptr, "Expected TransformComponent runtime reflection");
Require(HasField(transform->Fields, "Position"), "Expected Position field");
```

- [ ] **Step 4: Verify no component header generated include**

Run:

```powershell
rg -n "Generated/Reflection|HE_GENERATED_REFLECTION_SOURCE_|srefl_class\\(" HuaEngine/src/HuaEngine/ECS/Components.h HuaEngine/src/Module/Rendering/RenderingComponent.h
```

Expected: no output.

- [ ] **Step 5: Build and run**

Run:

```powershell
cmake --build build --config Debug --target ReflectionSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionSmoke.exe
cmake --build build --config Debug --target ReflectionToolSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
```

Expected: both pass.

- [ ] **Step 6: Commit component include removal**

```powershell
git add HuaEngine/src/HuaEngine/ECS/Components.h HuaEngine/src/Module/Rendering/RenderingComponent.h Tests/ReflectionToolSmoke.cpp Tests/ReflectionSmoke.cpp
git commit -m "refactor(reflection): remove component generated includes"
```

---

### Task 5: Component Serialization Uses Runtime Reflection

**Files:**
- Modify: `Tests/SerializationSmoke.cpp`
- Modify: `Tests/ECSSceneSerializationSmoke.cpp`

- [ ] **Step 1: Add component round-trip assertions**

In `Tests/SerializationSmoke.cpp`, verify `TransformComponent` serializes through generated runtime serializer by using `ComponentRegistry` metadata:

```cpp
HE::ComponentRegistry registry;
HE::RegisterCoreComponents(registry);
const auto* metadata = registry.FindByName("TransformComponent");
Require(metadata != nullptr, "Expected TransformComponent metadata");
Require(metadata->Serialize != nullptr, "Expected generated serializer");
```

Serialize a transform through `metadata->Serialize`, deserialize into another transform through `metadata->Deserialize`, assert field equality.

- [ ] **Step 2: Run RED/GREEN**

Run before final implementation if Task 3 has not wired serializer yet; expected fail. After Task 3/4 it should pass.

Run:

```powershell
cmake --build build --config Debug --target SerializationSmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationSmoke.exe
```

- [ ] **Step 3: Verify scene serialization**

Run:

```powershell
cmake --build build --config Debug --target ECSSceneSerializationSmoke
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
```

Expected: pass.

- [ ] **Step 4: Commit serialization runtime path coverage**

```powershell
git add Tests/SerializationSmoke.cpp Tests/ECSSceneSerializationSmoke.cpp
git commit -m "test(serialization): cover runtime reflected component serialization"
```

---

### Task 6: P2-A/B Full Verification

**Files:**
- Modify only if verification exposes issues.

- [ ] **Step 1: Regenerate and validate**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
git diff -- HuaEngine/src/HuaEngine/Generated
```

Expected: validate exits 0 and generated files have no diff.

- [ ] **Step 2: Run P2-A/B smoke set**

Run:

```powershell
cmake --build build --config Debug --target ReflectionSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target ReflectionToolSmoke
cmake --build build --config Debug --target SerializationSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target CLIReflectionSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIReflectionSmoke.exe
```

Expected: all pass.

- [ ] **Step 3: Commit fixes only if needed**

If verification required fixes:

```powershell
git add <fixed-files>
git commit -m "fix(reflection): address runtime facade verification issues"
```

---

### Task 7: P2-C Serialization Policy Audit and Tests

**Files:**
- Create or Modify: `Tests/SerializationPolicySmoke.cpp`
- Modify: `CMakeLists.txt`
- Modify: serialization files only as required by failing tests.

- [ ] **Step 1: Add missing field policy test**

Create `Tests/SerializationPolicySmoke.cpp` with a scene/component JSON missing one reflected field. Expected policy:

- load succeeds;
- missing field keeps default value;
- diagnostics path is planned for future structured reporting if current backend lacks it.

If we choose fail-on-missing instead during implementation, update this task before coding and keep the policy explicit.

- [ ] **Step 2: Add unknown component test**

Load scene JSON containing `"UnknownComponent": {}`.

Expected policy:

- load succeeds;
- unknown component is skipped;
- test asserts known components still load.

If no diagnostics channel exists yet, add an explicit test comment stating that structured diagnostics are the next required follow-up for this policy.

- [ ] **Step 3: Add unknown field test**

Load component JSON containing extra field.

Expected policy:

- extra field ignored;
- known fields deserialize correctly.

- [ ] **Step 4: Add `Ref<T>` serializer regression**

Add a minimal test around a nullable `Ref<T>` type that currently uses `Serializer<Ref<T>>`.

Expected:

- null ref round-trips as null.
- non-null ref round-trips if `T` has serializer support.

- [ ] **Step 5: Add scene output stability test**

Serialize the same scene twice and assert identical JSON string output.

If current entity iteration order prevents this, document and fix deterministic ordering in `SceneSerializer`.

- [ ] **Step 6: Wire CMake target**

Add:

```cmake
add_executable(SerializationPolicySmoke Tests/SerializationPolicySmoke.cpp)
target_link_libraries(SerializationPolicySmoke PRIVATE HuaEngine)
configure_smoke_target(SerializationPolicySmoke)
set_property(TARGET SerializationPolicySmoke PROPERTY FOLDER "Tests")
```

Match existing include dirs and MSVC options.

- [ ] **Step 7: Run RED/GREEN**

Run:

```powershell
cmake --build build --config Debug --target SerializationPolicySmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
```

Expected before fixes: at least one policy test may fail. Fix serialization behavior until it passes.

- [ ] **Step 8: Commit P2-C policy coverage**

```powershell
git add Tests/SerializationPolicySmoke.cpp CMakeLists.txt HuaEngine/src/HuaEngine/Serialization HuaEngine/src/HuaEngine/Scene
git commit -m "test(serialization): define reflection serialization policies"
```

---

### Task 8: P2-C Remaining `srefl_class` Audit

**Files:**
- Create: `.workspace/superpower/reports/2026-07-04-srefl-legacy-audit.md`

- [ ] **Step 1: Audit usage**

Run:

```powershell
rg -n "srefl_class|Refl::reflect|type_info<" HuaEngine/src Tests
```

- [ ] **Step 2: Write audit report**

Create Chinese report:

```markdown
# srefl legacy 使用审计

## 结论

...

## 保留项

...

## 迁移候选

...

## 删除前置条件

...
```

- [ ] **Step 3: Commit audit**

```powershell
git add .workspace/superpower/reports/2026-07-04-srefl-legacy-audit.md
git commit -m "docs(reflection): audit legacy srefl usage"
```

---

## Self-Review Notes

- P2-A/B 明确先解决 component generated include 和 `Serializer<T> -> Refl::reflect<T>()` 对组件路径的依赖。
- P2-C 已作为连续任务写入计划，A/B 完成后立即执行，不延期。
- 旧 `srefl_class` 不在 A/B 删除，避免影响 `MeshData` 和非组件默认 serializer。
- 计划包含 RED/GREEN 步骤、生成器 drift 验证、smoke 构建和运行命令。
