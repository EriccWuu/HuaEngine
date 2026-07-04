# Reflection Runtime Field Operations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `HE::Refl` runtime 反射从字段描述表推进为可查询字段语义、可安全读写字段、并支持 enum metadata 与 enum 字符串序列化的统一 facade。

**Architecture:** 在 `Reflection.h` 中扩展 runtime descriptor 与通用 field API；在 `reflection_tool.py` 中扫描 `HE_REFLECT_ENUM` 并生成 enum descriptors、enum-aware field callbacks；在 `RuntimeInspector` 中消费核心 `RuntimeFieldValueKind`，不再维护 Editor 私有字段分类。选一个真实引擎字段 `MaterialComponent::BlendMode` 验证 enum 端到端路径。

**Tech Stack:** C++20, Python 3 reflection tool, HuaEngine serialization backend, ImGui runtime inspector, CMake/MSBuild smoke tests.

---

## File Structure

- Modify: `HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h`
  - 增加空 marker `HE_REFLECT_ENUM(...)`。
- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
  - 增加 `RuntimeFieldValueKind`、`RuntimeEnumValueDescriptor`、`RuntimeEnumDescriptor`、enum 查询 API、field 查询/读写 API。
  - `RuntimeFieldDescriptor` 末尾增加 `const RuntimeEnumDescriptor* EnumType`。
- Modify: `Tools/Reflection/reflection_tool.py`
  - 扫描 `HE_REFLECT_ENUM`。
  - manifest 增加 `enums`。
  - 生成 runtime enum descriptors。
  - enum 字段生成字符串名序列化/反序列化 callback。
  - field flags 根据 value kind 增加 `Editable`。
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
  - 暴露 `GetReflectedEnums()` / `FindReflectedEnum()` 的轻量工具信息。
- Modify: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
  - 由工具重新生成，包含 enum descriptors 和更新后的 `RuntimeFieldDescriptor` 初始化。
- Modify: `HuaEngine/src/Module/Rendering/RenderingComponent.h`
  - 新增 `HE_REFLECT_ENUM()` 标记的 `MaterialBlendMode`。
  - `MaterialComponent` 新增 `HE_REFLECT_FIELD() MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;`。
- Modify: `Editor/src/Panels/RuntimeInspector.h`
  - 删除 Editor 私有 `RuntimeFieldEditKind`。
- Modify: `Editor/src/Panels/RuntimeInspector.cpp`
  - 使用 `HE::Refl::GetRuntimeFieldValueKind()` 分派。
  - enum 字段使用 combo。
- Modify: `Tests/ReflectionGeneratedSmoke.cpp`
  - 覆盖 field kind、generic get/set、enum descriptor、enum field serialization。
- Modify: `Tests/ReflectionToolSmoke.cpp`
  - 增加 enum 正向/负向 tool fixture。
- Modify: `Tests/EditorInspectorRuntimeSmoke.cpp`
  - 改为验证核心 `RuntimeFieldValueKind`，并覆盖 enum kind。
- Modify: `Tests/SerializationPolicySmoke.cpp`
  - 增加 enum 字符串反序列化失败不半写测试。

---

### Task 1: Reflection Core Field API

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Reflection/Reflection.h`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`

- [ ] **Step 1: Add failing core API assertions**

In `Tests/ReflectionGeneratedSmoke.cpp`, after `positionField` assertions, add:

```cpp
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*positionField) == HE::Refl::RuntimeFieldValueKind::Float3,
		"Expected Position to be classified as Float3");
	Require(HE::Refl::IsRuntimeFieldSerializable(*positionField), "Expected Position to be serializable");
	Require(HE::Refl::IsRuntimeFieldEditable(*positionField), "Expected Position to be editable");
	glm::vec3 readPosition{};
	Require(
		HE::Refl::GetRuntimeFieldValue(*positionField, &sourceTransform, readPosition),
		"Expected generic runtime get to read Position");
	Require(readPosition == sourceTransform.Position, "Expected generic runtime get to preserve Position value");

	glm::vec3 updatedPosition{ 10.0f, 11.0f, 12.0f };
	Require(
		HE::Refl::SetRuntimeFieldValue(*positionField, &loadedTransform, updatedPosition),
		"Expected generic runtime set to write Position");
	Require(loadedTransform.Position == updatedPosition, "Expected generic runtime set to update Position");
```

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: compile fails because `RuntimeFieldValueKind` and field helper APIs are not defined.

- [ ] **Step 2: Extend runtime descriptor declarations**

In `HuaEngine/src/HuaEngine/Reflection/Reflection.h`, before `RuntimeFieldFlags`, add:

```cpp
enum class RuntimeFieldValueKind {
    Unsupported,
    Bool,
    SignedInteger,
    UnsignedInteger,
    Float,
    Double,
    String,
    Float2,
    Float3,
    Float4,
    Enum,
    Object,
    AssetRef,
};

struct RuntimeEnumValueDescriptor {
    std::string_view Name;
    int64_t Value;
    std::string_view DisplayName;
};

struct RuntimeEnumDescriptor {
    std::string_view Name;
    std::string_view QualifiedName;
    std::string_view UnderlyingType;
    std::span<const RuntimeEnumValueDescriptor> Values;
};
```

In `RuntimeFieldDescriptor`, append this member after `Deserialize`:

```cpp
    const RuntimeEnumDescriptor* EnumType;
```

After runtime type lookup declarations, add:

```cpp
std::span<const RuntimeEnumDescriptor> GetRuntimeEnums();
const RuntimeEnumDescriptor* FindRuntimeEnum(std::string_view qualifiedName);
const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByName(
    const RuntimeEnumDescriptor& enumType,
    std::string_view name);
const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByValue(
    const RuntimeEnumDescriptor& enumType,
    int64_t value);

RuntimeFieldValueKind GetRuntimeFieldValueKind(const RuntimeFieldDescriptor& field);
bool IsRuntimeFieldSerializable(const RuntimeFieldDescriptor& field);
bool IsRuntimeFieldEditable(const RuntimeFieldDescriptor& field);
bool IsRuntimeFieldReadOnly(const RuntimeFieldDescriptor& field);
const void* GetRuntimeFieldConst(const RuntimeFieldDescriptor& field, const void* object);
void* GetRuntimeFieldMutable(const RuntimeFieldDescriptor& field, void* object);
bool CopyRuntimeFieldValue(const RuntimeFieldDescriptor& field, const void* sourceObject, void* targetObject);
bool GetRuntimeEnumFieldValue(const RuntimeFieldDescriptor& field, const void* object, int64_t& outValue);
bool SetRuntimeEnumFieldValue(const RuntimeFieldDescriptor& field, void* object, int64_t value);
bool SetRuntimeEnumFieldValueByName(const RuntimeFieldDescriptor& field, void* object, std::string_view valueName);
```

Add template helpers after the declarations:

```cpp
template<typename T>
bool GetRuntimeFieldValue(const RuntimeFieldDescriptor& field, const void* object, T& outValue) {
    if (field.Size != sizeof(T)) {
        return false;
    }

    const void* value = GetRuntimeFieldConst(field, object);
    if (value == nullptr) {
        return false;
    }

    outValue = *static_cast<const T*>(value);
    return true;
}

template<typename T>
bool SetRuntimeFieldValue(const RuntimeFieldDescriptor& field, void* object, const T& value) {
    if (field.Size != sizeof(T) || !IsRuntimeFieldEditable(field)) {
        return false;
    }

    void* target = GetRuntimeFieldMutable(field, object);
    if (target == nullptr) {
        return false;
    }

    *static_cast<T*>(target) = value;
    return true;
}
```

Update `MakeStaticRuntimeFieldDescriptor()` initializer by appending `nullptr`:

```cpp
            &DeserializeStaticRuntimeField<T, Index>,
            nullptr,
```

- [ ] **Step 3: Implement inline field helper APIs**

In `Reflection.h`, after the template helpers or before `type_info`, add inline implementations:

```cpp
inline bool IsAnyRuntimeTypeName(std::string_view value, std::initializer_list<std::string_view> candidates) {
    for (std::string_view candidate : candidates) {
        if (value == candidate) {
            return true;
        }
    }
    return false;
}

inline RuntimeFieldValueKind GetRuntimeFieldValueKind(const RuntimeFieldDescriptor& field) {
    if (field.EnumType != nullptr) {
        return RuntimeFieldValueKind::Enum;
    }
    if (field.Type == "bool") {
        return RuntimeFieldValueKind::Bool;
    }
    if (IsAnyRuntimeTypeName(field.Type, { "int", "int8_t", "int16_t", "int32_t", "int64_t", "long", "long long" })) {
        return RuntimeFieldValueKind::SignedInteger;
    }
    if (IsAnyRuntimeTypeName(field.Type, { "unsigned int", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "unsigned long", "unsigned long long" })) {
        return RuntimeFieldValueKind::UnsignedInteger;
    }
    if (field.Type == "float") {
        return RuntimeFieldValueKind::Float;
    }
    if (field.Type == "double") {
        return RuntimeFieldValueKind::Double;
    }
    if (field.Type == "std::string") {
        return RuntimeFieldValueKind::String;
    }
    if (field.Type == "glm::vec2") {
        return RuntimeFieldValueKind::Float2;
    }
    if (field.Type == "glm::vec3") {
        return RuntimeFieldValueKind::Float3;
    }
    if (field.Type == "glm::vec4") {
        return RuntimeFieldValueKind::Float4;
    }
    if (field.Type.rfind("Ref<", 0) == 0) {
        return RuntimeFieldValueKind::AssetRef;
    }
    return RuntimeFieldValueKind::Unsupported;
}

inline bool IsRuntimeFieldSerializable(const RuntimeFieldDescriptor& field) {
    return HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Serializable);
}

inline bool IsRuntimeFieldReadOnly(const RuntimeFieldDescriptor& field) {
    return HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::ReadOnly);
}

inline bool IsRuntimeFieldEditable(const RuntimeFieldDescriptor& field) {
    if (!HasRuntimeFieldFlag(field.Flags, RuntimeFieldFlags::Editable) ||
        IsRuntimeFieldReadOnly(field) ||
        field.GetMutable == nullptr) {
        return false;
    }

    const RuntimeFieldValueKind kind = GetRuntimeFieldValueKind(field);
    return kind != RuntimeFieldValueKind::Unsupported &&
           kind != RuntimeFieldValueKind::Object &&
           kind != RuntimeFieldValueKind::AssetRef;
}

inline const void* GetRuntimeFieldConst(const RuntimeFieldDescriptor& field, const void* object) {
    return object != nullptr && field.GetConst != nullptr ? field.GetConst(object) : nullptr;
}

inline void* GetRuntimeFieldMutable(const RuntimeFieldDescriptor& field, void* object) {
    return object != nullptr && field.GetMutable != nullptr ? field.GetMutable(object) : nullptr;
}

inline bool CopyRuntimeFieldValue(const RuntimeFieldDescriptor& field, const void* sourceObject, void* targetObject) {
    const void* source = GetRuntimeFieldConst(field, sourceObject);
    void* target = GetRuntimeFieldMutable(field, targetObject);
    if (source == nullptr || target == nullptr || field.Size == 0 || !IsRuntimeFieldEditable(field)) {
        return false;
    }
    std::memcpy(target, source, field.Size);
    return true;
}
```

Add `#include <cstring>` and `#include <initializer_list>` to `Reflection.h`.

- [ ] **Step 4: Build and commit core field API**

Run:

```powershell
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: still fails because generated `RuntimeFieldDescriptor` initializers do not yet include `EnumType`.

Commit only the core declaration/helper and failing test state after generated initializers are updated in Task 2, not now.

---

### Task 2: Enum Scanner and Generated Runtime Metadata

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h`
- Modify: `Tools/Reflection/reflection_tool.py`
- Modify generated: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h`
- Modify generated: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp`
- Test: `Tests/ReflectionToolSmoke.cpp`

- [ ] **Step 1: Add enum marker**

In `ReflectionMarkers.h`, change:

```cpp
#define HE_REFLECT_COMPONENT(...)
#define HE_REFLECT_FIELD(...)
```

to:

```cpp
#define HE_REFLECT_COMPONENT(...)
#define HE_REFLECT_FIELD(...)
#define HE_REFLECT_ENUM(...)
```

- [ ] **Step 2: Add enum scanner helpers**

In `Tools/Reflection/reflection_tool.py`, add helpers near `find_type_declaration`:

```python
def find_enum_declaration(text: str, offset: int) -> Optional[Dict[str, Any]]:
    window = text[offset : offset + 4096]
    pattern = re.compile(
        r"\benum\s+(?:class\s+)?(?P<name>[A-Za-z_]\w*)\b(?:\s*:\s*(?P<underlying>[A-Za-z_:]\w*(?:\s+[A-Za-z_:]\w*)?))?\s*\{",
        re.MULTILINE,
    )
    match = pattern.search(window)
    if not match:
        return None
    brace_offset = offset + match.end() - 1
    end_brace = find_matching_brace(text, brace_offset)
    if end_brace is None:
        return None
    return {
        "name": match.group("name"),
        "underlying_type": (match.group("underlying") or "int").strip(),
        "declaration_start": offset + match.start(),
        "body_start": brace_offset + 1,
        "body_end": end_brace,
    }


def parse_enum_value_expression(expression: str) -> Optional[int]:
    value = expression.strip()
    if re.fullmatch(r"-?\d+", value):
        return int(value, 10)
    if re.fullmatch(r"0[xX][0-9A-Fa-f]+", value):
        return int(value, 16)
    return None


def collect_enum_values(
    text: str,
    enum_decl: Dict[str, Any],
    source: str,
    diagnostics: List[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    body = text[enum_decl["body_start"] : enum_decl["body_end"]]
    values: List[Dict[str, Any]] = []
    next_value = 0
    for raw_entry in split_top_level_arguments(body):
        entry = raw_entry.strip()
        if not entry:
            continue
        if "=" in entry:
            name, expression = entry.split("=", 1)
            parsed_value = parse_enum_value_expression(expression)
            if parsed_value is None:
                diagnostics.append(
                    make_diagnostic(
                        "error",
                        "enum.value_unsupported_expression",
                        "HE_REFLECT_ENUM only supports implicit, decimal, hex, and negative integer values.",
                        source,
                        line_for_offset(text, enum_decl["body_start"] + body.find(raw_entry)),
                    )
                )
                continue
            next_value = parsed_value
        else:
            name = entry
        enum_name = name.strip()
        if not re.fullmatch(r"[A-Za-z_]\w*", enum_name):
            diagnostics.append(
                make_diagnostic(
                    "error",
                    "enum.value_unparsed",
                    "Could not parse enum value name.",
                    source,
                    line_for_offset(text, enum_decl["body_start"] + body.find(raw_entry)),
                )
            )
            continue
        values.append({"name": enum_name, "value": next_value, "display_name": ""})
        next_value += 1
    return values
```

- [ ] **Step 3: Scan enum markers into manifest**

In `scan_file()`, create `enums: List[Dict[str, Any]] = []` and add a loop before component scanning:

```python
    enums: List[Dict[str, Any]] = []
    for macro_start, macro_end, macro_args in find_macro_calls(text, "HE_REFLECT_ENUM"):
        line = line_for_offset(text, macro_start)
        args = split_top_level_arguments(macro_args)
        metadata = parse_metadata(args)
        declaration = find_enum_declaration(text, macro_end)
        if declaration is None:
            diagnostics.append(
                make_diagnostic(
                    "error",
                    "enum.declaration_not_found",
                    "Could not find an enum declaration after HE_REFLECT_ENUM.",
                    source,
                    line,
                )
            )
            continue
        qualified_name = infer_qualified_name(text, declaration)
        enum_info = {
            "name": declaration["name"],
            "qualified_name": qualified_name,
            "underlying_type": declaration["underlying_type"],
            "source": source,
            "line": line,
            "values": collect_enum_values(text, declaration, source, diagnostics),
        }
        enum_info.update(metadata)
        enums.append(enum_info)
```

Change `scan_file` return type to include both:

```python
    return {"types": types, "enums": enums}
```

In `scan_root()`, collect both:

```python
    enums: List[Dict[str, Any]] = []
    ...
            scanned = scan_file(resolved_root, path, diagnostics)
            types.extend(scanned["types"])
            enums.extend(scanned["enums"])
    ...
    enums.sort(key=lambda item: (item["qualified_name"], item["source"], item["line"]))
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "types": types,
        "enums": enums,
        "diagnostics": diagnostics,
    }
```

- [ ] **Step 4: Validate enum metadata**

In `validate_reflected_types()`, add a `seen_enum_names` map and validate:

```python
    seen_enum_names: Dict[str, Dict[str, Any]] = {}
    for reflected_enum in manifest.get("enums", []):
        source = reflected_enum.get("source")
        line = reflected_enum.get("line")
        qualified_name = reflected_enum.get("qualified_name", "")
        if not reflected_enum.get("values"):
            diagnostics.append(make_diagnostic("error", "enum.no_values", "HE_REFLECT_ENUM enum must declare at least one value.", source, line))
        previous = seen_enum_names.get(qualified_name)
        if previous is not None:
            diagnostics.append(make_diagnostic("error", "enum.duplicate_qualified_name", f"Duplicate reflected enum qualified_name: {qualified_name}", source, line))
            diagnostics.append(make_diagnostic("error", "enum.duplicate_qualified_name", f"Duplicate reflected enum qualified_name first seen here: {qualified_name}", previous.get("source"), previous.get("line")))
        else:
            seen_enum_names[qualified_name] = reflected_enum
```

- [ ] **Step 5: Generate enum descriptors and enum-aware fields**

In `write_generated_files()`, derive:

```python
    manifest_enums = manifest.get("enums", [])
    enum_by_qualified_name = {enum["qualified_name"]: enum for enum in manifest_enums}
    enum_by_short_name = {enum["name"]: enum for enum in manifest_enums}
```

Add `ReflectedEnumValueInfo` / `ReflectedEnumInfo` to generated header:

```cpp
struct ReflectedEnumValueInfo {
    std::string_view Name;
    int64_t Value;
    std::string_view DisplayName;
};

struct ReflectedEnumInfo {
    std::string_view Name;
    std::string_view QualifiedName;
    std::string_view UnderlyingType;
    std::span<const ReflectedEnumValueInfo> Values;
};

std::span<const ReflectedEnumInfo> GetReflectedEnums();
const ReflectedEnumInfo* FindReflectedEnum(std::string_view qualifiedName);
```

Generate `Refl::RuntimeEnumValueDescriptor EnumNValues[]`, `Refl::RuntimeEnumDescriptor RuntimeEnums[]`, `ReflectedEnumValueInfo EnumNReflectedValues[]`, and `ReflectedEnumInfo ReflectedEnums[]`.

For each field, resolve enum:

```python
def resolve_enum_for_field(field_type: str) -> Optional[Dict[str, Any]]:
    return enum_by_qualified_name.get(field_type) or enum_by_short_name.get(field_type.split("::")[-1])
```

When generating `RuntimeFieldDescriptor`, append enum pointer:

```python
enum_type = resolve_enum_for_field(field.get("type", ""))
enum_pointer = f"&RuntimeEnums[{enum_index_by_qualified_name[enum_type['qualified_name']}]" if enum_type else "nullptr"
```

Also append `enum_pointer` as the final initializer item.

For flags, add `Editable` for supported primitive/string/vector/enum fields:

```python
editable_types = {"bool", "int", "int8_t", "int16_t", "int32_t", "int64_t", "unsigned int", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "float", "double", "std::string", "glm::vec2", "glm::vec3", "glm::vec4"}
if field_type in editable_types or enum_type:
    flags += " | Refl::RuntimeFieldFlags::Editable"
```

- [ ] **Step 6: Generate enum serialization callback bodies**

When generating field serializer/deserializer for enum fields, emit:

```cpp
    const auto enumValue = static_cast<int64_t>(component.FieldName);
    if (const auto* value = Refl::FindRuntimeEnumValueByValue(*RuntimeTypeEnumPointer, enumValue)) {
        backend.Serialize(name, std::string(value->Name));
    }
```

For deserializer:

```cpp
    std::string enumName;
    if (!backend.Deserialize(name, enumName)) {
        return false;
    }
    const auto* value = Refl::FindRuntimeEnumValueByName(*RuntimeTypeEnumPointer, enumName);
    if (value == nullptr) {
        return false;
    }
    component.FieldName = static_cast<QualifiedEnumType>(value->Value);
    return true;
```

Non-enum fields keep the current `Serialization::SerializeValue` and temp-value deserialize pattern.

- [ ] **Step 7: Implement runtime enum lookup functions in generated source**

In generated `namespace HE::Refl`, add:

```cpp
std::span<const RuntimeEnumDescriptor> GetRuntimeEnums() {
    return Generated::RuntimeEnums;
}

const RuntimeEnumDescriptor* FindRuntimeEnum(std::string_view qualifiedName) {
    for (const RuntimeEnumDescriptor& enumType : GetRuntimeEnums()) {
        if (enumType.QualifiedName == qualifiedName) {
            return &enumType;
        }
    }
    return nullptr;
}

const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByName(
    const RuntimeEnumDescriptor& enumType,
    std::string_view name) {
    for (const RuntimeEnumValueDescriptor& value : enumType.Values) {
        if (value.Name == name) {
            return &value;
        }
    }
    return nullptr;
}

const RuntimeEnumValueDescriptor* FindRuntimeEnumValueByValue(
    const RuntimeEnumDescriptor& enumType,
    int64_t value) {
    for (const RuntimeEnumValueDescriptor& enumValue : enumType.Values) {
        if (enumValue.Value == value) {
            return &enumValue;
        }
    }
    return nullptr;
}
```

- [ ] **Step 8: Add tool smoke enum fixtures**

In `Tests/ReflectionToolSmoke.cpp`, add a positive enum fixture in `RunNegativeValidationSmoke()` after duplicate checks:

```cpp
		const auto enumRoot = workspaceRoot / "enum_positive";
		std::filesystem::remove_all(enumRoot, errorCode);
		Expect(!errorCode, "Failed to clean enum positive fixture");
		WriteTextFile(
			enumRoot / "HuaEngine" / "src" / "Fixture" / "EnumComponent.h",
			"namespace HE {\n"
			"HE_REFLECT_" "ENUM()\n"
			"enum class FixtureMode {\n"
			"    A,\n"
			"    B = 4,\n"
			"    C,\n"
			"    D = -1,\n"
			"    E = 0x10\n"
			"};\n"
			"HE_REFLECT_" "COMPONENT(DisplayName=\"Enum Component\", Category=\"Fixture\")\n"
			"struct EnumComponent {\n"
			"    HE_REFLECT_" "FIELD()\n"
			"    FixtureMode Mode = FixtureMode::A;\n"
			"};\n"
			"}\n");
		const auto enumResult = RunCommand(
			{ "python", reflectionToolPath.string(), "validate", "--root", enumRoot.string() },
			enumRoot);
		ExpectCommandSucceeded(enumResult, "enum positive fixture");
		Expect(enumResult.Output.find("\"enums\"") != std::string::npos, "enum positive fixture should include enums in manifest");
```

Add a negative fixture:

```cpp
		ValidateNegativeFixture(
			reflectionToolPath,
			workspaceRoot,
			"enum_complex_expression",
			"namespace HE {\n"
			"HE_REFLECT_" "ENUM()\n"
			"enum class BadFlags {\n"
			"    A = 1 << 0\n"
			"};\n"
			"}\n",
			"enum.value_unsupported_expression");
```

- [ ] **Step 9: Generate and verify**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
cmake --build build --config Debug --target ReflectionToolSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
cmake --build build --config Debug --target ReflectionGeneratedSmoke
```

Expected: `validate` exits 0, `ReflectionToolSmoke passed`, and `ReflectionGeneratedSmoke` builds far enough to expose remaining test expectations in Task 3.

Commit:

```powershell
git add HuaEngine/src/HuaEngine/Reflection/Reflection.h HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h Tools/Reflection/reflection_tool.py HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp Tests/ReflectionToolSmoke.cpp Tests/ReflectionGeneratedSmoke.cpp
git commit -m "feat(reflection): add runtime field operations and enum metadata"
```

---

### Task 3: Real Engine Enum Field and Serialization Policy

**Files:**
- Modify: `HuaEngine/src/Module/Rendering/RenderingComponent.h`
- Regenerate: `HuaEngine/src/HuaEngine/Generated/GeneratedReflection.*`
- Test: `Tests/ReflectionGeneratedSmoke.cpp`
- Test: `Tests/SerializationPolicySmoke.cpp`

- [ ] **Step 1: Add engine enum and reflected field**

In `HuaEngine/src/Module/Rendering/RenderingComponent.h`, inside `namespace HE::Rendering`, before `CameraComponent`, add:

```cpp
	HE_REFLECT_ENUM(DisplayName="Material Blend Mode")
	enum class MaterialBlendMode {
		Opaque,
		Masked,
		Transparent
	};
```

In `MaterialComponent`, after `MaterialInstance`, add:

```cpp
		HE_REFLECT_FIELD()
		MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
```

- [ ] **Step 2: Extend generated smoke enum assertions**

In `Tests/ReflectionGeneratedSmoke.cpp`, after material metadata lookup or near mesh assertions, add:

```cpp
	const auto* blendModeEnum = HE::Refl::FindRuntimeEnum("HE::Rendering::MaterialBlendMode");
	Require(blendModeEnum != nullptr, "Expected MaterialBlendMode runtime enum");
	Require(blendModeEnum->Values.size() == 3, "Expected three MaterialBlendMode values");
	Require(
		HE::Refl::FindRuntimeEnumValueByName(*blendModeEnum, "Transparent") != nullptr,
		"Expected Transparent enum value");
	Require(
		HE::Refl::FindRuntimeEnumValueByValue(*blendModeEnum, static_cast<int64_t>(HE::Rendering::MaterialBlendMode::Masked)) != nullptr,
		"Expected Masked enum value by integer value");

	const auto* materialRuntime = HE::Refl::FindRuntimeType("HE::Rendering::MaterialComponent");
	Require(materialRuntime != nullptr, "Expected MaterialComponent runtime descriptor");
	const HE::Refl::RuntimeFieldDescriptor* blendModeField = FindRuntimeField(materialRuntime->Fields, "BlendMode");
	Require(blendModeField != nullptr, "Expected MaterialComponent BlendMode field");
	Require(blendModeField->EnumType == blendModeEnum, "Expected BlendMode field to reference enum metadata");
	Require(
		HE::Refl::GetRuntimeFieldValueKind(*blendModeField) == HE::Refl::RuntimeFieldValueKind::Enum,
		"Expected BlendMode to be classified as enum");

	HE::Rendering::MaterialComponent materialComponent;
	Require(
		HE::Refl::SetRuntimeEnumFieldValueByName(*blendModeField, &materialComponent, "Transparent"),
		"Expected generic enum set by name to succeed");
	int64_t enumRuntimeValue = 0;
	Require(
		HE::Refl::GetRuntimeEnumFieldValue(*blendModeField, &materialComponent, enumRuntimeValue),
		"Expected generic enum get to succeed");
	Require(
		enumRuntimeValue == static_cast<int64_t>(HE::Rendering::MaterialBlendMode::Transparent),
		"Expected enum runtime value to match Transparent");
```

Add enum serialization assertions:

```cpp
	HE::Serialization::JsonSerializationBackend materialWriteBackend;
	HE::Refl::SerializeRuntimeObject(*materialRuntime, materialWriteBackend, "MaterialComponent", &materialComponent);
	const std::string materialJson = materialWriteBackend.SaveToString();
	Require(materialJson.find("\"BlendMode\": \"Transparent\"") != std::string::npos, "Expected enum to serialize as string name");

	HE::Rendering::MaterialComponent loadedMaterial;
	HE::Serialization::JsonSerializationBackend materialReadBackend;
	materialReadBackend.LoadFromString(materialJson);
	Require(
		HE::Refl::DeserializeRuntimeObject(*materialRuntime, materialReadBackend, "MaterialComponent", &loadedMaterial),
		"Expected enum string deserialization to succeed");
	Require(loadedMaterial.BlendMode == HE::Rendering::MaterialBlendMode::Transparent, "Expected enum round-trip");
```

- [ ] **Step 3: Implement enum field helper functions**

In `Reflection.h`, add:

```cpp
inline bool GetRuntimeEnumFieldValue(const RuntimeFieldDescriptor& field, const void* object, int64_t& outValue) {
    if (field.EnumType == nullptr) {
        return false;
    }
    const void* value = GetRuntimeFieldConst(field, object);
    if (value == nullptr) {
        return false;
    }
    switch (field.Size) {
        case sizeof(int8_t): outValue = *static_cast<const int8_t*>(value); return true;
        case sizeof(int16_t): outValue = *static_cast<const int16_t*>(value); return true;
        case sizeof(int32_t): outValue = *static_cast<const int32_t*>(value); return true;
        case sizeof(int64_t): outValue = *static_cast<const int64_t*>(value); return true;
        default: return false;
    }
}

inline bool SetRuntimeEnumFieldValue(const RuntimeFieldDescriptor& field, void* object, int64_t value) {
    if (field.EnumType == nullptr || !IsRuntimeFieldEditable(field) ||
        FindRuntimeEnumValueByValue(*field.EnumType, value) == nullptr) {
        return false;
    }
    void* target = GetRuntimeFieldMutable(field, object);
    if (target == nullptr) {
        return false;
    }
    switch (field.Size) {
        case sizeof(int8_t): *static_cast<int8_t*>(target) = static_cast<int8_t>(value); return true;
        case sizeof(int16_t): *static_cast<int16_t*>(target) = static_cast<int16_t>(value); return true;
        case sizeof(int32_t): *static_cast<int32_t*>(target) = static_cast<int32_t>(value); return true;
        case sizeof(int64_t): *static_cast<int64_t*>(target) = static_cast<int64_t>(value); return true;
        default: return false;
    }
}

inline bool SetRuntimeEnumFieldValueByName(const RuntimeFieldDescriptor& field, void* object, std::string_view valueName) {
    if (field.EnumType == nullptr) {
        return false;
    }
    const RuntimeEnumValueDescriptor* value = FindRuntimeEnumValueByName(*field.EnumType, valueName);
    return value != nullptr && SetRuntimeEnumFieldValue(field, object, value->Value);
}
```

- [ ] **Step 4: Add serialization policy failure case**

In `Tests/SerializationPolicySmoke.cpp`, add includes:

```cpp
#include "Module/Rendering/RenderingComponent.h"
```

Add a test block near existing bad field policy tests:

```cpp
	{
		const auto* materialDescriptor = HE::Refl::FindRuntimeType("HE::Rendering::MaterialComponent");
		Require(materialDescriptor != nullptr, "Expected MaterialComponent runtime descriptor");

		HE::Rendering::MaterialComponent material;
		material.BlendMode = HE::Rendering::MaterialBlendMode::Masked;

		HE::Serialization::JsonSerializationBackend backend;
		backend.LoadFromString("{\"MaterialComponent\":{\"BlendMode\":\"DoesNotExist\"}}");
		Require(
			!HE::Refl::DeserializeRuntimeObject(*materialDescriptor, backend, "MaterialComponent", &material),
			"Expected unknown enum string to fail deserialization");
		Require(
			material.BlendMode == HE::Rendering::MaterialBlendMode::Masked,
			"Expected failed enum deserialization not to overwrite existing enum value");
	}
```

- [ ] **Step 5: Regenerate and verify enum round-trip**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
cmake --build build --config Debug --target ReflectionGeneratedSmoke
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
cmake --build build --config Debug --target SerializationPolicySmoke
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
```

Expected: both smoke executables print passed.

Commit:

```powershell
git add HuaEngine/src/Module/Rendering/RenderingComponent.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp HuaEngine/src/HuaEngine/Reflection/Reflection.h Tests/ReflectionGeneratedSmoke.cpp Tests/SerializationPolicySmoke.cpp
git commit -m "feat(reflection): serialize enum fields by name"
```

---

### Task 4: Runtime Inspector Uses Core Field Kind

**Files:**
- Modify: `Editor/src/Panels/RuntimeInspector.h`
- Modify: `Editor/src/Panels/RuntimeInspector.cpp`
- Test: `Tests/EditorInspectorRuntimeSmoke.cpp`

- [ ] **Step 1: Update failing editor smoke expectations**

In `Tests/EditorInspectorRuntimeSmoke.cpp`, replace `RuntimeFieldEditKind` comparisons with `RuntimeFieldValueKind`:

```cpp
Require(
	HE::Refl::GetRuntimeFieldValueKind(*position) == HE::Refl::RuntimeFieldValueKind::Float3,
	"Expected Position to use Float3 runtime field kind");
```

Apply equivalent replacements:

- `Primary` -> `Bool`
- `MeshAssetName` -> `String`
- `MaterialInstance` -> `AssetRef`
- `BlendMode` -> `Enum`

Add:

```cpp
const auto* blendMode = FindField(material->RuntimeType->Fields, "BlendMode");
Require(blendMode != nullptr, "Expected MaterialComponent BlendMode field");
Require(
	HE::Refl::GetRuntimeFieldValueKind(*blendMode) == HE::Refl::RuntimeFieldValueKind::Enum,
	"Expected BlendMode to use Enum runtime field kind");
```

Run:

```powershell
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
```

Expected: compile fails because `RuntimeFieldEditKind` still exists in editor helper.

- [ ] **Step 2: Remove editor-private field kind**

In `Editor/src/Panels/RuntimeInspector.h`, delete:

```cpp
	enum class RuntimeFieldEditKind { ... };
	[[nodiscard]] RuntimeFieldEditKind GetRuntimeFieldEditKind(const Refl::RuntimeFieldDescriptor& field);
```

Keep:

```cpp
	[[nodiscard]] bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field);
```

- [ ] **Step 3: Use core kind in runtime inspector**

In `RuntimeInspector.cpp`:

- Delete local `IsAnyOf`.
- Delete `GetRuntimeFieldEditKind`.
- Change `IsRuntimeFieldEditable` implementation to:

```cpp
	bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field) {
		return Refl::IsRuntimeFieldEditable(field);
	}
```

In `DrawRuntimeFieldEditor`, switch on:

```cpp
		switch (Refl::GetRuntimeFieldValueKind(field)) {
```

Map cases:

- `Refl::RuntimeFieldValueKind::Bool`
- `SignedInteger`
- `UnsignedInteger`
- `Float`
- `Double`
- `String`
- `Float2`
- `Float3`
- `Float4`
- `Enum`
- default unsupported

For unsupported text, keep:

```cpp
ImGui::TextDisabled("%s: unsupported %.*s", label, static_cast<int>(field.Type.size()), field.Type.data());
```

- [ ] **Step 4: Add enum combo drawing**

In `RuntimeInspector.cpp`, add helper:

```cpp
		bool DrawRuntimeEnumField(const Refl::RuntimeFieldDescriptor& field, void* component, const char* label) {
			if (field.EnumType == nullptr) {
				ImGui::TextDisabled("%s: enum metadata unavailable", label);
				return false;
			}

			int64_t currentValue = 0;
			if (!Refl::GetRuntimeEnumFieldValue(field, component, currentValue)) {
				ImGui::TextDisabled("%s: enum value unavailable", label);
				return false;
			}

			const Refl::RuntimeEnumValueDescriptor* current =
				Refl::FindRuntimeEnumValueByValue(*field.EnumType, currentValue);
			const char* preview = current != nullptr
				? (current->DisplayName.empty() ? current->Name.data() : current->DisplayName.data())
				: "<unknown>";

			bool changed = false;
			if (ImGui::BeginCombo(label, preview)) {
				for (const Refl::RuntimeEnumValueDescriptor& value : field.EnumType->Values) {
					const bool selected = value.Value == currentValue;
					const char* itemLabel = value.DisplayName.empty() ? value.Name.data() : value.DisplayName.data();
					if (ImGui::Selectable(itemLabel, selected)) {
						changed = Refl::SetRuntimeEnumFieldValue(field, component, value.Value);
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			return changed;
		}
```

In switch:

```cpp
			case Refl::RuntimeFieldValueKind::Enum:
				changed = DrawRuntimeEnumField(field, component, label);
				break;
```

- [ ] **Step 5: Verify and commit inspector migration**

Run:

```powershell
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
cmake --build build --config Debug --target Editor
```

Expected: smoke prints `EditorInspectorRuntimeSmoke passed`, Editor builds.

Commit:

```powershell
git add Editor/src/Panels/RuntimeInspector.h Editor/src/Panels/RuntimeInspector.cpp Tests/EditorInspectorRuntimeSmoke.cpp
git commit -m "refactor(editor): use core runtime field kinds"
```

---

### Task 5: Final Verification and Drift Guard

**Files:**
- Verify all modified files.

- [ ] **Step 1: Run reflection tool drift validation**

Run:

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
git diff -- HuaEngine/src/HuaEngine/Generated/GeneratedReflection.h HuaEngine/src/HuaEngine/Generated/GeneratedReflection.cpp
python Tools/Reflection/reflection_tool.py validate --root .
```

Expected:

- `git diff -- GeneratedReflection.*` has no output after generation.
- `validate` exits 0.

- [ ] **Step 2: Build required targets**

Run serially to avoid MSBuild tlog locks:

```powershell
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target ReflectionToolSmoke
cmake --build build --config Debug --target SerializationPolicySmoke
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
cmake --build build --config Debug --target CLIReflectionSmoke
```

Expected: all builds exit 0.

- [ ] **Step 3: Run smoke executables**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIReflectionSmoke.exe
```

Expected output includes:

```text
ReflectionGeneratedSmoke passed
ReflectionToolSmoke passed
SerializationPolicySmoke passed
EditorInspectorRuntimeSmoke passed
CLIReflectionSmoke passed
```

- [ ] **Step 4: Search for stale editor field kind**

Run:

```powershell
rg -n "RuntimeFieldEditKind|GetRuntimeFieldEditKind" Editor/src Tests
rg -n "HE_REFLECT_ENUM|RuntimeEnumDescriptor|RuntimeFieldValueKind|BlendMode" HuaEngine/src Editor/src Tests Tools/Reflection
```

Expected:

- First command returns no matches.
- Second command returns matches in reflection core, generator, generated files, rendering component, tests, and runtime inspector.

- [ ] **Step 5: Commit verification fixes when files changed**

If verification caused file changes, commit them:

```powershell
git add <changed-files>
git commit -m "fix(reflection): address runtime field operation verification"
```

If no files changed, do not create an empty commit.

---

## Self-Review Notes

- Spec coverage: plan covers `RuntimeFieldValueKind`, generic field get/set/copy, enum descriptors, `HE_REFLECT_ENUM`, enum string serialization, Editor combo behavior, tool diagnostics, and smoke tests.
- Scope: plan does not add C# and does not rewrite provider registry.
- Type consistency: `RuntimeFieldValueKind` belongs to `HE::Refl`; Editor consumes it instead of defining `RuntimeFieldEditKind`.
- Implementation boundary: enum support is intentionally limited to simple values; complex expressions produce `enum.value_unsupported_expression`.
