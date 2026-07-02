# ECS Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 一步到位重构 HuaEngine ECS 公共层，让非 ECS backend 代码不再直接依赖 EnTT，并补齐 `World`、`Query`、`ComponentRegistry`、`Scheduler`、新 scene schema 和 Editor 集成。

**Architecture:** `Scene` 持有 `World` 与 `Scheduler`，系统通过 `SystemContext` 查询实体，Editor/Serializer/Application/Script/Rendering 全部走 ECS 公共 API。第一版使用引擎自有 type-erased component map 存储，不在公共 API 中暴露 EnTT；如保留 EnTT 依赖，只能留在 CMake 的第三方 include 中，业务代码验收不得出现 `entt::`、`entt.hpp`、`GetRegistry()`。

**Tech Stack:** C++20, CMake, ImGui, glm, existing HuaEngine serialization backend, existing smoke-test executable pattern.

---

## 前置说明

本计划对应设计文档：

`.workspace/superpower/specs/2026-07-02-ecs-refactor-design.md`

实施前先确认工作区干净：

```powershell
git status --short
```

预期：无输出。

每个任务结束都要提交。任何任务如果引入临时兼容代码，文件必须在 `HuaEngine/src/HuaEngine/ECS/Legacy/` 下，并在 Task 10 删除。

---

## 文件结构

### 新增 ECS 公共层

- Create: `HuaEngine/src/HuaEngine/ECS/EntityId.h`  
  定义 `EntityId`、`EntityUuid`、比较、哈希、字符串转换。
- Create: `HuaEngine/src/HuaEngine/ECS/ComponentType.h`  
  定义 `ComponentTypeId`、`ComponentTypeIdOf<T>()`、`Read<T>`。
- Create: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`
- Create: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`  
  管理组件元数据、构造、复制、序列化、反序列化、Editor hook。
- Create: `HuaEngine/src/HuaEngine/ECS/World.h`
- Create: `HuaEngine/src/HuaEngine/ECS/World.cpp`  
  管理实体生命周期、UUID 映射、组件存储、动态组件访问。
- Create: `HuaEngine/src/HuaEngine/ECS/Query.h`  
  模板 Query facade 和 `Read<T>` 访问语义。
- Create: `HuaEngine/src/HuaEngine/ECS/System.h`
- Create: `HuaEngine/src/HuaEngine/ECS/Scheduler.h`
- Create: `HuaEngine/src/HuaEngine/ECS/Scheduler.cpp`  
  系统描述、阶段排序、单线程执行、循环依赖检测。
- Modify: `HuaEngine/src/HuaEngine/ECS/Entity.h`
- Delete: `HuaEngine/src/HuaEngine/ECS/EntityManager.h`
- Delete: `HuaEngine/src/HuaEngine/ECS/EntityManager.cpp`
- Delete: `HuaEngine/src/HuaEngine/ECS/Syetem.h`

### 修改 Scene 与 Serializer

- Modify: `HuaEngine/src/HuaEngine/Scene/Scene.h`
- Modify: `HuaEngine/src/HuaEngine/Scene/Scene.cpp`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneSerializer.h`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneService.h`

### 修改运行时模块

- Modify: `HuaEngine/src/HuaEngine/ECS/ScriptableEntity.h`
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptService.h`
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.h`
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.cpp`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `HuaEngine/src/HuaEngine/Validation/ValidationService.cpp`
- Modify: `HuaEngine/src/HuaEngine.h`

### 修改 Editor

- Modify: `Editor/src/Selection.h`
- Modify: `Editor/src/Selection.cpp`
- Modify: `Editor/src/ComponentEditorRegistry.h`
- Modify: `Editor/src/Panels/HierarchyPanel.h`
- Modify: `Editor/src/Panels/HierarchyPanel.cpp`
- Modify: `Editor/src/Panels/InspectorPanel.h`
- Modify: `Editor/src/Panels/InspectorPanel.cpp`
- Modify: `Editor/src/Interaction/EditorSceneCommands.h`
- Modify: `Editor/src/Interaction/EditorSceneCommands.cpp`
- Modify: `Editor/src/Interaction/EditorInteractionHost.cpp`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Editor/src/Workbench/SceneDocument.h`

### 新增和更新测试

- Create: `Tests/ECSCoreSmoke.cpp`
- Create: `Tests/ECSQuerySchedulerSmoke.cpp`
- Create: `Tests/ECSSceneSerializationSmoke.cpp`
- Modify: `Tests/SceneServiceSmoke.cpp`
- Modify: `Tests/ScriptServiceSmoke.cpp`
- Modify: `Tests/RenderingOperationsSmoke.cpp`
- Modify: `Tests/EditorInteractionSmoke.cpp`
- Modify: `CMakeLists.txt`

---

### Task 1: ECS 身份类型与组件类型 ID

**Files:**
- Create: `HuaEngine/src/HuaEngine/ECS/EntityId.h`
- Create: `HuaEngine/src/HuaEngine/ECS/ComponentType.h`
- Create: `Tests/ECSCoreSmoke.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

Create `Tests/ECSCoreSmoke.cpp`:

```cpp
#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/EntityId.h"

#include <cassert>
#include <cstdint>
#include <string>

struct SmokePosition {
    float X = 0.0f;
};

struct SmokeVelocity {
    float X = 0.0f;
};

int main() {
    const HE::EntityId invalid{};
    assert(!invalid);

    const HE::EntityId a{ 7, 2 };
    const HE::EntityId b{ 7, 2 };
    const HE::EntityId stale{ 7, 1 };
    assert(a);
    assert(a == b);
    assert(a != stale);

    const HE::EntityUuid uuid{ 0x1111222233334444ull, 0x5555666677778888ull };
    const std::string text = HE::ToString(uuid);
    assert(text == "11112222333344445555666677778888");
    const auto parsed = HE::EntityUuid::FromString(text);
    assert(parsed.has_value());
    assert(*parsed == uuid);

    const auto positionType = HE::ComponentTypeIdOf<SmokePosition>();
    const auto velocityType = HE::ComponentTypeIdOf<SmokeVelocity>();
    assert(positionType != HE::InvalidComponentTypeId);
    assert(velocityType != HE::InvalidComponentTypeId);
    assert(positionType != velocityType);

    const bool readTerm = HE::IsReadTerm<HE::Read<SmokePosition>>::Value;
    const bool writeTerm = HE::IsReadTerm<SmokePosition>::Value;
    assert(readTerm);
    assert(!writeTerm);

    return 0;
}
```

Modify root `CMakeLists.txt` after `EditorInteractionSmoke` target block:

```cmake
add_executable(ECSCoreSmoke Tests/ECSCoreSmoke.cpp)
target_include_directories(ECSCoreSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)
target_link_libraries(ECSCoreSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ECSCoreSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ECSCoreSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ECSCoreSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ECSCoreSmoke)
set_property(TARGET ECSCoreSmoke PROPERTY FOLDER "Tests")
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target ECSCoreSmoke
```

Expected: build fails because `HuaEngine/ECS/EntityId.h` and `HuaEngine/ECS/ComponentType.h` do not exist.

- [ ] **Step 3: 实现身份类型**

Create `HuaEngine/src/HuaEngine/ECS/EntityId.h`:

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace HE {

	struct EntityId {
		uint32_t Index = 0;
		uint32_t Generation = 0;

		[[nodiscard]] explicit operator bool() const {
			return Generation != 0;
		}
	};

	[[nodiscard]] inline bool operator==(EntityId lhs, EntityId rhs) {
		return lhs.Index == rhs.Index && lhs.Generation == rhs.Generation;
	}

	[[nodiscard]] inline bool operator!=(EntityId lhs, EntityId rhs) {
		return !(lhs == rhs);
	}

	struct EntityUuid {
		uint64_t High = 0;
		uint64_t Low = 0;

		[[nodiscard]] explicit operator bool() const {
			return High != 0 || Low != 0;
		}

		[[nodiscard]] static std::optional<EntityUuid> FromString(std::string_view value) {
			if (value.size() != 32) {
				return std::nullopt;
			}

			auto parseHalf = [](std::string_view text) -> std::optional<uint64_t> {
				uint64_t parsed = 0;
				for (const char ch : text) {
					parsed <<= 4;
					if (ch >= '0' && ch <= '9') {
						parsed |= static_cast<uint64_t>(ch - '0');
					}
					else if (ch >= 'a' && ch <= 'f') {
						parsed |= static_cast<uint64_t>(ch - 'a' + 10);
					}
					else if (ch >= 'A' && ch <= 'F') {
						parsed |= static_cast<uint64_t>(ch - 'A' + 10);
					}
					else {
						return std::nullopt;
					}
				}
				return parsed;
			};

			auto high = parseHalf(value.substr(0, 16));
			auto low = parseHalf(value.substr(16, 16));
			if (!high || !low) {
				return std::nullopt;
			}

			return EntityUuid{ *high, *low };
		}
	};

	[[nodiscard]] inline bool operator==(EntityUuid lhs, EntityUuid rhs) {
		return lhs.High == rhs.High && lhs.Low == rhs.Low;
	}

	[[nodiscard]] inline bool operator!=(EntityUuid lhs, EntityUuid rhs) {
		return !(lhs == rhs);
	}

	[[nodiscard]] inline std::string ToString(EntityUuid uuid) {
		std::ostringstream stream;
		stream << std::hex << std::setfill('0') << std::nouppercase
			<< std::setw(16) << uuid.High
			<< std::setw(16) << uuid.Low;
		return stream.str();
	}

}

namespace std {
	template<>
	struct hash<HE::EntityId> {
		size_t operator()(HE::EntityId id) const noexcept {
			return (static_cast<size_t>(id.Generation) << 32u) ^ id.Index;
		}
	};

	template<>
	struct hash<HE::EntityUuid> {
		size_t operator()(HE::EntityUuid uuid) const noexcept {
			return static_cast<size_t>(uuid.High ^ (uuid.Low + 0x9e3779b97f4a7c15ull + (uuid.High << 6u) + (uuid.High >> 2u)));
		}
	};
}
```

- [ ] **Step 4: 实现组件类型 ID 与读写 term**

Create `HuaEngine/src/HuaEngine/ECS/ComponentType.h`:

```cpp
#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace HE {

	using ComponentTypeId = uint32_t;
	inline constexpr ComponentTypeId InvalidComponentTypeId = 0;

	namespace Detail {
		inline ComponentTypeId NextComponentTypeId() {
			static std::atomic<ComponentTypeId> next{ 1 };
			return next.fetch_add(1, std::memory_order_relaxed);
		}
	}

	template<typename T>
	[[nodiscard]] ComponentTypeId ComponentTypeIdOf() {
		static const ComponentTypeId id = Detail::NextComponentTypeId();
		return id;
	}

	template<typename T>
	struct Read {
		using Type = T;
	};

	template<typename T>
	struct QueryTermTraits {
		using Type = T;
		static constexpr bool ReadOnly = false;
	};

	template<typename T>
	struct QueryTermTraits<Read<T>> {
		using Type = T;
		static constexpr bool ReadOnly = true;
	};

	template<typename T>
	struct IsReadTerm {
		static constexpr bool Value = QueryTermTraits<T>::ReadOnly;
	};

}
```

- [ ] **Step 5: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target ECSCoreSmoke
.\build\bin\Debug-Windows-x64\smoke\ECSCoreSmoke.exe
```

Expected: build exit code 0 and executable exit code 0.

- [ ] **Step 6: 提交**

```powershell
git add HuaEngine/src/HuaEngine/ECS/EntityId.h HuaEngine/src/HuaEngine/ECS/ComponentType.h Tests/ECSCoreSmoke.cpp CMakeLists.txt
git commit -m "feat(ecs): add entity identity and component type ids"
```

---

### Task 2: ComponentRegistry

**Files:**
- Create: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`
- Create: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`
- Modify: `Tests/ECSCoreSmoke.cpp`

- [ ] **Step 1: 扩展失败测试**

Append to `Tests/ECSCoreSmoke.cpp` before `return 0;`:

```cpp
    HE::ComponentRegistry registry;
    bool duplicateRejected = false;

    const auto registerResult = registry.Register<SmokePosition>({
        .TypeName = "Tests.SmokePosition",
        .DisplayName = "Smoke Position",
        .Category = "Tests"
    });
    assert(registerResult);
    assert(registry.FindByType<SmokePosition>() != nullptr);
    assert(registry.FindByName("Tests.SmokePosition") != nullptr);

    const auto duplicateResult = registry.Register<SmokePosition>({
        .TypeName = "Tests.SmokePosition",
        .DisplayName = "Duplicate",
        .Category = "Tests"
    });
    duplicateRejected = !duplicateResult;
    assert(duplicateRejected);
```

Add include:

```cpp
#include "HuaEngine/ECS/ComponentRegistry.h"
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target ECSCoreSmoke
```

Expected: build fails because `ComponentRegistry.h` does not exist.

- [ ] **Step 3: 实现 registry 头文件**

Create `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`:

```cpp
#pragma once

#include "HuaEngine/ECS/ComponentType.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace HE {

	class World;
	struct EntityId;

	struct ComponentRegistration {
		std::string TypeName;
		std::string DisplayName;
		std::string Category;
		bool AllowMultiple = false;
	};

	struct ComponentMetadata {
		ComponentTypeId TypeId = InvalidComponentTypeId;
		std::string TypeName;
		std::string DisplayName;
		std::string Category;
		size_t Size = 0;
		bool AllowMultiple = false;
		std::function<void*(void)> ConstructDefault;
		std::function<void(void*)> Destroy;
		std::function<void*(const void*)> Copy;
	};

	class ComponentRegistry {
	public:
		template<typename T>
		bool Register(ComponentRegistration registration) {
			const ComponentTypeId typeId = ComponentTypeIdOf<T>();
			if (m_ByTypeId.find(typeId) != m_ByTypeId.end() || m_ByName.find(registration.TypeName) != m_ByName.end()) {
				return false;
			}

			ComponentMetadata metadata;
			metadata.TypeId = typeId;
			metadata.TypeName = std::move(registration.TypeName);
			metadata.DisplayName = std::move(registration.DisplayName);
			metadata.Category = std::move(registration.Category);
			metadata.Size = sizeof(T);
			metadata.AllowMultiple = registration.AllowMultiple;
			metadata.ConstructDefault = []() -> void* { return new T{}; };
			metadata.Destroy = [](void* value) { delete static_cast<T*>(value); };
			metadata.Copy = [](const void* value) -> void* { return new T(*static_cast<const T*>(value)); };

			const auto index = m_Metadata.size();
			m_Metadata.emplace_back(std::move(metadata));
			m_ByTypeId[typeId] = index;
			m_ByName[m_Metadata[index].TypeName] = index;
			return true;
		}

		template<typename T>
		[[nodiscard]] const ComponentMetadata* FindByType() const {
			return FindByTypeId(ComponentTypeIdOf<T>());
		}

		[[nodiscard]] const ComponentMetadata* FindByTypeId(ComponentTypeId typeId) const;
		[[nodiscard]] const ComponentMetadata* FindByName(std::string_view typeName) const;
		[[nodiscard]] const std::vector<ComponentMetadata>& GetAll() const { return m_Metadata; }

	private:
		std::vector<ComponentMetadata> m_Metadata;
		std::unordered_map<ComponentTypeId, size_t> m_ByTypeId;
		std::unordered_map<std::string, size_t> m_ByName;
	};

}
```

- [ ] **Step 4: 实现 registry cpp**

Create `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp`:

```cpp
#include "enginepch.h"
#include "HuaEngine/ECS/ComponentRegistry.h"

namespace HE {

	const ComponentMetadata* ComponentRegistry::FindByTypeId(ComponentTypeId typeId) const {
		const auto it = m_ByTypeId.find(typeId);
		if (it == m_ByTypeId.end()) {
			return nullptr;
		}
		return &m_Metadata[it->second];
	}

	const ComponentMetadata* ComponentRegistry::FindByName(std::string_view typeName) const {
		const auto it = m_ByName.find(std::string(typeName));
		if (it == m_ByName.end()) {
			return nullptr;
		}
		return &m_Metadata[it->second];
	}

}
```

- [ ] **Step 5: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target ECSCoreSmoke
.\build\bin\Debug-Windows-x64\smoke\ECSCoreSmoke.exe
```

Expected: exit code 0.

- [ ] **Step 6: 提交**

```powershell
git add HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp Tests/ECSCoreSmoke.cpp
git commit -m "feat(ecs): add component registry metadata"
```

---

### Task 3: World 存储、Entity facade 与 Query

**Files:**
- Create: `HuaEngine/src/HuaEngine/ECS/World.h`
- Create: `HuaEngine/src/HuaEngine/ECS/World.cpp`
- Create: `HuaEngine/src/HuaEngine/ECS/Query.h`
- Modify: `HuaEngine/src/HuaEngine/ECS/Entity.h`
- Delete after migration in this task: `HuaEngine/src/HuaEngine/ECS/Entity.cpp`
- Create: `Tests/ECSQuerySchedulerSmoke.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

Create `Tests/ECSQuerySchedulerSmoke.cpp`:

```cpp
#include "HuaEngine/ECS/World.h"

#include <cassert>
#include <vector>

struct QueryPosition {
    float X = 0.0f;
};

struct QueryVelocity {
    float X = 0.0f;
};

int main() {
    HE::World world;
    assert(world.GetEntityCount() == 0);

    auto entity = world.CreateEntity("Mover");
    assert(entity.IsValid());
    assert(world.GetEntityCount() == 1);

    const auto id = entity.GetId();
    const auto uuid = entity.GetUuid();
    assert(world.FindEntity(uuid) == id);

    world.AddComponent<QueryPosition>(id, QueryPosition{ 1.0f });
    world.AddComponent<QueryVelocity>(id, QueryVelocity{ 2.0f });
    assert(world.HasComponent<QueryPosition>(id));

    auto* position = world.TryGetComponent<QueryPosition>(id);
    assert(position != nullptr);
    assert(position->X == 1.0f);

    int visited = 0;
    world.Query<QueryPosition, HE::Read<QueryVelocity>>().ForEach(
        [&](HE::Entity found, QueryPosition& p, const QueryVelocity& v) {
            assert(found.GetId() == id);
            p.X += v.X;
            ++visited;
        });
    assert(visited == 1);
    assert(world.TryGetComponent<QueryPosition>(id)->X == 3.0f);

    world.DestroyEntity(id);
    assert(!world.IsAlive(id));
    assert(world.TryGetComponent<QueryPosition>(id) == nullptr);
    assert(world.GetEntityCount() == 0);

    return 0;
}
```

Add target to root `CMakeLists.txt` after `ECSCoreSmoke`:

```cmake
add_executable(ECSQuerySchedulerSmoke Tests/ECSQuerySchedulerSmoke.cpp)
target_include_directories(ECSQuerySchedulerSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)
target_link_libraries(ECSQuerySchedulerSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ECSQuerySchedulerSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ECSQuerySchedulerSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ECSQuerySchedulerSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ECSQuerySchedulerSmoke)
set_property(TARGET ECSQuerySchedulerSmoke PROPERTY FOLDER "Tests")
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target ECSQuerySchedulerSmoke
```

Expected: build fails because `World.h` does not exist.

- [ ] **Step 3: 实现 Entity facade**

Replace `HuaEngine/src/HuaEngine/ECS/Entity.h`:

```cpp
#pragma once

#include "HuaEngine/ECS/EntityId.h"

#include <string>

namespace HE {

	class World;

	class Entity {
	public:
		Entity() = default;
		Entity(EntityId id, World* world)
			: m_Id(id), m_World(world) {}

		[[nodiscard]] EntityId GetId() const { return m_Id; }
		[[nodiscard]] EntityUuid GetUuid() const;
		[[nodiscard]] bool IsValid() const;
		[[nodiscard]] std::string GetName() const;
		void SetName(const std::string& name);

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args);

		template<typename T>
		T& GetComponent();

		template<typename T>
		const T& GetComponent() const;

		template<typename T>
		T* TryGetComponent();

		template<typename T>
		bool HasComponent() const;

		template<typename T>
		void RemoveComponent();

		[[nodiscard]] explicit operator bool() const { return IsValid(); }

	private:
		EntityId m_Id{};
		World* m_World = nullptr;
	};

}
```

- [ ] **Step 4: 实现 World 和 Query**

Create `HuaEngine/src/HuaEngine/ECS/Query.h` with a template query that iterates World entities and resolves component pointers:

```cpp
#pragma once

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/Entity.h"

#include <tuple>
#include <type_traits>

namespace HE {

	class World;

	template<typename... Terms>
	class Query {
	public:
		explicit Query(World& world)
			: m_World(world) {}

		template<typename Callback>
		void ForEach(Callback&& callback);

	private:
		template<typename Term>
		using Component = typename QueryTermTraits<Term>::Type;

		template<typename Term>
		using CallbackArg = std::conditional_t<QueryTermTraits<Term>::ReadOnly, const Component<Term>&, Component<Term>&>;

		World& m_World;
	};

}
```

Create `HuaEngine/src/HuaEngine/ECS/World.h`:

```cpp
#pragma once

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/Query.h"

#include <any>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace HE {

	class World {
	public:
		Entity CreateEntity(std::string_view name = "Entity");
		Entity CreateEntityWithUuid(EntityUuid uuid, std::string_view name);
		void DestroyEntity(EntityId id);

		[[nodiscard]] bool IsAlive(EntityId id) const;
		[[nodiscard]] EntityUuid GetUuid(EntityId id) const;
		[[nodiscard]] EntityId FindEntity(EntityUuid uuid) const;
		[[nodiscard]] Entity GetEntity(EntityId id);
		[[nodiscard]] Entity GetEntity(EntityUuid uuid);
		[[nodiscard]] size_t GetEntityCount() const;

		[[nodiscard]] const std::string& GetName(EntityId id) const;
		void SetName(EntityId id, const std::string& name);

		template<typename T, typename... Args>
		T& AddComponent(EntityId id, Args&&... args) {
			auto& storage = Storage<T>();
			auto [it, inserted] = storage.emplace(id, T(std::forward<Args>(args)...));
			if (!inserted) {
				it->second = T(std::forward<Args>(args)...);
			}
			return it->second;
		}

		template<typename T>
		T* TryGetComponent(EntityId id) {
			if (!IsAlive(id)) {
				return nullptr;
			}
			auto& storage = Storage<T>();
			auto it = storage.find(id);
			return it == storage.end() ? nullptr : &it->second;
		}

		template<typename T>
		const T* TryGetComponent(EntityId id) const {
			if (!IsAlive(id)) {
				return nullptr;
			}
			const auto type = std::type_index(typeid(T));
			auto it = m_ComponentStores.find(type);
			if (it == m_ComponentStores.end()) {
				return nullptr;
			}
			const auto& storage = std::any_cast<const std::unordered_map<EntityId, T>&>(*it->second);
			auto componentIt = storage.find(id);
			return componentIt == storage.end() ? nullptr : &componentIt->second;
		}

		template<typename T>
		bool HasComponent(EntityId id) const {
			return TryGetComponent<T>(id) != nullptr;
		}

		template<typename T>
		void RemoveComponent(EntityId id) {
			auto& storage = Storage<T>();
			storage.erase(id);
		}

		template<typename... Terms>
		HE::Query<Terms...> Query() {
			return HE::Query<Terms...>(*this);
		}

		template<typename Callback>
		void ForEachEntity(Callback&& callback) {
			for (const auto& record : m_Records) {
				if (record.Alive) {
					callback(Entity{ EntityId{ record.Index, record.Generation }, this });
				}
			}
		}

	private:
		struct EntityRecord {
			uint32_t Index = 0;
			uint32_t Generation = 1;
			bool Alive = false;
			EntityUuid Uuid{};
			std::string Name = "Entity";
		};

		template<typename T>
		std::unordered_map<EntityId, T>& Storage() {
			const auto type = std::type_index(typeid(T));
			auto it = m_ComponentStores.find(type);
			if (it == m_ComponentStores.end()) {
				auto storage = std::make_unique<std::any>(std::unordered_map<EntityId, T>{});
				it = m_ComponentStores.emplace(type, std::move(storage)).first;
			}
			return std::any_cast<std::unordered_map<EntityId, T>&>(*it->second);
		}

		EntityUuid MakeUuid();
		void DestroyComponents(EntityId id);

		std::vector<EntityRecord> m_Records;
		std::vector<uint32_t> m_FreeIndices;
		std::unordered_map<EntityUuid, EntityId> m_UuidToEntity;
		std::unordered_map<std::type_index, std::unique_ptr<std::any>> m_ComponentStores;
		uint64_t m_NextUuidLow = 1;
	};

	template<typename T, typename... Args>
	T& Entity::AddComponent(Args&&... args) {
		return m_World->AddComponent<T>(m_Id, std::forward<Args>(args)...);
	}

	template<typename T>
	T& Entity::GetComponent() {
		return *m_World->TryGetComponent<T>(m_Id);
	}

	template<typename T>
	const T& Entity::GetComponent() const {
		return *m_World->TryGetComponent<T>(m_Id);
	}

	template<typename T>
	T* Entity::TryGetComponent() {
		return m_World->TryGetComponent<T>(m_Id);
	}

	template<typename T>
	bool Entity::HasComponent() const {
		return m_World->HasComponent<T>(m_Id);
	}

	template<typename T>
	void Entity::RemoveComponent() {
		m_World->RemoveComponent<T>(m_Id);
	}

	template<typename... Terms>
	template<typename Callback>
	void Query<Terms...>::ForEach(Callback&& callback) {
		m_World.ForEachEntity([&](Entity entity) {
			const EntityId id = entity.GetId();
			if (((m_World.template TryGetComponent<typename QueryTermTraits<Terms>::Type>(id) != nullptr) && ...)) {
				callback(entity, (*m_World.template TryGetComponent<typename QueryTermTraits<Terms>::Type>(id))...);
			}
		});
	}

}
```

Create `HuaEngine/src/HuaEngine/ECS/World.cpp`:

```cpp
#include "enginepch.h"
#include "HuaEngine/ECS/World.h"

namespace HE {

	Entity World::CreateEntity(std::string_view name) {
		return CreateEntityWithUuid(MakeUuid(), name);
	}

	Entity World::CreateEntityWithUuid(EntityUuid uuid, std::string_view name) {
		uint32_t index = 0;
		uint32_t generation = 1;
		if (!m_FreeIndices.empty()) {
			index = m_FreeIndices.back();
			m_FreeIndices.pop_back();
			auto& record = m_Records[index];
			generation = record.Generation + 1;
			record = EntityRecord{ index, generation, true, uuid, name.empty() ? "Entity" : std::string(name) };
		}
		else {
			index = static_cast<uint32_t>(m_Records.size());
			m_Records.push_back(EntityRecord{ index, generation, true, uuid, name.empty() ? "Entity" : std::string(name) });
		}

		const EntityId id{ index, generation };
		m_UuidToEntity[uuid] = id;
		return Entity{ id, this };
	}

	void World::DestroyEntity(EntityId id) {
		if (!IsAlive(id)) {
			return;
		}

		auto& record = m_Records[id.Index];
		DestroyComponents(id);
		m_UuidToEntity.erase(record.Uuid);
		record.Alive = false;
		m_FreeIndices.push_back(id.Index);
	}

	bool World::IsAlive(EntityId id) const {
		return id.Index < m_Records.size()
			&& m_Records[id.Index].Alive
			&& m_Records[id.Index].Generation == id.Generation;
	}

	EntityUuid World::GetUuid(EntityId id) const {
		return IsAlive(id) ? m_Records[id.Index].Uuid : EntityUuid{};
	}

	EntityId World::FindEntity(EntityUuid uuid) const {
		const auto it = m_UuidToEntity.find(uuid);
		return it == m_UuidToEntity.end() ? EntityId{} : it->second;
	}

	Entity World::GetEntity(EntityId id) {
		return IsAlive(id) ? Entity{ id, this } : Entity{};
	}

	Entity World::GetEntity(EntityUuid uuid) {
		return GetEntity(FindEntity(uuid));
	}

	size_t World::GetEntityCount() const {
		size_t count = 0;
		for (const auto& record : m_Records) {
			if (record.Alive) {
				++count;
			}
		}
		return count;
	}

	const std::string& World::GetName(EntityId id) const {
		static const std::string emptyName = "Entity";
		return IsAlive(id) ? m_Records[id.Index].Name : emptyName;
	}

	void World::SetName(EntityId id, const std::string& name) {
		if (IsAlive(id)) {
			m_Records[id.Index].Name = name.empty() ? "Entity" : name;
		}
	}

	EntityUuid World::MakeUuid() {
		return EntityUuid{ 0x4855454e47494e45ull, m_NextUuidLow++ };
	}

	void World::DestroyComponents(EntityId) {
		// Type-erased component erasure is completed in Task 7 when dynamic component metadata is added.
	}

	EntityUuid Entity::GetUuid() const {
		return m_World ? m_World->GetUuid(m_Id) : EntityUuid{};
	}

	bool Entity::IsValid() const {
		return m_World != nullptr && m_World->IsAlive(m_Id);
	}

	std::string Entity::GetName() const {
		return m_World ? m_World->GetName(m_Id) : "Entity";
	}

	void Entity::SetName(const std::string& name) {
		if (m_World) {
			m_World->SetName(m_Id, name);
		}
	}

}
```

- [ ] **Step 5: 删除旧 Entity.cpp**

Delete `HuaEngine/src/HuaEngine/ECS/Entity.cpp`. The old file has no remaining implementation after the facade moves to `World.cpp`.

- [ ] **Step 6: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target ECSQuerySchedulerSmoke
.\build\bin\Debug-Windows-x64\smoke\ECSQuerySchedulerSmoke.exe
```

Expected: exit code 0.

- [ ] **Step 7: 提交**

```powershell
git add HuaEngine/src/HuaEngine/ECS/World.h HuaEngine/src/HuaEngine/ECS/World.cpp HuaEngine/src/HuaEngine/ECS/Query.h HuaEngine/src/HuaEngine/ECS/Entity.h Tests/ECSQuerySchedulerSmoke.cpp CMakeLists.txt
git rm HuaEngine/src/HuaEngine/ECS/Entity.cpp
git commit -m "feat(ecs): add world storage and query facade"
```

---

### Task 4: SystemContext 与 Scheduler

**Files:**
- Create: `HuaEngine/src/HuaEngine/ECS/System.h`
- Create: `HuaEngine/src/HuaEngine/ECS/Scheduler.h`
- Create: `HuaEngine/src/HuaEngine/ECS/Scheduler.cpp`
- Delete: `HuaEngine/src/HuaEngine/ECS/Syetem.h`
- Modify: `Tests/ECSQuerySchedulerSmoke.cpp`

- [ ] **Step 1: 扩展失败测试**

Append to `Tests/ECSQuerySchedulerSmoke.cpp` before `return 0;`:

```cpp
    class FirstSystem final : public HE::System {
    public:
        explicit FirstSystem(std::vector<int>& order) : m_Order(order) {}
        HE::SystemDescriptor Describe() const override {
            return { .Name = "FirstSystem", .Stage = HE::SystemStage::Update };
        }
        void Update(HE::SystemContext&) override {
            m_Order.push_back(1);
        }
    private:
        std::vector<int>& m_Order;
    };

    class SecondSystem final : public HE::System {
    public:
        explicit SecondSystem(std::vector<int>& order) : m_Order(order) {}
        HE::SystemDescriptor Describe() const override {
            HE::SystemDescriptor descriptor;
            descriptor.Name = "SecondSystem";
            descriptor.Stage = HE::SystemStage::Update;
            descriptor.After = { "FirstSystem" };
            return descriptor;
        }
        void Update(HE::SystemContext&) override {
            m_Order.push_back(2);
        }
    private:
        std::vector<int>& m_Order;
    };

    std::vector<int> order;
    HE::Scheduler scheduler;
    scheduler.AddSystem(std::make_shared<SecondSystem>(order));
    scheduler.AddSystem(std::make_shared<FirstSystem>(order));
    HE::SystemContext context{ world, 1.0f / 60.0f };
    const bool sorted = scheduler.Build();
    assert(sorted);
    scheduler.Update(context);
    assert((order == std::vector<int>{ 1, 2 }));
```

Add include:

```cpp
#include "HuaEngine/ECS/Scheduler.h"
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target ECSQuerySchedulerSmoke
```

Expected: build fails because `Scheduler.h` does not exist.

- [ ] **Step 3: 实现 System.h**

Create `HuaEngine/src/HuaEngine/ECS/System.h`:

```cpp
#pragma once

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/World.h"

#include <string>
#include <vector>

namespace HE {

	enum class SystemStage {
		PreUpdate,
		Update,
		PostUpdate,
		Render
	};

	struct SystemDescriptor {
		std::string Name;
		SystemStage Stage = SystemStage::Update;
		std::vector<std::string> Before;
		std::vector<std::string> After;
		std::vector<ComponentTypeId> Reads;
		std::vector<ComponentTypeId> Writes;
		bool Enabled = true;
	};

	class SystemContext {
	public:
		SystemContext(World& world, float deltaTime)
			: m_World(world), m_DeltaTime(deltaTime) {}

		[[nodiscard]] World& WorldRef() { return m_World; }
		[[nodiscard]] const World& WorldRef() const { return m_World; }
		[[nodiscard]] float DeltaTime() const { return m_DeltaTime; }

	private:
		World& m_World;
		float m_DeltaTime = 0.0f;
	};

	class System {
	public:
		virtual ~System() = default;
		[[nodiscard]] virtual SystemDescriptor Describe() const = 0;
		virtual void Update(SystemContext& context) = 0;
	};

}
```

- [ ] **Step 4: 实现 Scheduler**

Create `HuaEngine/src/HuaEngine/ECS/Scheduler.h`:

```cpp
#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/System.h"

#include <memory>
#include <string>
#include <vector>

namespace HE {

	class Scheduler {
	public:
		void AddSystem(Ref<System> system);
		bool Build();
		void Update(SystemContext& context);

	private:
		struct Entry {
			Ref<System> Instance;
			SystemDescriptor Descriptor;
		};

		std::vector<Entry> m_Systems;
		std::vector<size_t> m_Order;
	};

}
```

Create `HuaEngine/src/HuaEngine/ECS/Scheduler.cpp`:

```cpp
#include "enginepch.h"
#include "HuaEngine/ECS/Scheduler.h"

#include <algorithm>
#include <unordered_map>

namespace HE {

	void Scheduler::AddSystem(Ref<System> system) {
		if (!system) {
			return;
		}
		m_Systems.push_back({ system, system->Describe() });
	}

	bool Scheduler::Build() {
		m_Order.clear();
		std::vector<size_t> indices(m_Systems.size());
		for (size_t i = 0; i < indices.size(); ++i) {
			indices[i] = i;
		}

		std::stable_sort(indices.begin(), indices.end(), [this](size_t lhs, size_t rhs) {
			const auto& a = m_Systems[lhs].Descriptor;
			const auto& b = m_Systems[rhs].Descriptor;
			if (a.Stage != b.Stage) {
				return static_cast<int>(a.Stage) < static_cast<int>(b.Stage);
			}
			if (std::find(a.After.begin(), a.After.end(), b.Name) != a.After.end()) {
				return false;
			}
			if (std::find(b.After.begin(), b.After.end(), a.Name) != b.After.end()) {
				return true;
			}
			if (std::find(a.Before.begin(), a.Before.end(), b.Name) != a.Before.end()) {
				return true;
			}
			if (std::find(b.Before.begin(), b.Before.end(), a.Name) != b.Before.end()) {
				return false;
			}
			return a.Name < b.Name;
		});

		m_Order = std::move(indices);
		return true;
	}

	void Scheduler::Update(SystemContext& context) {
		if (m_Order.empty() && !m_Systems.empty()) {
			(void)Build();
		}

		for (const size_t index : m_Order) {
			auto& entry = m_Systems[index];
			if (entry.Descriptor.Enabled) {
				entry.Instance->Update(context);
			}
		}
	}

}
```

- [ ] **Step 5: 删除旧拼写错误系统头文件**

Delete `HuaEngine/src/HuaEngine/ECS/Syetem.h`.

- [ ] **Step 6: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target ECSQuerySchedulerSmoke
.\build\bin\Debug-Windows-x64\smoke\ECSQuerySchedulerSmoke.exe
```

Expected: exit code 0.

- [ ] **Step 7: 提交**

```powershell
git add HuaEngine/src/HuaEngine/ECS/System.h HuaEngine/src/HuaEngine/ECS/Scheduler.h HuaEngine/src/HuaEngine/ECS/Scheduler.cpp Tests/ECSQuerySchedulerSmoke.cpp
git rm HuaEngine/src/HuaEngine/ECS/Syetem.h
git commit -m "feat(ecs): add scheduler and system context"
```

---

### Task 5: 迁移 Components、Scene 和公开 include

**Files:**
- Modify: `HuaEngine/src/HuaEngine/ECS/Components.h`
- Modify: `HuaEngine/src/HuaEngine/ECS/ScriptableEntity.h`
- Modify: `HuaEngine/src/HuaEngine/Scene/Scene.h`
- Modify: `HuaEngine/src/HuaEngine/Scene/Scene.cpp`
- Modify: `HuaEngine/src/HuaEngine.h`

- [ ] **Step 1: 写失败测试**

Modify `Tests/SceneServiceSmoke.cpp` near the beginning of `main()` to assert the new Scene API:

```cpp
    HE::Scene ecsScene("ECS Scene");
    auto createdEntity = ecsScene.GetWorld().CreateEntity("Scene Entity");
    assert(createdEntity.IsValid());
    assert(ecsScene.GetWorld().GetEntityCount() == 1);
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target SceneServiceSmoke
```

Expected: build fails because `Scene::GetWorld()` does not exist.

- [ ] **Step 3: 更新 Scene.h**

Replace `HuaEngine/src/HuaEngine/Scene/Scene.h`:

```cpp
#pragma once

#include <string>

#include "HuaEngine/ECS/Scheduler.h"
#include "HuaEngine/ECS/World.h"

namespace HE {

	class Scene {
	public:
		Scene() = default;
		explicit Scene(const std::string& name)
			: m_Name(name) {}

		void OnRuntimeStart();
		void OnUpdate(float deltaTime = 0.0f);
		void OnRuntimeStop();

		[[nodiscard]] const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		[[nodiscard]] World& GetWorld() { return m_World; }
		[[nodiscard]] const World& GetWorld() const { return m_World; }
		[[nodiscard]] Scheduler& GetScheduler() { return m_Scheduler; }

		void AddSystem(Ref<System> system);

		template<typename T>
		[[nodiscard]] Ref<T> FindSystem() const {
			for (const auto& system : m_Systems) {
				auto typedSystem = std::dynamic_pointer_cast<T>(system);
				if (typedSystem) {
					return typedSystem;
				}
			}
			return nullptr;
		}

	private:
		std::string m_Name;
		World m_World;
		Scheduler m_Scheduler;
		std::vector<Ref<System>> m_Systems;
	};

}
```

- [ ] **Step 4: 更新 Scene.cpp**

Replace `HuaEngine/src/HuaEngine/Scene/Scene.cpp`:

```cpp
#include "enginepch.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE {

	void Scene::OnRuntimeStart() {
		(void)m_Scheduler.Build();
	}

	void Scene::OnUpdate(float deltaTime) {
		SystemContext context{ m_World, deltaTime };
		m_Scheduler.Update(context);
	}

	void Scene::OnRuntimeStop() {
	}

	void Scene::AddSystem(Ref<System> system) {
		if (!system) {
			return;
		}
		m_Systems.emplace_back(system);
		m_Scheduler.AddSystem(system);
	}

}
```

- [ ] **Step 5: 更新脚本 facade**

Modify `HuaEngine/src/HuaEngine/ECS/ScriptableEntity.h` so methods still use `Entity`, but include no EnTT-dependent files:

```cpp
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/Core/Assert.h"
```

Keep the existing `AddComponent/GetComponent/HasComponent/RemoveComponent` template bodies. They compile against the new `Entity`.

- [ ] **Step 6: 更新公开 include**

Modify `HuaEngine/src/HuaEngine.h`: replace old ECS includes with:

```cpp
#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/Query.h"
#include "HuaEngine/ECS/Scheduler.h"
#include "HuaEngine/ECS/ScriptableEntity.h"
#include "HuaEngine/ECS/System.h"
#include "HuaEngine/ECS/World.h"
```

- [ ] **Step 7: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target SceneServiceSmoke
.\build\bin\Debug-Windows-x64\smoke\SceneServiceSmoke.exe
```

Expected: exit code 0.

- [ ] **Step 8: 提交**

```powershell
git add HuaEngine/src/HuaEngine/ECS/Components.h HuaEngine/src/HuaEngine/ECS/ScriptableEntity.h HuaEngine/src/HuaEngine/Scene/Scene.h HuaEngine/src/HuaEngine/Scene/Scene.cpp HuaEngine/src/HuaEngine.h Tests/SceneServiceSmoke.cpp
git commit -m "refactor(scene): move scene runtime to world and scheduler"
```

---

### Task 6: 迁移 ScriptService 与 ScriptRuntimeSystem

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptService.h`
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.h`
- Modify: `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.cpp`
- Modify: `Tests/ScriptServiceSmoke.cpp`

- [ ] **Step 1: 写失败测试**

Modify `Tests/ScriptServiceSmoke.cpp` to create script entities through `scene.GetWorld()`:

```cpp
    auto scriptEntity = scene.GetWorld().CreateEntity("Script Entity");
    auto bindResult = scriptService.BindNativeScript<TestScript>(scriptEntity, "TestScript");
    assert(bindResult.Succeeded());
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target ScriptServiceSmoke
```

Expected: build fails where old code still expects `Scene::View` or `GetEntityManager`.

- [ ] **Step 3: 更新 ScriptService 遍历**

In `HuaEngine/src/HuaEngine/Script/ScriptService.cpp`, replace the helper that uses `scene.View<NativeScriptComponent>()` with:

```cpp
template<typename Callback>
void ForEachScriptComponent(HE::Scene& scene, Callback&& callback) {
    auto query = scene.GetWorld().Query<HE::NativeScriptComponent>();
    query.ForEach([&](HE::Entity entity, HE::NativeScriptComponent& scriptComponent) {
        callback(entity, scriptComponent);
    });
}
```

Replace all `scene.View<HE::NativeScriptComponent>()` loops with `ForEachScriptComponent(scene, ...)`.

- [ ] **Step 4: 更新 ScriptRuntimeSystem**

Replace `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.h` include from old system header to:

```cpp
#include "HuaEngine/ECS/System.h"
```

Replace `ScriptRuntimeSystem::Update()` signature in header:

```cpp
SystemDescriptor Describe() const override;
void Update(SystemContext& context) override;
```

Replace `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.cpp`:

```cpp
#include "enginepch.h"
#include "ScriptRuntimeSystem.h"

namespace HE {

	SystemDescriptor ScriptRuntimeSystem::Describe() const {
		SystemDescriptor descriptor;
		descriptor.Name = "ScriptRuntimeSystem";
		descriptor.Stage = SystemStage::Update;
		descriptor.Writes = { ComponentTypeIdOf<NativeScriptComponent>() };
		return descriptor;
	}

	void ScriptRuntimeSystem::Update(SystemContext&) {
		HE_CORE_ASSERT(m_Scene, "ScriptRuntimeSystem requires a valid scene");
		HE_CORE_ASSERT(m_ScriptService, "ScriptRuntimeSystem requires a valid script service");
		(void)m_ScriptService->UpdateSceneScripts(*m_Scene);
	}

}
```

- [ ] **Step 5: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target ScriptServiceSmoke
.\build\bin\Debug-Windows-x64\smoke\ScriptServiceSmoke.exe
```

Expected: exit code 0.

- [ ] **Step 6: 提交**

```powershell
git add HuaEngine/src/HuaEngine/Script/ScriptService.h HuaEngine/src/HuaEngine/Script/ScriptService.cpp HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.h HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.cpp Tests/ScriptServiceSmoke.cpp
git commit -m "refactor(script): run native scripts through world query"
```

---

### Task 7: SceneSerializer 新 schema 与动态组件访问

**Files:**
- Modify: `HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h`
- Modify: `HuaEngine/src/HuaEngine/ECS/World.h`
- Modify: `HuaEngine/src/HuaEngine/ECS/World.cpp`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneSerializer.h`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp`
- Create: `Tests/ECSSceneSerializationSmoke.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

Create `Tests/ECSSceneSerializationSmoke.cpp`:

```cpp
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"

#include <cassert>
#include <filesystem>

int main() {
    HE::Scene scene("Serialized ECS Scene");
    auto entity = scene.GetWorld().CreateEntity("Camera");
    entity.AddComponent<HE::TransformComponent>();

    const std::filesystem::path path = "ecs_scene_serialization_smoke.scene";
    const bool saved = HE::Serialization::SaveScene(scene, path.string());
    assert(saved);

    HE::Scene loaded;
    const bool loadedOk = HE::Serialization::LoadScene(path.string(), loaded);
    assert(loadedOk);
    assert(loaded.GetName() == "Serialized ECS Scene");
    assert(loaded.GetWorld().GetEntityCount() == 1);

    std::filesystem::remove(path);
    return 0;
}
```

Add target to root `CMakeLists.txt`:

```cmake
add_executable(ECSSceneSerializationSmoke Tests/ECSSceneSerializationSmoke.cpp)
target_include_directories(ECSSceneSerializationSmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/HuaEngine/src
    ${SPDLOG_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${STB_IMAGE_INCLUDE_DIR}
)
target_link_libraries(ECSSceneSerializationSmoke PRIVATE HuaEngine)
if(WIN32)
    target_compile_definitions(ECSSceneSerializationSmoke PRIVATE GLFW_INCLUDE_NONE)
    if(MSVC)
        set_target_properties(ECSSceneSerializationSmoke PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(ECSSceneSerializationSmoke PRIVATE /utf-8)
    endif()
endif()
configure_smoke_target(ECSSceneSerializationSmoke)
set_property(TARGET ECSSceneSerializationSmoke PROPERTY FOLDER "Tests")
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target ECSSceneSerializationSmoke
```

Expected: build or runtime failure because serializer still uses registry storage.

- [ ] **Step 3: 扩展 ComponentRegistry 序列化 hook**

In `ComponentMetadata`, add:

```cpp
std::function<void(Serialization::SerializationBackend&, const std::string&, const void*)> Serialize;
std::function<bool(Serialization::SerializationBackend&, const std::string&, void*)> Deserialize;
```

Add include in `ComponentRegistry.h`:

```cpp
#include "HuaEngine/Serialization/SerializationCore.h"
```

In `ComponentRegistry::Register<T>()`, set:

```cpp
metadata.Serialize = [](Serialization::SerializationBackend& backend, const std::string& name, const void* component) {
    Serialization::Serializer<T>::Serialize(backend, name, *static_cast<const T*>(component));
};
metadata.Deserialize = [](Serialization::SerializationBackend& backend, const std::string& name, void* component) {
    return Serialization::Serializer<T>::Deserialize(backend, name, *static_cast<T*>(component));
};
```

- [ ] **Step 4: 增加 World 动态组件接口**

Add to `World.h`:

```cpp
void AddComponentByType(EntityId id, const ComponentMetadata& metadata, const void* component);
const void* TryGetComponentByType(EntityId id, ComponentTypeId typeId) const;
std::vector<ComponentTypeId> ListComponentTypes(EntityId id) const;
```

Implement in `World.cpp` with `m_DynamicComponents`:

```cpp
std::unordered_map<ComponentTypeId, std::unordered_map<EntityId, std::unique_ptr<void, void(*)(void*)>>> m_DynamicComponents;
```

For Task 7, store dynamic components created by serializer and registered components. Keep typed `Storage<T>()` for template Query; mirror typed additions into dynamic metadata when registry is available in Task 8.

- [ ] **Step 5: 重写 SceneSerializer schema**

Replace serializer loops with World API:

```cpp
backend.BeginObject("scene");
backend.SerializeValue("name", scene.GetName());
backend.EndObject();

backend.BeginArray("entities");
scene.GetWorld().ForEachEntity([&](Entity entity) {
    backend.BeginArrayElement();
    backend.BeginObject("");
    backend.SerializeValue("uuid", ToString(entity.GetUuid()));
    backend.SerializeValue("name", entity.GetName());
    backend.BeginObject("components");
    for (const auto typeId : scene.GetWorld().ListComponentTypes(entity.GetId())) {
        const auto* metadata = registry.FindByTypeId(typeId);
        const void* component = scene.GetWorld().TryGetComponentByType(entity.GetId(), typeId);
        if (metadata && component && metadata->Serialize) {
            metadata->Serialize(backend, metadata->TypeName, component);
        }
    }
    backend.EndObject();
    backend.EndObject();
    backend.EndArrayElement();
});
backend.EndArray();
```

Use `EntityUuid::FromString()` during load and call `CreateEntityWithUuid`.

- [ ] **Step 6: 注册核心组件**

Add a helper in `Components.h` or `ComponentRegistry.cpp`:

```cpp
void RegisterCoreComponents(ComponentRegistry& registry) {
    registry.Register<NameComponent>({ "HE.NameComponent", "Name", "Core" });
    registry.Register<TransformComponent>({ "HE.TransformComponent", "Transform", "Core" });
    registry.Register<NativeScriptComponent>({ "HE.NativeScriptComponent", "Native Script", "Script" });
}
```

Expose it in a header:

```cpp
void RegisterCoreComponents(ComponentRegistry& registry);
```

- [ ] **Step 7: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target ECSSceneSerializationSmoke
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
```

Expected: exit code 0 and no leftover `ecs_scene_serialization_smoke.scene`.

- [ ] **Step 8: 提交**

```powershell
git add HuaEngine/src/HuaEngine/ECS/ComponentRegistry.h HuaEngine/src/HuaEngine/ECS/ComponentRegistry.cpp HuaEngine/src/HuaEngine/ECS/World.h HuaEngine/src/HuaEngine/ECS/World.cpp HuaEngine/src/HuaEngine/ECS/Components.h HuaEngine/src/HuaEngine/Scene/SceneSerializer.h HuaEngine/src/HuaEngine/Scene/SceneSerializer.cpp Tests/ECSSceneSerializationSmoke.cpp CMakeLists.txt
git commit -m "feat(scene): serialize scenes through ecs registry"
```

---

### Task 8: 迁移 ApplicationOperations、SceneService、Validation、Rendering

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `HuaEngine/src/HuaEngine/Scene/SceneService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Validation/ValidationService.cpp`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.h`
- Modify: `HuaEngine/src/Module/Rendering/RenderSystem.cpp`
- Modify: `Tests/ApplicationOperationsSmoke.cpp`
- Modify: `Tests/RenderingOperationsSmoke.cpp`
- Modify: `Tests/ValidationServiceSmoke.cpp`

- [ ] **Step 1: 写失败测试**

In `Tests/ApplicationOperationsSmoke.cpp`, replace any `GetEntityManager()` setup with:

```cpp
auto entity = scene->GetWorld().CreateEntity("Operations Entity");
const auto entityId = entity.GetId();
```

Then call operations using `entityId` converted through the new operation overload:

```cpp
auto renameResult = operations.UpsertSceneEntityName(*scene, entityId, HE::NameComponent("Renamed"));
assert(renameResult.Succeeded());
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target ApplicationOperationsSmoke
```

Expected: build fails because operations still accept `uint32_t` entity handles.

- [ ] **Step 3: 更新 ApplicationOperations entity 参数**

Change operation signatures from `uint32_t entityId` to `EntityId entityId` for scene entity mutations:

```cpp
ResultEnvelope UpsertSceneEntityName(Scene& scene, EntityId entityId, const NameComponent& component) const;
ResultEnvelope UpsertSceneEntityTransform(Scene& scene, EntityId entityId, const TransformComponent& component) const;
ResultEnvelope AddSceneComponent(Scene& scene, EntityId entityId, SceneComponentKind componentKind) const;
ResultEnvelope RemoveSceneComponent(Scene& scene, EntityId entityId, SceneComponentKind componentKind) const;
```

Update implementation to use:

```cpp
auto& world = scene.GetWorld();
if (!world.IsAlive(entityId)) {
    return ResultEnvelope::Failure("scene.entity.resolve", scene.GetName(), "Scene entity does not exist");
}
world.AddComponent<NameComponent>(entityId, component);
```

- [ ] **Step 4: 更新 SceneService 验证**

Replace registry iteration:

```cpp
scene.GetWorld().ForEachEntity([&](Entity entity) {
    const bool hasTransform = entity.HasComponent<TransformComponent>();
    const bool hasMesh = entity.HasComponent<Rendering::MeshComponent>();
    const bool hasMaterial = entity.HasComponent<Rendering::MaterialComponent>();
});
```

- [ ] **Step 5: 更新 RenderSystem**

Change header include to `HuaEngine/ECS/System.h`. Replace `Update()` with:

```cpp
SystemDescriptor Describe() const override;
void Update(SystemContext& context) override;
```

In `RenderSystem.cpp`:

```cpp
SystemDescriptor RenderSystem::Describe() const {
    SystemDescriptor descriptor;
    descriptor.Name = "RenderSystem";
    descriptor.Stage = SystemStage::Render;
    descriptor.Reads = {
        ComponentTypeIdOf<TransformComponent>(),
        ComponentTypeIdOf<Rendering::CameraComponent>(),
        ComponentTypeIdOf<Rendering::MeshComponent>(),
        ComponentTypeIdOf<Rendering::MaterialComponent>()
    };
    return descriptor;
}

void RenderSystem::Update(SystemContext& context) {
    auto cameraQuery = context.WorldRef().Query<Rendering::CameraComponent>();
    cameraQuery.ForEach([&](Entity, Rendering::CameraComponent& camera) {
        RenderSingleCamera(context.WorldRef(), *camera.Camera);
    });
}
```

Change `RenderSingleCamera` signature:

```cpp
void RenderSingleCamera(World& world, Rendering::Camera& camera);
```

Use:

```cpp
auto query = world.Query<TransformComponent, Rendering::MeshComponent, Rendering::MaterialComponent>();
```

- [ ] **Step 6: 运行测试确认通过**

Run:

```powershell
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target RenderingOperationsSmoke
cmake --build build --config Debug --target ValidationServiceSmoke
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ValidationServiceSmoke.exe
```

Expected: all exit code 0.

- [ ] **Step 7: 提交**

```powershell
git add HuaEngine/src/HuaEngine/Application/ApplicationOperations.h HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp HuaEngine/src/HuaEngine/Scene/SceneService.cpp HuaEngine/src/HuaEngine/Validation/ValidationService.cpp HuaEngine/src/Module/Rendering/RenderSystem.h HuaEngine/src/Module/Rendering/RenderSystem.cpp Tests/ApplicationOperationsSmoke.cpp Tests/RenderingOperationsSmoke.cpp Tests/ValidationServiceSmoke.cpp
git commit -m "refactor(runtime): route scene operations through world"
```

---

### Task 9: 迁移 Editor 选择、命令、Hierarchy、Inspector

**Files:**
- Modify: `Editor/src/Selection.h`
- Modify: `Editor/src/Selection.cpp`
- Modify: `Editor/src/ComponentEditorRegistry.h`
- Modify: `Editor/src/Panels/HierarchyPanel.h`
- Modify: `Editor/src/Panels/HierarchyPanel.cpp`
- Modify: `Editor/src/Panels/InspectorPanel.h`
- Modify: `Editor/src/Panels/InspectorPanel.cpp`
- Modify: `Editor/src/Interaction/EditorSceneCommands.h`
- Modify: `Editor/src/Interaction/EditorSceneCommands.cpp`
- Modify: `Editor/src/Interaction/EditorInteractionHost.cpp`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/EditorInteractionSmoke.cpp`

- [ ] **Step 1: 写失败测试**

Modify `Tests/EditorInteractionSmoke.cpp` to assert selection uses UUID:

```cpp
auto entity = scene.GetWorld().CreateEntity("Editor Entity");
selection.SetSelectedEntity(entity.GetUuid());
assert(selection.HasSelection());
assert(selection.GetSelectedEntityUuid() == entity.GetUuid());
```

- [ ] **Step 2: 运行测试确认失败**

Run:

```powershell
cmake --build build --config Debug --target EditorInteractionSmoke
```

Expected: build fails because `Selection` does not expose UUID selection.

- [ ] **Step 3: 更新 Selection**

Change `Selection` to store:

```cpp
std::optional<HE::EntityUuid> m_SelectedEntityUuid;
```

Expose:

```cpp
void SetSelectedEntity(HE::EntityUuid uuid);
void Clear();
bool HasSelection() const;
HE::EntityUuid GetSelectedEntityUuid() const;
HE::Entity ResolveSelectedEntity(HE::World& world) const;
```

- [ ] **Step 4: 更新 ComponentEditorRegistry**

Change draw function type from EnTT to ECS:

```cpp
using DrawFunction = std::function<bool(HE::World&, HE::EntityId, const DrawOptions&)>;
```

Template draw body:

```cpp
if (auto* component = world.TryGetComponent<T>(entityId)) {
    return ComponentEditor<T>::Draw(displayName.c_str(), *component, options);
}
return false;
```

- [ ] **Step 5: 更新 HierarchyPanel**

Replace registry view loop with:

```cpp
m_Context->GetWorld().ForEachEntity([&](HE::Entity entity) {
    const bool selected = m_Selection && m_Selection->GetSelectedEntityUuid() == entity.GetUuid();
    if (ImGui::Selectable(entity.GetName().c_str(), selected)) {
        m_Selection->SetSelectedEntity(entity.GetUuid());
    }
});
```

- [ ] **Step 6: 更新 InspectorPanel**

Resolve selected entity:

```cpp
auto entity = m_Selection->ResolveSelectedEntity(m_Context->GetWorld());
if (!entity.IsValid()) {
    ImGui::TextUnformatted("No entity selected.");
    return;
}
m_ComponentEditors.DrawComponents(m_Context->GetWorld(), entity.GetId(), options);
```

- [ ] **Step 7: 更新 EditorSceneCommands**

Store `EntityUuid` in commands. Resolve before applying:

```cpp
auto entity = context.SceneDocument->SceneRef->GetWorld().GetEntity(m_EntityUuid);
if (!entity.IsValid()) {
    return ResultEnvelope::Failure("editor.entity.resolve", ToString(m_EntityUuid), "Editor entity no longer exists");
}
```

For create command, set `m_EntityUuid = createdEntity.GetUuid();`.

- [ ] **Step 8: 更新 EditorLayer 直接 registry 使用点**

Replace each `GetEntityManager().GetRegistry()` block with `GetWorld()` API:

```cpp
auto& world = m_SceneDocument.SceneRef->GetWorld();
world.ForEachEntity([&](HE::Entity entity) {
    if (auto* transform = entity.TryGetComponent<TransformComponent>()) {
        // existing logic using transform
    }
});
```

For mesh/material loading:

```cpp
world.Query<Rendering::MeshComponent>().ForEach([&](HE::Entity, Rendering::MeshComponent& meshComponent) {
    // existing mesh setup
});
```

- [ ] **Step 9: 运行测试和 Editor 构建确认通过**

Run:

```powershell
cmake --build build --config Debug --target EditorInteractionSmoke
cmake --build build --config Debug --target Editor
.\build\bin\Debug-Windows-x64\smoke\EditorInteractionSmoke.exe
```

Expected: both builds exit code 0 and smoke executable exit code 0.

- [ ] **Step 10: 提交**

```powershell
git add Editor/src/Selection.h Editor/src/Selection.cpp Editor/src/ComponentEditorRegistry.h Editor/src/Panels/HierarchyPanel.h Editor/src/Panels/HierarchyPanel.cpp Editor/src/Panels/InspectorPanel.h Editor/src/Panels/InspectorPanel.cpp Editor/src/Interaction/EditorSceneCommands.h Editor/src/Interaction/EditorSceneCommands.cpp Editor/src/Interaction/EditorInteractionHost.cpp Editor/src/EditorLayer.cpp Tests/EditorInteractionSmoke.cpp
git commit -m "refactor(editor): use world api for entity editing"
```

---

### Task 10: 删除 EnTT 泄漏、更新 CMake 测试与完整验证

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `HuaEngine/CMakeLists.txt`
- Modify: any file returned by leakage searches below

- [ ] **Step 1: 搜索泄漏**

Run:

```powershell
rg -n "entt::|entt.hpp|GetRegistry\\(|Scene::View|\\.View<|\\.Get<" HuaEngine\src Editor\src Tests -g "*.h" -g "*.cpp"
```

Expected before cleanup: output may include old tests or comments.

- [ ] **Step 2: 清理业务代码 EnTT include**

For every result outside `Dependencies/entt` and outside an explicitly internal backend directory, replace direct EnTT usage with ECS API. Accepted replacements:

```cpp
scene.GetWorld()
world.Query<T...>()
world.ForEachEntity(...)
entity.GetId()
entity.GetUuid()
```

After cleanup, rerun:

```powershell
rg -n "entt::|entt.hpp|GetRegistry\\(|Scene::View|\\.View<|\\.Get<" HuaEngine\src Editor\src Tests -g "*.h" -g "*.cpp"
```

Expected: no output.

- [ ] **Step 3: 更新 CMake include 依赖**

Root `CMakeLists.txt` smoke targets can keep `${ENTT_INCLUDE_DIR}` only if third-party public headers still require it. If the leakage search is clean and no smoke target needs EnTT, remove `${ENTT_INCLUDE_DIR}` from smoke target include directories.

In `HuaEngine/CMakeLists.txt`, keep `${ENTT_INCLUDE_DIR}` only if internal backend files include EnTT. If no ECS backend uses EnTT, remove:

```cmake
file(GLOB_RECURSE ENTT_SOURCES
    "${ENTT_INCLUDE_DIR}/*.hpp"
)
```

and remove `${ENTT_SOURCES}` from `add_library(HuaEngine STATIC ...)`.

- [ ] **Step 4: 完整构建验证**

Run:

```powershell
cmake --build build --config Debug --target HuaEngine
cmake --build build --config Debug --target Editor
cmake --build build --config Debug --target ECSCoreSmoke
cmake --build build --config Debug --target ECSQuerySchedulerSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target SceneServiceSmoke
cmake --build build --config Debug --target ScriptServiceSmoke
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target RenderingOperationsSmoke
cmake --build build --config Debug --target ValidationServiceSmoke
cmake --build build --config Debug --target EditorInteractionSmoke
```

Expected: every command exits 0. Existing third-party encoding warnings are acceptable only if they come from `Dependencies/`.

- [ ] **Step 5: 运行 smoke executables**

Run:

```powershell
.\build\bin\Debug-Windows-x64\smoke\ECSCoreSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ECSQuerySchedulerSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ECSSceneSerializationSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\SceneServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ScriptServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\RenderingOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ValidationServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\EditorInteractionSmoke.exe
```

Expected: every executable exits 0.

- [ ] **Step 6: 最终状态检查**

Run:

```powershell
git status --short
rg -n "entt::|entt.hpp|GetRegistry\\(|Scene::View|\\.View<|\\.Get<" HuaEngine\src Editor\src Tests -g "*.h" -g "*.cpp"
```

Expected: `git status --short` only shows intended CMake/source/test changes, and leakage search has no output.

- [ ] **Step 7: 提交**

```powershell
git add CMakeLists.txt HuaEngine/CMakeLists.txt HuaEngine/src Editor/src Tests
git commit -m "chore(ecs): remove entt-facing legacy api"
```

---

## 自审清单

- 设计文档要求“公共 API 不暴露 EnTT”：Task 10 通过搜索验收。
- 设计文档要求 `EntityId + EntityUuid`：Task 1、Task 3 覆盖。
- 设计文档要求 `ComponentRegistry`：Task 2、Task 7 覆盖。
- 设计文档要求 `World/Query`：Task 3 覆盖。
- 设计文档要求 `Scheduler/SystemContext`：Task 4 覆盖。
- 设计文档要求 Scene 持有 World 和 Scheduler：Task 5 覆盖。
- 设计文档要求 Script/Rendering/Application 迁移：Task 6、Task 8 覆盖。
- 设计文档要求新 scene schema：Task 7 覆盖。
- 设计文档要求 Editor 纳入迁移：Task 9 覆盖。
- 设计文档要求构建与 smoke 验证：Task 10 覆盖。
