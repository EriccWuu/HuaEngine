# Reflection Runtime Provider P3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立统一 runtime reflection provider 模型，增强字段 descriptor/accessor，让组件和部分非组件都通过 `HE::Refl` runtime facade 对外提供反射与序列化能力。

**Architecture:** `RuntimeFieldDescriptor` 扩展为带 offset/size/flags/accessor/field operation 的 property metadata；组件序列化从 generated type-level callback 迁移为 `SerializeRuntimeObject` / `DeserializeRuntimeObject` 通用遍历；`ComponentRegistry` 关联 `RuntimeTypeDescriptor`，不再要求普通组件 generated `Serialize_X` / `Deserialize_X` 函数；后续通过 provider/adapter 汇总 generated 和 static reflection metadata。

**Tech Stack:** C++20, Python 3, CMake/MSBuild, HuaEngine ECS, `HE::Refl`, `HE::Serialization`, generated reflection tool.

---

## File Structure

- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
  - 扩展 runtime descriptor API、flags、field accessors、generic runtime object operations。
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`
  - `ComponentMetadata` 关联 `const Refl::RuntimeTypeDescriptor* RuntimeType`。
  - 移除或降级 `Serialize` / `Deserialize` registry storage。
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`
  - descriptor registration 保存 runtime descriptor，并提供 generic serialization helpers if needed。
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
  - scene component serialization/deserialization 改为通过 `metadata.RuntimeType` 调用 runtime generic operations。
- Modify: `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`
  - component fast path 改为 runtime generic operation；非组件 legacy fallback 保留。
- Modify: `Tools/Reflection/reflection_tool.py`
  - 生成 field getter/mutable/serialize/deserialize callbacks。
  - 停止为普通组件生成 `Serialize_X` / `Deserialize_X` type-level callbacks。
  - 生成 descriptor provider/registration output。
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
  - checked-in generated output。
- Modify: `Tests/ReflectionGeneratedSmoke.cpp`
  - 验证 field descriptor offset/size/flags/accessor。
- Modify: `Tests/ReflectionSmoke.cpp`
  - 验证外部 runtime field traversal/read/write。
- Modify: `Tests/SerializationSmoke.cpp`
  - 验证 registry metadata 通过 generic runtime operation round-trip。
- Modify: `Tests/SerializationPolicySmoke.cpp`
  - 保持缺失字段、坏字段、半写防护、known component failure policy。
- Modify: `Tests/ECSSceneSerializationSmoke.cpp`
  - 保持 scene runtime component field round-trip。
- Modify: `Tests/CLIReflectionSmoke.cpp`
  - 如 CLI 输出受 Kind/field metadata 影响，更新断言。
- Create or Modify: `Tests/ReflectionRuntimeProviderSmoke.cpp`
  - 验证 runtime provider/static adapter 的第一个非组件路径。
- Modify: `CMakeLists.txt`
  - 如新增 smoke target，则加入构建和 Tests folder。
- Modify: `.workspace/superpower/reports/2026-07-04-srefl-legacy-audit.md`
  - 更新 P3 后 legacy/static reflection 状态。

---

### Task 1: Extend Runtime Field Descriptor API

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`

- [ ] **Step 1: Add failing field descriptor assertions**

In `Tests/ReflectionGeneratedSmoke.cpp`, extend the Transform runtime descriptor assertions:

```cpp
const auto* transformRuntime = HE::Refl::FindRuntimeType("HE::TransformComponent");
Require(transformRuntime != nullptr, "Expected runtime reflection descriptor for TransformComponent");
const HE::Refl::RuntimeFieldDescriptor* positionField = FindRuntimeField(transformRuntime->Fields, "Position");
Require(positionField != nullptr, "Expected runtime Position field descriptor");
Require(positionField->Offset == offsetof(HE::TransformComponent, Position), "Expected Position runtime offset");
Require(positionField->Size == sizeof(glm::vec3), "Expected Position runtime field size");
Require(HE::Refl::HasRuntimeFieldFlag(positionField->Flags, HE::Refl::RuntimeFieldFlags::Serializable), "Expected Position to be serializable");
Require(positionField->GetConst != nullptr, "Expected Position const accessor");
Require(positionField->GetMutable != nullptr, "Expected Position mutable accessor");
```

Add helper:

```cpp
const HE::Refl::RuntimeFieldDescriptor* FindRuntimeField(
    std::span<const HE::Refl::RuntimeFieldDescriptor> fields,
    std::string_view name) {
    for (const auto& field : fields) {
        if (field.Name == name) {
            return &field;
        }
    }
    return nullptr;
}
```

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: fail to compile because `Offset`, `Size`, `Flags`, accessors, and flag helpers do not exist.

- [ ] **Step 2: Extend descriptor declarations**

In `HuaEngine/src/HuaEngine/Reflection/Reflection.h`, replace `RuntimeFieldDescriptor` with:

```cpp
enum class RuntimeFieldFlags : uint32_t {
    None = 0,
    Serializable = 1u << 0,
    Editable = 1u << 1,
    ReadOnly = 1u << 2,
    ComponentField = 1u << 3,
};

inline RuntimeFieldFlags operator|(RuntimeFieldFlags lhs, RuntimeFieldFlags rhs) {
    return static_cast<RuntimeFieldFlags>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline RuntimeFieldFlags operator&(RuntimeFieldFlags lhs, RuntimeFieldFlags rhs) {
    return static_cast<RuntimeFieldFlags>(
        static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline bool HasRuntimeFieldFlag(RuntimeFieldFlags flags, RuntimeFieldFlags flag) {
    return static_cast<uint32_t>(flags & flag) != 0;
}

struct RuntimeFieldDescriptor {
    std::string_view Name;
    std::string_view Type;
    std::string_view DisplayName;
    std::string_view Category;
    size_t Offset;
    size_t Size;
    RuntimeFieldFlags Flags;
    const void* (*GetConst)(const void*);
    void* (*GetMutable)(void*);
    void (*Serialize)(Serialization::SerializationBackend&, const std::string&, const void*);
    bool (*Deserialize)(Serialization::SerializationBackend&, const std::string&, void*);
};
```

Keep `RuntimeTypeDescriptor` unchanged in this task except required compile fixes.

- [ ] **Step 3: Run compile check**

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: still fail because generated descriptors do not fill the new fields yet.

- [ ] **Step 4: Commit API and failing test**

```powershell
git add HuaEngine/src/HuaEngine/Reflection/Reflection.h Tests/ReflectionGeneratedSmoke.cpp
git commit -m "feat(reflection): extend runtime field descriptors"
```

---

### Task 2: Generate Runtime Field Accessors and Field Operations

**Files:**
- Modify: `Tools/Reflection/reflection_tool.py`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`

- [ ] **Step 1: Update generator to emit field callbacks**

For each reflected component field, generate callbacks:

```cpp
static const void* GetConst_HE__TransformComponent_Position(const void* object) {
    return &static_cast<const HE::TransformComponent*>(object)->Position;
}

static void* GetMutable_HE__TransformComponent_Position(void* object) {
    return &static_cast<HE::TransformComponent*>(object)->Position;
}

static void Serialize_HE__TransformComponent_Position(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    const auto& field = *static_cast<const glm::vec3*>(
        GetConst_HE__TransformComponent_Position(object));
    Serialization::SerializeValue(backend, name, field);
}

static bool Deserialize_HE__TransformComponent_Position(
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    auto& field = *static_cast<glm::vec3*>(
        GetMutable_HE__TransformComponent_Position(object));
    auto fieldValue = field;
    if (!Serialization::DeserializeValue(backend, name, fieldValue)) {
        return false;
    }
    field = fieldValue;
    return true;
}
```

Generate equivalent callbacks for all reflected fields.

- [ ] **Step 2: Fill extended RuntimeFieldDescriptor**

Generated `RuntimeType4Fields` should look like:

```cpp
static constexpr Refl::RuntimeFieldDescriptor RuntimeType4Fields[] = {
    {
        "Position",
        "glm::vec3",
        "",
        "",
        offsetof(HE::TransformComponent, Position),
        sizeof(glm::vec3),
        Refl::RuntimeFieldFlags::Serializable | Refl::RuntimeFieldFlags::Editable | Refl::RuntimeFieldFlags::ComponentField,
        &GetConst_HE__TransformComponent_Position,
        &GetMutable_HE__TransformComponent_Position,
        &Serialize_HE__TransformComponent_Position,
        &Deserialize_HE__TransformComponent_Position,
    },
};
```

Use the parsed field type for `sizeof(<field type>)`.

- [ ] **Step 3: Regenerate files**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
```

Expected: validate exits 0.

- [ ] **Step 4: Verify field descriptor smoke**

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
```

Expected: pass.

- [ ] **Step 5: Commit generated field accessors**

```powershell
git add Tools/Reflection/reflection_tool.py HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp Tests/ReflectionGeneratedSmoke.cpp
git commit -m "feat(reflection): generate runtime field accessors"
```

---

### Task 3: Add Generic Runtime Object Serialization

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Test: `Tests/ReflectionSmoke.cpp`
- Test: `Tests/SerializationPolicySmoke.cpp`

- [ ] **Step 1: Add failing generic runtime operation tests**

In `Tests/ReflectionSmoke.cpp`, replace direct descriptor serializer usage with:

```cpp
HE::Serialization::JsonSerializationBackend writeBackend;
HE::Refl::SerializeRuntimeObject(*transformType, writeBackend, std::string(transformType->Name), &sourceTransform);
const std::string transformJson = writeBackend.SaveToString();
Require(transformJson.find("\"Position\"") != std::string::npos, "Expected generic runtime serialization to emit Position");
```

For deserialize:

```cpp
HE::Serialization::JsonSerializationBackend readBackend;
readBackend.LoadFromString(transformJson);
Require(
    HE::Refl::DeserializeRuntimeObject(*transformType, readBackend, std::string(transformType->Name), &loadedTransform),
    "Expected generic runtime deserialization to succeed");
```

Run:

```powershell
cmake --build build --config Debug --target ReflectionSmoke
```

Expected: fail because generic runtime operations do not exist.

- [ ] **Step 2: Declare generic runtime operations**

In `Reflection.h`, add:

```cpp
void SerializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object);

bool DeserializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object);
```

- [ ] **Step 3: Implement generic runtime operations**

Implement in `GeneratedReflection.cpp` under `namespace HE::Refl` for now:

```cpp
void SerializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object) {
    backend.BeginObject(name);
    for (const RuntimeFieldDescriptor& field : type.Fields) {
        if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) || field.Serialize == nullptr) {
            continue;
        }
        field.Serialize(backend, std::string(field.Name), object);
    }
    backend.EndObject();
}

bool DeserializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object) {
    if (!name.empty() && !backend.HasField(name)) {
        return false;
    }

    backend.BeginObject(name);
    bool success = true;
    for (const RuntimeFieldDescriptor& field : type.Fields) {
        if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable) || field.Deserialize == nullptr) {
            continue;
        }
        const std::string fieldName(field.Name);
        if (!backend.HasField(fieldName)) {
            continue;
        }
        if (!field.Deserialize(backend, fieldName, object)) {
            success = false;
        }
    }
    backend.EndObject();
    return success;
}
```

- [ ] **Step 4: Verify policy smoke still passes**

Run:

```powershell
cmake --build build --config Debug --target ReflectionSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionSmoke.exe
cmake --build build --config Debug --target SerializationPolicySmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
```

Expected: both pass.

- [ ] **Step 5: Commit generic runtime operations**

```powershell
git add HuaEngine/src/HuaEngine/Reflection/Reflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp Tests/ReflectionSmoke.cpp Tests/SerializationPolicySmoke.cpp
git commit -m "feat(reflection): add generic runtime object operations"
```

---

### Task 4: Move ComponentRegistry to RuntimeTypeDescriptor Generic Operations

**Files:**
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- Modify: `HuaEngine/src/HuaEngine/Serialization/SerializationCore.h`
- Test: `Tests/SerializationSmoke.cpp`
- Test: `Tests/ECSSceneSerializationSmoke.cpp`
- Test: `Tests/SerializationPolicySmoke.cpp`

- [ ] **Step 1: Add assertions that registry metadata has runtime type**

In `Tests/SerializationSmoke.cpp`, after finding `transformMetadata`, add:

```cpp
Require(transformMetadata->RuntimeType != nullptr, "Expected TransformComponent metadata runtime type");
Require(transformMetadata->RuntimeType->QualifiedName == "HE::TransformComponent", "Expected TransformComponent runtime type metadata");
```

Also change metadata round-trip to call:

```cpp
HE::Refl::SerializeRuntimeObject(*transformMetadata->RuntimeType, transformWriteBackend, transformMetadata->TypeName, &sourceTransform);
```

and:

```cpp
Require(
    HE::Refl::DeserializeRuntimeObject(*transformMetadata->RuntimeType, transformReadBackend, transformMetadata->TypeName, &loadedMetadataTransform),
    "Expected metadata transform JSON to deserialize");
```

Run:

```powershell
cmake --build build --config Debug --target SerializationSmoke
```

Expected: fail because `ComponentMetadata::RuntimeType` does not exist.

- [ ] **Step 2: Add RuntimeType pointer to ComponentMetadata**

In `ComponentRegistry.h`, add:

```cpp
const Refl::RuntimeTypeDescriptor* RuntimeType = nullptr;
```

Remove `Serialize` and `Deserialize` from `ComponentMetadata` only if all compile errors are handled in this task. If the change is too large, keep them temporarily but mark them as generic wrappers in implementation and tests.

- [ ] **Step 3: Register descriptor pointer**

In `ComponentRegistry::Register(const Refl::RuntimeTypeDescriptor& descriptor)`, set:

```cpp
metadata.RuntimeType = &descriptor;
```

Do not copy generated type-level serialize/deserialize callbacks for normal components.

- [ ] **Step 4: Update SceneSerializer**

In `SceneSerializer.cpp`, replace metadata serialization:

```cpp
metadata->Serialize(backend, metadata->TypeName, component);
```

with:

```cpp
if (metadata->RuntimeType == nullptr) {
    continue;
}
HE::Refl::SerializeRuntimeObject(*metadata->RuntimeType, backend, metadata->TypeName, component);
```

Replace metadata deserialization:

```cpp
const bool componentSuccess = metadata->Deserialize(backend, componentName, component);
```

with:

```cpp
const bool componentSuccess = metadata->RuntimeType != nullptr &&
    HE::Refl::DeserializeRuntimeObject(*metadata->RuntimeType, backend, componentName, component);
```

Keep unknown component skip behavior and known bad component failure behavior.

- [ ] **Step 5: Update SerializationCore component fast path**

In `Serializer<T>::Serialize`, for component runtime descriptor:

```cpp
Refl::SerializeRuntimeObject(*descriptor, backend, name, &obj);
return;
```

In `Serializer<T>::Deserialize`:

```cpp
return Refl::DeserializeRuntimeObject(*descriptor, backend, name, &obj);
```

- [ ] **Step 6: Verify component serialization smoke**

Run:

```powershell
cmake --build build --config Debug --target SerializationSmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationSmoke.exe
cmake --build build --config Debug --target ECSSceneSerializationSmoke
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
cmake --build build --config Debug --target SerializationPolicySmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
```

Expected: all pass.

- [ ] **Step 7: Commit registry generic runtime path**

```powershell
git add HuaEngine/src/HuaEngine/ECS/ComponentRegistry.* HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp HuaEngine/src/HuaEngine/Serialization/SerializationCore.h Tests/SerializationSmoke.cpp Tests/ECSSceneSerializationSmoke.cpp Tests/SerializationPolicySmoke.cpp
git commit -m "refactor(reflection): route component registry through runtime type descriptors"
```

---

### Task 5: Remove Ordinary Component Type-Level Generated Callbacks

**Files:**
- Modify: `Tools/Reflection/reflection_tool.py`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`
- Test: `Tests/SerializationSmoke.cpp`

- [ ] **Step 1: Add generated output regression assertion**

In `Tests/ReflectionGeneratedSmoke.cpp`, add a source text check similar to existing generated checks if repository root helper is not available, or add a generator smoke assertion in `Tests/ReflectionToolSmoke.cpp`:

```cpp
const std::string generatedSource = ReadTextFile(repositoryRoot / "HuaEngine" / "src" / "HuaEngine" / "Generated" / "GeneratedReflection.cpp");
Expect(generatedSource.find("Serialize_HE__TransformComponent(") == std::string::npos, "Generated reflection should not emit ordinary component type-level serializers");
Expect(generatedSource.find("Deserialize_HE__TransformComponent(") == std::string::npos, "Generated reflection should not emit ordinary component type-level deserializers");
```

Prefer `ReflectionToolSmoke.cpp` if it already has repository root helpers.

Run:

```powershell
cmake --build build --config Debug --target ReflectionToolSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
```

Expected: fail because generated source still contains type-level callbacks.

- [ ] **Step 2: Remove type-level serializer generation**

In `reflection_tool.py`, stop emitting:

```text
static void Serialize_<Type>(...)
static bool Deserialize_<Type>(...)
```

for ordinary component descriptors.

Keep field-level callbacks from Task 2.

- [ ] **Step 3: Update RuntimeTypeDescriptor generated entries**

For ordinary components, generated descriptor should set serialize/deserialize override fields to `nullptr` if Task 3 renamed them to `SerializeOverride` / `DeserializeOverride`, or remove references to deleted functions.

Example:

```cpp
{"TransformComponent", ..., nullptr, nullptr, &AddCopyToWorld_HE__TransformComponent}
```

- [ ] **Step 4: Regenerate and validate**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
```

Expected: validate exits 0.

- [ ] **Step 5: Verify smoke suite subset**

Run:

```powershell
cmake --build build --config Debug --target ReflectionToolSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
cmake --build build --config Debug --target SerializationSmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationSmoke.exe
cmake --build build --config Debug --target SerializationPolicySmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
```

Expected: all pass.

- [ ] **Step 6: Commit removal**

```powershell
git add Tools/Reflection/reflection_tool.py HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp Tests/ReflectionToolSmoke.cpp
git commit -m "refactor(reflection): remove generated component type serializers"
```

---

### Task 6: Add Runtime Provider Registry Layer

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`

- [ ] **Step 1: Add provider behavior assertions**

In `Tests/ReflectionGeneratedSmoke.cpp`, add:

```cpp
const auto runtimeTypes = HE::Refl::GetRuntimeTypes();
Require(runtimeTypes.size() >= 5, "Expected runtime provider registry to expose generated component types");
Require(HE::Refl::FindRuntimeType("HE::TransformComponent") == transformRuntime, "Expected runtime lookup to return provider descriptor");
```

This may already pass before the registry layer. The implementation should keep behavior stable.

- [ ] **Step 2: Add RuntimeRegistry declaration**

In `Reflection.h`, add:

```cpp
class RuntimeRegistry {
public:
    bool RegisterType(const RuntimeTypeDescriptor& type);
    [[nodiscard]] std::span<const RuntimeTypeDescriptor> GetTypes() const;
    [[nodiscard]] const RuntimeTypeDescriptor* FindByQualifiedName(std::string_view qualifiedName) const;
    [[nodiscard]] const RuntimeTypeDescriptor* FindByComponentTypeId(ComponentTypeId typeId) const;
};
```

If storing references in a vector is awkward, implement an internal `std::vector<const RuntimeTypeDescriptor*>` and expose a stable span through a generated flattened cache. Keep the public API simple.

- [ ] **Step 3: Implement provider aggregation without static init order risk**

In `GeneratedReflection.cpp`, use function-local static initialization:

```cpp
static std::span<const RuntimeTypeDescriptor> GetGeneratedRuntimeTypes() {
    return Generated::RuntimeTypes;
}

std::span<const RuntimeTypeDescriptor> GetRuntimeTypes() {
    return GetGeneratedRuntimeTypes();
}
```

If a full mutable registry is too large for this task, create a minimal provider layer:

```cpp
using RuntimeTypeProvider = std::span<const RuntimeTypeDescriptor> (*)();
```

and aggregate generated provider plus future static provider. The acceptance criterion is that `GetRuntimeTypes()` is no longer hard-coded directly to generated arrays in public-facing implementation.

- [ ] **Step 4: Verify lookups**

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
```

Expected: pass.

- [ ] **Step 5: Commit provider layer**

```powershell
git add HuaEngine/src/HuaEngine/Reflection/Reflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp Tests/ReflectionGeneratedSmoke.cpp
git commit -m "feat(reflection): add runtime type provider layer"
```

---

### Task 7: Add Static Reflection Adapter Smoke

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- Create or Modify: `Tests/ReflectionRuntimeProviderSmoke.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add failing static adapter smoke**

Create `Tests/ReflectionRuntimeProviderSmoke.cpp`:

```cpp
#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace HE {
    struct RuntimeProviderFixture {
        std::string Name = "default";
        int Value = 7;
    };
}

srefl_class(HE::RuntimeProviderFixture,
    fields(
        field(Name),
        field(Value)
    )
)

namespace {
    void Require(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "[ReflectionRuntimeProviderSmoke] " << message << std::endl;
            std::exit(1);
        }
    }
}

int main() {
    const HE::Refl::RuntimeTypeDescriptor& descriptor =
        HE::Refl::MakeStaticRuntimeTypeDescriptor<HE::RuntimeProviderFixture>(
            "RuntimeProviderFixture",
            "HE::RuntimeProviderFixture",
            "struct");

    Require(descriptor.Fields.size() == 2, "Expected static runtime descriptor fields");
    HE::RuntimeProviderFixture source;
    source.Name = "runtime";
    source.Value = 42;

    HE::Serialization::JsonSerializationBackend writeBackend;
    HE::Refl::SerializeRuntimeObject(descriptor, writeBackend, descriptor.Name.data(), &source);
    const std::string json = writeBackend.SaveToString();
    Require(json.find("\"Name\"") != std::string::npos, "Expected static runtime serializer to emit Name");

    HE::RuntimeProviderFixture loaded;
    HE::Serialization::JsonSerializationBackend readBackend;
    readBackend.LoadFromString(json);
    Require(HE::Refl::DeserializeRuntimeObject(descriptor, readBackend, descriptor.Name.data(), &loaded), "Expected static runtime deserialize");
    Require(loaded.Name == "runtime", "Expected Name to round-trip");
    Require(loaded.Value == 42, "Expected Value to round-trip");

    std::cout << "ReflectionRuntimeProviderSmoke passed" << std::endl;
    return 0;
}
```

Add target in `CMakeLists.txt` following the `ReflectionSmoke` pattern.

Run:

```powershell
cmake --build build --config Debug --target ReflectionRuntimeProviderSmoke
```

Expected: fail because `MakeStaticRuntimeTypeDescriptor` does not exist.

- [ ] **Step 2: Implement static descriptor adapter**

In `Reflection.h`, add a templated adapter. For this task, it may return a function-local static descriptor and use static reflection field traits:

```cpp
template<typename T>
const RuntimeTypeDescriptor& MakeStaticRuntimeTypeDescriptor(
    std::string_view name,
    std::string_view qualifiedName,
    std::string_view kind);
```

Implementation constraints:

- Use `Refl::reflect<T>().visit_fields(...)`.
- Build field descriptors with offset/size/accessors/serialize/deserialize.
- Avoid dangling string_view: require caller string literals or store stable generated/static strings.
- `Kind` may remain string if `RuntimeTypeDescriptor` has not moved to enum yet.

If generic dynamic allocation is too complex in a header template, keep this task to a test-only static adapter with `std::vector<RuntimeFieldDescriptor>` function-local static storage and document the lifetime.

- [ ] **Step 3: Verify static provider smoke**

Run:

```powershell
cmake --build build --config Debug --target ReflectionRuntimeProviderSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionRuntimeProviderSmoke.exe
```

Expected: pass.

- [ ] **Step 4: Commit static adapter**

```powershell
git add HuaEngine/src/HuaEngine/Reflection/Reflection.h Tests/ReflectionRuntimeProviderSmoke.cpp CMakeLists.txt
git commit -m "feat(reflection): adapt static reflection to runtime descriptors"
```

---

### Task 8: Migrate One Legacy Non-Component Type to Runtime Provider

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Project/ProjectContext.h`
- Modify: `Tests/ReflectionRuntimeProviderSmoke.cpp`
- Test: existing project-related smoke if available

- [ ] **Step 1: Add runtime descriptor assertion for ProjectDescriptor**

In `Tests/ReflectionRuntimeProviderSmoke.cpp`, add:

```cpp
const auto& projectDescriptor = HE::Refl::MakeStaticRuntimeTypeDescriptor<HE::ProjectDescriptor>(
    "ProjectDescriptor",
    "HE::ProjectDescriptor",
    "struct");
Require(projectDescriptor.Fields.size() == 4, "Expected ProjectDescriptor runtime fields");
```

Include:

```cpp
#include "HuaEngine/Project/ProjectContext.h"
```

Run:

```powershell
cmake --build build --config Debug --target ReflectionRuntimeProviderSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionRuntimeProviderSmoke.exe
```

Expected: pass if adapter works; fail if ProjectDescriptor static reflection fields have incompatibilities.

- [ ] **Step 2: Verify ProjectDescriptor serializer remains stable**

Find project smoke target:

```powershell
rg -n "Project.*Smoke|ProjectDescriptor" Tests CMakeLists.txt HuaEngine/src
```

Run the relevant target, likely:

```powershell
cmake --build build --config Debug --target ProjectServiceSmoke
.\build\bin\Debug-Windows-x64\smoke\ProjectServiceSmoke.exe
```

Expected: pass.

- [ ] **Step 3: Commit legacy type adapter validation**

```powershell
git add Tests/ReflectionRuntimeProviderSmoke.cpp
git commit -m "test(reflection): cover project descriptor runtime provider"
```

---

### Task 9: Update Legacy Audit and Docs

**Files:**
- Modify: `.workspace/superpower/reports/2026-07-04-srefl-legacy-audit.md`
- Modify: `HuaEngine/src/HuaEngine/Serialization/README.md` only if encoding-safe edits are practical

- [ ] **Step 1: Update audit report**

Update the report with P3 status:

- static reflection is now an internal provider source.
- components use runtime field descriptors and generic runtime object operations.
- ordinary component generated type-level callbacks have been removed.
- `ProjectDescriptor` has runtime provider smoke coverage.
- remaining migration candidates still include `MeshData` series and README cleanup.

- [ ] **Step 2: Avoid README encoding churn unless necessary**

If `Serialization/README.md` still shows mojibake in PowerShell but file edits risk spreading encoding churn, do not rewrite it wholesale. Add a short note to the audit report instead.

- [ ] **Step 3: Commit docs update**

```powershell
git add .workspace/superpower/reports/2026-07-04-srefl-legacy-audit.md
git commit -m "docs(reflection): update runtime provider audit"
```

---

### Task 10: Full Verification

**Files:**
- Modify only if verification exposes issues.

- [ ] **Step 1: Regenerate and validate**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
git diff -- HuaEngine/src/HuaEngine/Generated Tools/Reflection/reflection_tool.py
```

Expected:

- validate exits 0.
- generated/tool diff is empty.

- [ ] **Step 2: Build smoke targets**

Run sequentially:

```powershell
cmake --build build --config Debug --target ReflectionSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target ReflectionToolSmoke
cmake --build build --config Debug --target ReflectionRuntimeProviderSmoke
cmake --build build --config Debug --target SerializationSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target SerializationPolicySmoke
cmake --build build --config Debug --target MaterialSerializationSmoke
cmake --build build --config Debug --target CLIReflectionSmoke
```

Expected: all build exit 0.

- [ ] **Step 3: Run smoke executables**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\ReflectionSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionRuntimeProviderSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
.\build\bin\Debug-Windows-x64\smoke\MaterialSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIReflectionSmoke.exe
```

Expected: all exit 0.

- [ ] **Step 4: Commit verification fixes only if needed**

If verification required fixes:

```powershell
git add <fixed-files>
git commit -m "fix(reflection): address runtime provider verification issues"
```

---

## Self-Review Notes

- The plan explicitly removes ordinary component generated type-level serializer/deserializer callbacks.
- `ComponentRegistry` moves to descriptor association and generic runtime operations.
- Static reflection remains available internally through an adapter smoke, not as the external component API.
- P2-C serialization policy tests remain part of required verification.
- Provider registry is introduced incrementally to avoid static initialization order risk.
