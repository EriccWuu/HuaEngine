# Editor Runtime Inspector Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 Editor Inspector 的组件显示与字段编辑主路径迁移到 `RuntimeTypeDescriptor` / `RuntimeFieldDescriptor`，删除旧 `ComponentEditorRegistry` / `REGISTER_COMPONENT_EDITOR` 注册逻辑。

**Architecture:** Inspector 从实体真实持有的 runtime component type ids 出发，通过 `ComponentRegistry` 找到 `RuntimeTypeDescriptor`，再用 generic runtime field editor 绘制字段。保留一个新的 runtime override registry 接口作为未来扩展点，但本阶段不为现有组件注册 override；复杂字段统一显示 unsupported/disabled UI。

**Tech Stack:** C++20, ImGui, HuaEngine ECS, `HE::Refl` runtime descriptors, Editor command stack, CMake/MSBuild smoke tests.

---

## File Structure

- Create: `Editor/src/Panels/RuntimeInspector.h`
  - 声明 runtime inspector helper、field edit kind、override registry、generic draw entrypoints。
- Create: `Editor/src/Panels/RuntimeInspector.cpp`
  - 实现 runtime field type 分派、ImGui field controls、component header/body 绘制、unsupported display。
- Modify: `Editor/src/Panels/InspectorPanel.h`
  - 移除 `ComponentEditorRegistry.h` include。
  - 持有 `ComponentRegistry m_ComponentRegistry` 和 `RuntimeComponentEditorOverrideRegistry m_RuntimeOverrides`。
  - 将构造函数移到 cpp 初始化 registry。
- Modify: `Editor/src/Panels/InspectorPanel.cpp`
  - 从 `ComponentEditorRegistry::DrawComponents()` 改为 entity runtime component list + `DrawRuntimeComponentInspector()`。
  - Add Component 窗口从 `ComponentRegistry::GetAll()` 获取候选。
  - Remove context menu 使用 runtime descriptor qualified name 映射到现有 `EditorInspectableComponent` command。
- Delete: `Editor/src/ComponentEditorRegistry.h`
  - 删除旧手写组件注册逻辑和 `REGISTER_COMPONENT_EDITOR` 宏。
- Delete: `Editor/src/ComponentEditor.h`
  - 删除旧 `Refl::reflect<T>()` 静态反射 editor。
- Create: `Tests/EditorInspectorRuntimeSmoke.cpp`
  - 覆盖 runtime metadata 枚举、field edit kind 分派、core components generic inspector eligibility、旧注册字符串不再存在。
- Modify: `CMakeLists.txt`
  - 新增 `EditorInspectorRuntimeSmoke` target，链接 `HuaEngine` / `ImGui`，包含 `Editor/src/Panels/RuntimeInspector.cpp`。
- No change expected: `Editor/CMakeLists.txt`
  - Editor target 用 `file(GLOB_RECURSE EDITOR_SOURCES "src/*.cpp" "src/*.h")`，新增/删除 Editor 源文件会自动进入/退出 Editor target。

---

### Task 1: Add Runtime Inspector Helper Layer

**Files:**
- Create: `Editor/src/Panels/RuntimeInspector.h`
- Create: `Editor/src/Panels/RuntimeInspector.cpp`
- Test: `Tests/EditorInspectorRuntimeSmoke.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add failing smoke target skeleton**

Create `Tests/EditorInspectorRuntimeSmoke.cpp` with:

```cpp
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Generated/GeneratedReflection.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "Module/Rendering/RenderingComponent.h"
#include "Panels/RuntimeInspector.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[EditorInspectorRuntimeSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	const HE::Refl::RuntimeFieldDescriptor* FindField(
		std::span<const HE::Refl::RuntimeFieldDescriptor> fields,
		std::string_view name) {
		for (const auto& field : fields) {
			if (field.Name == name) {
				return &field;
			}
		}
		return nullptr;
	}
}

int main() {
	HE::ComponentRegistry registry;
	HE::RegisterCoreComponents(registry);

	const HE::ComponentMetadata* transform = registry.FindByType<HE::TransformComponent>();
	Require(transform != nullptr, "Expected TransformComponent metadata");
	Require(transform->RuntimeType != nullptr, "Expected TransformComponent runtime type");
	const auto* position = FindField(transform->RuntimeType->Fields, "Position");
	Require(position != nullptr, "Expected TransformComponent Position field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*position) == HE::Editor::RuntimeFieldEditKind::Float3,
		"Expected Position to use Float3 runtime editor");

	const HE::ComponentMetadata* camera = registry.FindByType<HE::Rendering::CameraComponent>();
	Require(camera != nullptr, "Expected CameraComponent metadata");
	const auto* primary = FindField(camera->RuntimeType->Fields, "Primary");
	Require(primary != nullptr, "Expected CameraComponent Primary field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*primary) == HE::Editor::RuntimeFieldEditKind::Bool,
		"Expected CameraComponent Primary to use Bool runtime editor");

	const HE::ComponentMetadata* mesh = registry.FindByType<HE::Rendering::MeshComponent>();
	Require(mesh != nullptr, "Expected MeshComponent metadata");
	const auto* meshAssetName = FindField(mesh->RuntimeType->Fields, "MeshAssetName");
	Require(meshAssetName != nullptr, "Expected MeshComponent MeshAssetName field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*meshAssetName) == HE::Editor::RuntimeFieldEditKind::String,
		"Expected MeshAssetName to use String runtime editor");

	const HE::ComponentMetadata* material = registry.FindByType<HE::Rendering::MaterialComponent>();
	Require(material != nullptr, "Expected MaterialComponent metadata");
	const auto* materialInstance = FindField(material->RuntimeType->Fields, "MaterialInstance");
	Require(materialInstance != nullptr, "Expected MaterialComponent MaterialInstance field");
	Require(
		HE::Editor::GetRuntimeFieldEditKind(*materialInstance) == HE::Editor::RuntimeFieldEditKind::Unsupported,
		"Expected MaterialInstance to use unsupported runtime editor");

	std::cout << "EditorInspectorRuntimeSmoke passed" << std::endl;
	return 0;
}
```

`RendererComponent` is explicitly marked as a deprecated legacy component in `RenderingComponent.h` and is not present in the generated runtime reflection metadata. This plan removes its old hand-written Inspector registration instead of migrating it as a current editable component. If Renderer needs Inspector support later, first add official runtime reflection metadata for it.

Modify `CMakeLists.txt` near `EditorInteractionSmoke`:

```cmake
add_executable(EditorInspectorRuntimeSmoke
    Tests/EditorInspectorRuntimeSmoke.cpp
    Editor/src/Panels/RuntimeInspector.cpp
)
target_include_directories(EditorInspectorRuntimeSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Editor/src
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(EditorInspectorRuntimeSmoke PRIVATE HuaEngine ImGui)
if(WIN32)
    target_compile_definitions(EditorInspectorRuntimeSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(EditorInspectorRuntimeSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(EditorInspectorRuntimeSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(EditorInspectorRuntimeSmoke)
```

Add folder property near other test target folder assignments:

```cmake
set_property(TARGET EditorInspectorRuntimeSmoke PROPERTY FOLDER "Tests")
```

Run:

```powershell
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
```

Expected: fail because `Panels/RuntimeInspector.h` does not exist.

- [ ] **Step 2: Create runtime inspector header**

Create `Editor/src/Panels/RuntimeInspector.h`:

```cpp
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "HuaEngine/Reflection/Reflection.h"

namespace HE::Editor {
	enum class RuntimeFieldEditKind {
		Unsupported,
		Bool,
		Int,
		UInt,
		Float,
		Double,
		String,
		Float2,
		Float3,
		Float4,
	};

	using RuntimeComponentEditorOverride =
		std::function<bool(const Refl::RuntimeTypeDescriptor&, void*)>;

	class RuntimeComponentEditorOverrideRegistry {
	public:
		void RegisterOverride(std::string_view qualifiedName, RuntimeComponentEditorOverride editor);
		[[nodiscard]] const RuntimeComponentEditorOverride* FindOverride(std::string_view qualifiedName) const;

	private:
		std::unordered_map<std::string, RuntimeComponentEditorOverride> m_Overrides;
	};

	[[nodiscard]] RuntimeFieldEditKind GetRuntimeFieldEditKind(const Refl::RuntimeFieldDescriptor& field);
	[[nodiscard]] bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field);
	[[nodiscard]] std::string GetRuntimeComponentDisplayName(const Refl::RuntimeTypeDescriptor& type);

	bool DrawRuntimeFieldEditor(const Refl::RuntimeFieldDescriptor& field, void* component);
	bool DrawRuntimeComponentInspector(
		const Refl::RuntimeTypeDescriptor& type,
		void* component,
		const RuntimeComponentEditorOverrideRegistry& overrides);
}
```

- [ ] **Step 3: Implement pure helper functions and override registry**

Create `Editor/src/Panels/RuntimeInspector.cpp` with:

```cpp
#include "RuntimeInspector.h"

#include <array>
#include <cstring>

#include "imgui.h"
#include "glm/glm.hpp"

namespace HE::Editor {
	namespace {
		bool IsAnyOf(std::string_view value, std::initializer_list<std::string_view> candidates) {
			for (std::string_view candidate : candidates) {
				if (value == candidate) {
					return true;
				}
			}
			return false;
		}

		const char* FieldLabel(const Refl::RuntimeFieldDescriptor& field) {
			return field.DisplayName.empty() ? field.Name.data() : field.DisplayName.data();
		}
	}

	void RuntimeComponentEditorOverrideRegistry::RegisterOverride(
		std::string_view qualifiedName,
		RuntimeComponentEditorOverride editor) {
		if (qualifiedName.empty() || !editor) {
			return;
		}
		m_Overrides[std::string(qualifiedName)] = std::move(editor);
	}

	const RuntimeComponentEditorOverride* RuntimeComponentEditorOverrideRegistry::FindOverride(
		std::string_view qualifiedName) const {
		const auto iterator = m_Overrides.find(std::string(qualifiedName));
		return iterator != m_Overrides.end() ? &iterator->second : nullptr;
	}

	RuntimeFieldEditKind GetRuntimeFieldEditKind(const Refl::RuntimeFieldDescriptor& field) {
		if (!Refl::HasRuntimeFieldFlag(field.Flags, Refl::RuntimeFieldFlags::Serializable) ||
			field.GetMutable == nullptr) {
			return RuntimeFieldEditKind::Unsupported;
		}

		if (field.Type == "bool") {
			return RuntimeFieldEditKind::Bool;
		}
		if (IsAnyOf(field.Type, { "int", "int8_t", "int16_t", "int32_t", "int64_t" })) {
			return RuntimeFieldEditKind::Int;
		}
		if (IsAnyOf(field.Type, { "unsigned int", "uint8_t", "uint16_t", "uint32_t", "uint64_t" })) {
			return RuntimeFieldEditKind::UInt;
		}
		if (field.Type == "float") {
			return RuntimeFieldEditKind::Float;
		}
		if (field.Type == "double") {
			return RuntimeFieldEditKind::Double;
		}
		if (field.Type == "std::string") {
			return RuntimeFieldEditKind::String;
		}
		if (field.Type == "glm::vec2") {
			return RuntimeFieldEditKind::Float2;
		}
		if (field.Type == "glm::vec3") {
			return RuntimeFieldEditKind::Float3;
		}
		if (field.Type == "glm::vec4") {
			return RuntimeFieldEditKind::Float4;
		}

		return RuntimeFieldEditKind::Unsupported;
	}

	bool IsRuntimeFieldEditable(const Refl::RuntimeFieldDescriptor& field) {
		return GetRuntimeFieldEditKind(field) != RuntimeFieldEditKind::Unsupported;
	}

	std::string GetRuntimeComponentDisplayName(const Refl::RuntimeTypeDescriptor& type) {
		if (!type.DisplayName.empty()) {
			return std::string(type.DisplayName);
		}
		if (!type.Name.empty()) {
			return std::string(type.Name);
		}
		return std::string(type.QualifiedName);
	}
}
```

- [ ] **Step 4: Implement ImGui field editors and component draw entrypoint**

Append to `Editor/src/Panels/RuntimeInspector.cpp`:

```cpp
namespace HE::Editor {
	bool DrawRuntimeFieldEditor(const Refl::RuntimeFieldDescriptor& field, void* component) {
		if (component == nullptr || field.GetMutable == nullptr) {
			ImGui::TextDisabled("%s: unavailable", FieldLabel(field));
			return false;
		}

		void* value = field.GetMutable(component);
		if (value == nullptr) {
			ImGui::TextDisabled("%s: unavailable", FieldLabel(field));
			return false;
		}

		ImGui::PushID(field.Name.data());
		bool changed = false;
		const char* label = FieldLabel(field);
		switch (GetRuntimeFieldEditKind(field)) {
			case RuntimeFieldEditKind::Bool:
				changed = ImGui::Checkbox(label, static_cast<bool*>(value));
				break;
			case RuntimeFieldEditKind::Int:
				changed = ImGui::DragScalar(label, ImGuiDataType_S32, value, 1.0f);
				break;
			case RuntimeFieldEditKind::UInt:
				changed = ImGui::DragScalar(label, ImGuiDataType_U32, value, 1.0f);
				break;
			case RuntimeFieldEditKind::Float:
				changed = ImGui::DragFloat(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Double:
				changed = ImGui::DragScalar(label, ImGuiDataType_Double, value, 0.1f);
				break;
			case RuntimeFieldEditKind::String: {
				auto& text = *static_cast<std::string*>(value);
				std::array<char, 256> buffer{};
				const size_t copyLength = (std::min)(text.size(), buffer.size() - 1);
				std::memcpy(buffer.data(), text.data(), copyLength);
				if (ImGui::InputText(label, buffer.data(), buffer.size())) {
					text = buffer.data();
					changed = true;
				}
				break;
			}
			case RuntimeFieldEditKind::Float2:
				changed = ImGui::DragFloat2(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Float3:
				changed = ImGui::DragFloat3(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Float4:
				changed = ImGui::DragFloat4(label, static_cast<float*>(value), 0.1f);
				break;
			case RuntimeFieldEditKind::Unsupported:
			default:
				ImGui::TextDisabled("%s: unsupported %.*s", label, static_cast<int>(field.Type.size()), field.Type.data());
				break;
		}
		ImGui::PopID();
		return changed;
	}

	bool DrawRuntimeComponentInspector(
		const Refl::RuntimeTypeDescriptor& type,
		void* component,
		const RuntimeComponentEditorOverrideRegistry& overrides) {
		if (component == nullptr) {
			return false;
		}

		if (const RuntimeComponentEditorOverride* editor = overrides.FindOverride(type.QualifiedName)) {
			return (*editor)(type, component);
		}

		bool changed = false;
		for (const Refl::RuntimeFieldDescriptor& field : type.Fields) {
			changed |= DrawRuntimeFieldEditor(field, component);
		}
		return changed;
	}
}
```

- [ ] **Step 5: Build and run smoke**

Run:

```powershell
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
```

Expected: build succeeds and executable prints `EditorInspectorRuntimeSmoke passed`.

- [ ] **Step 6: Commit runtime inspector helper**

```powershell
git add Editor/src/Panels/RuntimeInspector.* Tests/EditorInspectorRuntimeSmoke.cpp CMakeLists.txt
git commit -m "feat(editor): add runtime inspector helpers"
```

---

### Task 2: Route Inspector Display Through Runtime Components

**Files:**
- Modify: `Editor/src/Panels/InspectorPanel.h`
- Modify: `Editor/src/Panels/InspectorPanel.cpp`
- Test: `Tests/EditorInspectorRuntimeSmoke.cpp`

- [ ] **Step 1: Update InspectorPanel members and constructor**

In `Editor/src/Panels/InspectorPanel.h`:

- Remove:

```cpp
#include "ComponentEditorRegistry.h"
```

- Add:

```cpp
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "Panels/RuntimeInspector.h"
```

- Change constructor declaration:

```cpp
InspectorPanel();
```

- Add members:

```cpp
ComponentRegistry m_ComponentRegistry;
Editor::RuntimeComponentEditorOverrideRegistry m_RuntimeOverrides;
```

In `Editor/src/Panels/InspectorPanel.cpp`, add:

```cpp
InspectorPanel::InspectorPanel() {
	RegisterCoreComponents(m_ComponentRegistry);
}
```

- [ ] **Step 2: Replace component drawing call**

In `InspectorPanel.cpp`, replace the block that calls `ComponentEditorRegistry::Instance().DrawComponents(...)` with a loop over runtime component ids:

```cpp
for (const ComponentTypeId typeId : selection.ListComponentTypes()) {
	const ComponentMetadata* metadata = m_ComponentRegistry.FindByTypeId(typeId);
	void* component = selection.TryGetComponentByType(typeId);
	if (metadata == nullptr || component == nullptr) {
		continue;
	}

	const Refl::RuntimeTypeDescriptor* runtimeType = metadata->RuntimeType;
	const std::string displayName = runtimeType != nullptr
		? Editor::GetRuntimeComponentDisplayName(*runtimeType)
		: metadata->TypeName;

	const bool open = ImGui::CollapsingHeader(displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
	if (ImGui::BeginPopupContextItem((displayName + "ContextMenu").c_str())) {
		const auto* descriptor = FindInspectableComponentByQualifiedName(runtimeType != nullptr ? runtimeType->QualifiedName : std::string_view{});
		const bool canRemove = descriptor != nullptr && CanRemoveInspectableComponent(descriptor->Type, selection);
		if (ImGui::MenuItem("Remove Component", nullptr, false, canRemove)) {
			if (m_RemoveComponentCallback) {
				m_RemoveComponentCallback(descriptor->Type);
			}
			ImGui::CloseCurrentPopup();
		}
		if (descriptor == nullptr) {
			ImGui::TextDisabled("Runtime remove command not available");
		}
		ImGui::EndPopup();
	}

	if (open) {
		ImGui::Indent();
		if (runtimeType != nullptr) {
			changed |= Editor::DrawRuntimeComponentInspector(*runtimeType, component, m_RuntimeOverrides);
		}
		else {
			ImGui::TextDisabled("Runtime descriptor unavailable");
		}
		ImGui::Unindent();
	}
}
```

- [ ] **Step 3: Replace type-index mapping helper**

Remove `FindInspectableComponentByType(std::type_index type)`.

Add helper in the anonymous namespace:

```cpp
const EditorInspectableComponentDescriptor* FindInspectableComponentByQualifiedName(std::string_view qualifiedName) {
	if (qualifiedName == "HE::Rendering::CameraComponent") {
		return FindEditorInspectableComponent("component.camera");
	}
	if (qualifiedName == "HE::Rendering::MeshComponent") {
		return FindEditorInspectableComponent("component.mesh");
	}
	if (qualifiedName == "HE::Rendering::MaterialComponent") {
		return FindEditorInspectableComponent("component.material");
	}
	return nullptr;
}
```

Remove `#include <typeindex>` from `InspectorPanel.cpp`.

- [ ] **Step 4: Build Editor**

Run:

```powershell
cmake --build build --config Debug --target Editor
```

Expected: Editor builds successfully.

- [ ] **Step 5: Commit runtime component display route**

```powershell
git add Editor/src/Panels/InspectorPanel.*
git commit -m "refactor(editor): draw inspector components from runtime metadata"
```

---

### Task 3: Route Add Component Window Through ComponentRegistry

**Files:**
- Modify: `Editor/src/Panels/InspectorPanel.cpp`
- Test: `Tests/EditorInspectorRuntimeSmoke.cpp`

- [ ] **Step 1: Add pure mapping helper coverage**

In `Tests/EditorInspectorRuntimeSmoke.cpp`, after material assertions, add assertions for display names:

```cpp
Require(
	HE::Editor::GetRuntimeComponentDisplayName(*transform->RuntimeType) == "Transform",
	"Expected Transform display name from runtime metadata");
Require(
	HE::Editor::GetRuntimeComponentDisplayName(*camera->RuntimeType) == "Camera",
	"Expected Camera display name from runtime metadata");
```

Run:

```powershell
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
```

Expected: pass.

- [ ] **Step 2: Replace Add Component candidate loop**

In `InspectorPanel::DrawAddComponentWindow()`, replace:

```cpp
for (const auto& descriptor : GetEditorInspectableComponents()) {
    ...
}
```

with:

```cpp
for (const ComponentMetadata& metadata : m_ComponentRegistry.GetAll()) {
	const Refl::RuntimeTypeDescriptor* runtimeType = metadata.RuntimeType;
	if (runtimeType == nullptr || metadata.TypeId == InvalidComponentTypeId) {
		continue;
	}

	const std::string displayName = Editor::GetRuntimeComponentDisplayName(*runtimeType);
	const bool alreadyHas = selection.TryGetComponentByType(metadata.TypeId) != nullptr;
	const auto* descriptor = FindInspectableComponentByQualifiedName(runtimeType->QualifiedName);
	const bool canAdd = descriptor != nullptr && metadata.ConstructDefault != nullptr && metadata.AddCopyToWorld != nullptr && !alreadyHas;

	if (ImGui::Selectable(displayName.c_str(), false, 0, ImVec2(0.0f, 0.0f)) && canAdd) {
		if (m_AddComponentCallback) {
			m_AddComponentCallback(descriptor->Type);
		}
		m_ShowAddComponentWindow = false;
	}

	if (alreadyHas) {
		ImGui::SameLine();
		ImGui::TextDisabled("(Already Added)");
	}
	else if (descriptor == nullptr) {
		ImGui::SameLine();
		ImGui::TextDisabled("(Runtime add unavailable)");
	}

	if (!canAdd && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		if (alreadyHas) {
			ImGui::SetTooltip("%s already exists on the selected entity.", displayName.c_str());
		}
		else if (descriptor == nullptr) {
			ImGui::SetTooltip("Runtime add command is not available for %s.", displayName.c_str());
		}
	}
}
```

This makes `ComponentRegistry::GetAll()` the candidate source while preserving existing add command limits.

- [ ] **Step 3: Build Editor and smoke**

Run:

```powershell
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
```

Expected: all pass.

- [ ] **Step 4: Commit add component runtime source**

```powershell
git add Editor/src/Panels/InspectorPanel.cpp Tests/EditorInspectorRuntimeSmoke.cpp
git commit -m "refactor(editor): source add component list from runtime registry"
```

---

### Task 4: Remove Legacy Component Editor Registration

**Files:**
- Delete: `Editor/src/ComponentEditorRegistry.h`
- Delete: `Editor/src/ComponentEditor.h`
- Test: `Tests/EditorInspectorRuntimeSmoke.cpp`

- [ ] **Step 1: Add source regression checks**

In `Tests/EditorInspectorRuntimeSmoke.cpp`, add helpers:

```cpp
std::string ReadTextFile(const std::filesystem::path& path) {
	std::ifstream file(path);
	Require(file.is_open(), "Expected file to be readable: " + path.string());
	std::ostringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

std::filesystem::path FindRepositoryRoot() {
	std::filesystem::path current = std::filesystem::current_path();
	for (int depth = 0; depth < 8; ++depth) {
		if (std::filesystem::exists(current / "Editor" / "src" / "Panels" / "InspectorPanel.cpp") &&
			std::filesystem::exists(current / "HuaEngine" / "src")) {
			return current;
		}
		current = current.parent_path();
	}
	return std::filesystem::current_path();
}
```

At the end of `main()`, add:

```cpp
const std::filesystem::path repositoryRoot = FindRepositoryRoot();
const std::string inspectorSource = ReadTextFile(repositoryRoot / "Editor" / "src" / "Panels" / "InspectorPanel.cpp");
Require(
	inspectorSource.find("ComponentEditorRegistry") == std::string::npos,
	"Expected InspectorPanel not to use legacy ComponentEditorRegistry");
Require(
	inspectorSource.find("Refl::reflect") == std::string::npos,
	"Expected InspectorPanel not to use static reflection editor path");
Require(
	!std::filesystem::exists(repositoryRoot / "Editor" / "src" / "ComponentEditorRegistry.h"),
	"Expected legacy ComponentEditorRegistry.h to be removed");
Require(
	!std::filesystem::exists(repositoryRoot / "Editor" / "src" / "ComponentEditor.h"),
	"Expected legacy ComponentEditor.h to be removed");
```

Run:

```powershell
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
```

Expected: fail until legacy files are deleted.

- [ ] **Step 2: Delete legacy headers**

Delete:

```text
Editor/src/ComponentEditorRegistry.h
Editor/src/ComponentEditor.h
```

No replacement include should reference either file.

- [ ] **Step 3: Search for old registration logic**

Run:

```powershell
rg -n "ComponentEditorRegistry|REGISTER_COMPONENT_EDITOR|DrawComponentEditor|Refl::reflect<T>|ComponentEditor.h" Editor/src Tests
```

Expected: no matches except permitted historical text in docs/specs outside `Editor/src` and `Tests`.

- [ ] **Step 4: Build and run regression**

Run:

```powershell
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
```

Expected: all pass.

- [ ] **Step 5: Commit legacy removal**

```powershell
git add Editor/src/ComponentEditorRegistry.h Editor/src/ComponentEditor.h Tests/EditorInspectorRuntimeSmoke.cpp
git commit -m "refactor(editor): remove legacy component editor registration"
```

---

### Task 5: Final Verification and Documentation Check

**Files:**
- Modify only if verification reveals issues.

- [ ] **Step 1: Run focused source searches**

Run:

```powershell
rg -n "ComponentEditorRegistry|REGISTER_COMPONENT_EDITOR|DrawComponentEditor|Refl::reflect<T>|ComponentEditor.h" Editor/src Tests
rg -n "DrawRuntimeComponentInspector|GetRuntimeFieldEditKind|ComponentRegistry::GetAll|ListComponentTypes" Editor/src Tests/EditorInspectorRuntimeSmoke.cpp
```

Expected:

- First command has no matches.
- Second command shows runtime inspector and registry/list usage.

- [ ] **Step 2: Build targets**

Run sequentially:

```powershell
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target EditorInspectorRuntimeSmoke
cmake --build build --config Debug --target EditorInteractionSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target SerializationPolicySmoke
```

Expected: all build exit 0.

- [ ] **Step 3: Run smoke executables**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\EditorInteractionSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SerializationPolicySmoke.exe
```

Expected: all exit 0.

- [ ] **Step 4: Review git diff**

Run:

```powershell
git status --short
git diff --check
git diff --stat
```

Expected:

- Only intended files are modified/untracked.
- `git diff --check` exits 0.

- [ ] **Step 5: Commit verification fixes when verification changes files**

If verification required fixes:

```powershell
git add <fixed-files>
git commit -m "fix(editor): address runtime inspector verification issues"
```

---

## Self-Review Notes

- The plan deletes the old `ComponentEditorRegistry` / `REGISTER_COMPONENT_EDITOR` implementation, not merely bypassing it.
- The plan keeps a new runtime override interface but does not register current components as overrides.
- The Inspector display path is driven by `Entity::ListComponentTypes()` and `ComponentRegistry` runtime metadata.
- Add Component candidates come from `ComponentRegistry::GetAll()` while existing add command support remains enum-limited.
- Unsupported complex fields use generic disabled UI instead of static reflection fallback.
