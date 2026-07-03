# HuaEngine Reflection Tool P1 设计文档

## 背景

P0 已完成 CLI 操作入口收口，当前 `HuaEngineCLI` 已具备稳定 command catalog、JSON 输出、exit code、contract smoke 和 workflow smoke。P1 按路线图推进 Reflection Tool，目标是把反射信息从手写宏和手写组件注册中逐步收口到工具生成链路，为后续序列化、Editor Inspector、脚本绑定、资产工具和 YAML 迁移提供统一类型信息基础。

现有代码已经有 C++ 宏反射系统：

- `srefl_class(...)`
- `fields(...)`
- `field(...)`

它已经被 `NameComponent`、`TransformComponent`、`CameraComponent`、`MeshComponent`、`MaterialComponent`、`ProjectDescriptor`、mesh/material 数据结构和序列化系统使用。当前不足是：

- 反射声明仍需要手写 `srefl_class`。
- `ComponentRegistry::RegisterCoreComponents` 仍手写注册核心组件。
- 没有工具扫描源码并输出稳定 manifest。
- 没有生成的 C++ reflection manifest。
- CLI 尚不能执行 reflection scan / generate / validate。

## 目标

P1 要完成一个完整但保守的工具闭环：

1. 使用空宏标记组件和字段。
2. Python 工具扫描源码，生成 JSON manifest。
3. Python 工具生成 C++ 反射代码、反射 manifest 和组件注册代码。
4. 迁移现有核心组件，移除对应手写 `srefl_class` 和手写 component registry 注册。
5. 新增正式 reflection operation，并接入 CLI。
6. 用 smoke 覆盖扫描、生成、编译、registry 注册和 CLI 输出契约。

P1 覆盖 Python MVP、C++ 代码生成、CLI 接入三部分。同一阶段全部完成。

## 源码标记

新增空宏头：

```cpp
// HuaEngine/src/HuaEngine/Reflection/ReflectionMarkers.h
#pragma once

#define HE_REFLECT_COMPONENT(...)
#define HE_REFLECT_FIELD(...)
```

组件源码使用这些宏作为工具扫描源：

```cpp
HE_REFLECT_COMPONENT(DisplayName="Transform", Category="Core")
struct TransformComponent : Component {
    HE_REFLECT_FIELD()
    glm::vec3 Position = {0.0f, 0.0f, 0.0f};

    HE_REFLECT_FIELD()
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};

    HE_REFLECT_FIELD()
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};
};
```

工具只扫描 `HE_REFLECT_COMPONENT` 和 `HE_REFLECT_FIELD`。未标记字段不进入生成反射，例如 `MeshComponent::m_CachedVertexArray`。

选择空宏而不是注释或 C++ attribute 的原因：

- 比注释更正式，formatter 和代码 review 更容易看见。
- 不改变编译后布局和运行时行为。
- 比 C++ 自定义 attribute 更少编译器 warning 和兼容性问题。
- 与现有 `srefl_class` 宏式风格一致。

## Python 工具

新增工具：

```text
Tools/Reflection/reflection_tool.py
```

支持命令：

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .
```

第一版扫描器只支持当前仓库代码风格，不做完整 C++ parser。

支持规则：

- `HE_REFLECT_COMPONENT(...)` 紧邻 `struct/class Type : Component`。
- `HE_REFLECT_FIELD()` 紧邻简单 data member 声明。
- 字段声明支持当前需要的简单形式，例如：
  - `std::string Name = "Entity";`
  - `glm::vec3 Position = {...};`
  - `bool Primary = true;`
  - `Ref<HE::Rendering::MaterialInstance> MaterialInstance;`
- 类型参数第一版支持：
  - `DisplayName="..."`
  - `Category="..."`
- `AllowMultiple` P1 默认 `false`，暂不开放。

不支持规则：

- 不解析函数。
- 不解析复杂宏展开后的字段。
- 不解析多变量同一行声明。
- 不解析 private/protected 字段作为反射字段。
- 不处理完整 C++ AST。

## Manifest

扫描输出 JSON manifest：

```json
{
  "schema_version": 1,
  "types": [
    {
      "name": "TransformComponent",
      "qualified_name": "HE::TransformComponent",
      "kind": "component",
      "display_name": "Transform",
      "category": "Core",
      "source": "HuaEngine/src/HuaEngine/ECS/Components.h",
      "fields": [
        { "name": "Position", "type": "glm::vec3" },
        { "name": "Rotation", "type": "glm::vec3" },
        { "name": "Scale", "type": "glm::vec3" }
      ]
    }
  ],
  "diagnostics": []
}
```

`validate` 规则：

- 被标记类型必须能解析到 type name。
- 被标记 component 必须至少有一个 reflected field。
- `HE_REFLECT_FIELD()` 后必须是可解析字段。
- `DisplayName` 和 `Category` 缺失时返回 error diagnostic。
- 同一个 qualified type 重复出现时返回 error diagnostic。
- generated files 与当前 scan 结果不一致时返回 error diagnostic。

## 生成物

生成目录：

```text
HuaEngine/src/HuaEngine/Generated/
  GeneratedReflection.h
  GeneratedReflection.cpp
```

生成内容包括：

1. `srefl_class(...)` specialization。
2. generated reflection manifest 查询接口。
3. `RegisterGeneratedComponents(ComponentRegistry&)`。

示例接口：

```cpp
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
```

生成 `srefl_class` 时保留字段名大小写，不做 snake_case 转换。字段 type 在 generated manifest 中只作为字符串 metadata，不参与 C++ 类型推导。

## ComponentRegistry 迁移

P1 迁移这些现有组件：

- `NameComponent`
- `TransformComponent`
- `CameraComponent`
- `MeshComponent`
- `MaterialComponent`

迁移后：

- 目标组件源码保留 `HE_REFLECT_COMPONENT` 和 `HE_REFLECT_FIELD` 标记。
- 目标组件不再手写 `srefl_class`。
- `ComponentRegistry::RegisterCoreComponents` 改为调用 `Generated::RegisterGeneratedComponents(registry)`，或者只保留非生成组件的手写注册。
- `NativeScriptComponent` 不迁移。
- deprecated `RendererComponent` 不迁移。

`RegisterGeneratedComponents` 注册内容：

- `TypeName` = C++ unqualified type name。
- `DisplayName` = `HE_REFLECT_COMPONENT` 参数。
- `Category` = `HE_REFLECT_COMPONENT` 参数。
- `AllowMultiple` = `false`。

## 正式 Operation 与 CLI

新增正式 operation：

- `reflection.scan`
- `reflection.generate`
- `reflection.validate`

CLI 新增命令：

```powershell
HuaEngineCLI.exe reflection scan --root <path> [--out <manifest>]
HuaEngineCLI.exe reflection generate --root <path> --out-dir <path>
HuaEngineCLI.exe reflection validate --root <path>
```

CLI 仍只调用 `ApplicationOperations`，不在 CLI handler 中直接写工具逻辑。工具执行放在 operation/service 层。第一版可以由 C++ reflection service 调用 Python 工具进程，后续再决定是否把扫描器迁移到 C++ 或 C#。

CLI 输出继续遵循 P0 契约：

- stdout 为 JSON。
- exit code 来自 `ResultEnvelope`。
- 成功 payload 至少包含 reflected type count。
- 失败 details 包含 tool diagnostics。

## 构建策略

P1 采用 checked-in generated files：

- 生成文件提交到 `HuaEngine/src/HuaEngine/Generated/`。
- CMake 正常编译 generated `.cpp`。
- 构建本身不强制每次运行 Python。
- smoke 验证 generated files 与当前 scan 结果一致，防止漂移。

这个策略比 build-time generation 稳定。等标记规则和生成规则稳定后，再考虑 CMake 自动生成。

## 测试设计

### `ReflectionToolSmoke`

外部或 C++ smoke 调用 Python 工具：

- `scan` 能识别 5 个核心 component。
- manifest 包含 display/category/fields。
- `MeshComponent` 只包含 `MeshAssetName`，不包含 `m_CachedVertexArray`。
- `validate` 成功。

### `ReflectionGeneratedSmoke`

C++ smoke 编译 generated manifest：

- `Generated::GetReflectedTypes()` 返回 5 个 component。
- `FindReflectedType("HE::TransformComponent")` 成功。
- `TransformComponent` 字段包含 `Position`、`Rotation`、`Scale`。
- `RegisterGeneratedComponents(ComponentRegistry&)` 后 registry 能按 name 找到 5 个 component metadata。

### `CLIReflectionSmoke`

外部进程运行：

- `reflection scan --root <repo>`
- `reflection validate --root <repo>`

断言：

- exit code 为 `0`。
- JSON status 为 `success`。
- payload 包含 reflected type count。
- details 可包含诊断摘要。

### 回归 smoke

迁移生成反射后必须继续通过：

- `ReflectionSmoke`
- `SerializationSmoke`
- `ECSSceneSerializationSmoke`
- `CLIContractSmoke`
- `CLIHostSmoke`

## 验收命令

```powershell
python Tools/Reflection/reflection_tool.py scan --root . --out .workspace/reflection/reflection_manifest.json
python Tools/Reflection/reflection_tool.py generate --manifest .workspace/reflection/reflection_manifest.json --out-dir HuaEngine/src/HuaEngine/Generated
python Tools/Reflection/reflection_tool.py validate --root .

cmake --build build --config Debug --target ReflectionToolSmoke
cmake --build build --config Debug --target ReflectionGeneratedSmoke
cmake --build build --config Debug --target CLIReflectionSmoke

& .\build\bin\Debug-Windows-x64\smoke\ReflectionToolSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ReflectionGeneratedSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\CLIReflectionSmoke.exe

cmake --build build --config Debug --target ReflectionSmoke
cmake --build build --config Debug --target SerializationSmoke
cmake --build build --config Debug --target ECSSceneSerializationSmoke
cmake --build build --config Debug --target CLIContractSmoke
cmake --build build --config Debug --target CLIHostSmoke
```

## 非目标

- 不做完整 C++ AST parser。
- 不用 clang tooling。
- 不引入 C# 正式版工具。
- 不迁移 `NativeScriptComponent`。
- 不迁移 deprecated `RendererComponent`。
- 不替换 JSON 序列化后端。
- 不改 YAML schema。
- 不接 Editor Inspector 自动面板生成。
- 不接 Lua binding。
- 不强制 build-time generation。

## 风险与缓解

Python 正则扫描可能误判复杂 C++。缓解方式是 P1 明确只支持当前标记组件的简单声明形式，遇到不支持语法直接 validate 失败。

生成 `srefl_class` 可能与旧手写 specialization 冲突。缓解方式是迁移目标组件时删除对应手写 `srefl_class`，并由 `ReflectionSmoke` 和编译器验证重复 specialization 不存在。

generated files 可能与源码标记漂移。缓解方式是 `reflection validate` 比较当前 scan 和 checked-in generated manifest。

ComponentRegistry 自动注册可能改变 type name、display name 或 category。缓解方式是 `ReflectionGeneratedSmoke` 和 `ECSCoreSmoke` 锁定 5 个核心 component metadata。

CLI 调 Python 进程可能受 Python 环境影响。缓解方式是 P1 smoke 明确运行 Python 工具；CLI failure 要返回 `manual_intervention_required` 或 `failure` 并带诊断，不能崩溃。

## 后续方向

P1 完成后可以继续推进：

- 用 generated manifest 驱动 Editor Inspector。
- 用 generated manifest 驱动 script binding。
- 生成更多 component 的 registry 注册。
- 将 Python MVP 迁移为 C# 或 clang tooling。
- 在 CMake 中加入可选自动生成目标。
