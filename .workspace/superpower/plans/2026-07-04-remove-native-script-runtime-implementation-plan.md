# Remove Native Script Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 删除 `NativeScriptComponent` 及其脚本 runtime 能力，让当前 ECS component 集中保持可反射、可序列化的数据职责。

**Architecture:** 直接移除原生脚本组件、脚本服务、脚本系统和公开 operation，不保留空壳 API。聚合 validation 只保留 project/scene/asset 三个域，CLI/agent 测试不再期待 `script.*` operation。

**Tech Stack:** C++17, CMake, HuaEngine ECS/Application/Validation/Smoke tests.

---

### Task 1: 删除脚本组件和脚本 runtime 类型

**Files:**
- Modify: `HuaEngine/src/HuaEngine/ECS/Components.h`
- Delete: `HuaEngine/src/HuaEngine/ECS/ScriptableEntity.h`
- Delete: `HuaEngine/src/HuaEngine/Script/ScriptService.h`
- Delete: `HuaEngine/src/HuaEngine/Script/ScriptService.cpp`
- Delete: `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.h`
- Delete: `HuaEngine/src/HuaEngine/Script/ScriptRuntimeSystem.cpp`
- Modify: `HuaEngine/src/HuaEngine.h`

- [ ] **Step 1: 删除 `NativeScriptComponent` 定义**

从 `Components.h` 删除 `class ScriptableEntity;` 和整个 `NativeScriptComponent` struct，仅保留 `NameComponent` 与 `TransformComponent`。

- [ ] **Step 2: 删除脚本 runtime 文件**

删除 `ScriptableEntity.h`、`ScriptService.*`、`ScriptRuntimeSystem.*`，并从 `HuaEngine.h` 删除 `#include "HuaEngine/ECS/ScriptableEntity.h"`。

- [ ] **Step 3: 搜索脚本类型残留**

Run: `rg -n "NativeScriptComponent|ScriptableEntity|ScriptService|ScriptRuntimeSystem" HuaEngine/src`

Expected: 只允许后续尚未处理的应用层/validation 引用出现；完成 Task 2 后应无结果。

### Task 2: 移除应用层 `script.*` operation 和 validation script domain

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationServices.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `HuaEngine/src/HuaEngine/Validation/ValidationService.h`
- Modify: `HuaEngine/src/HuaEngine/Validation/ValidationService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Automation/AgentHostAdapter.cpp`

- [ ] **Step 1: 从 `ApplicationServices` 删除脚本服务成员**

删除 `#include "HuaEngine/Script/ScriptService.h"`、`Scripts()` accessor 和 `ScriptService m_ScriptService`。

- [ ] **Step 2: 从 `ApplicationOperations` 删除脚本 API**

删除 `ScriptStatusReport` 前置声明、`ApplicationValidationRequest::ScriptScene`、`IncludeScripts`，删除 `UnbindNativeScript`、`AttachScriptRuntime`、`InitializeSceneScripts`、`UpdateSceneScripts`、`ShutdownSceneScripts`、`CheckSceneScripts` 声明和实现。

- [ ] **Step 3: 从 operation registry 删除 `script.*`**

从 `RegisterDefaultOperations()` 删除 `script.unbind`、`script.attach_runtime`、`script.initialize`、`script.update`、`script.shutdown`、`script.status` 注册。

- [ ] **Step 4: 从 validation 删除 script domain**

删除 `ValidationRequest::ScriptScene`、`Scripts`、`ValidationReport::IncludesScripts`、`ScriptStatus`、`ScriptResult`，并移除 `ValidationService::Validate()` 中的 script 分支和 `script_status` payload。

- [ ] **Step 5: 从 agent adapter 删除 `script.status` 特判**

删除 `AgentHostAdapter` 对 `script.status` 的分支；该 operation 不再发布。

### Task 3: 更新构建和 smoke tests

**Files:**
- Modify: `CMakeLists.txt`
- Delete: `Tests/ScriptServiceSmoke.cpp`
- Modify: `Tests/ValidationServiceSmoke.cpp`
- Modify: `Tests/ApplicationServicesSmoke.cpp`
- Modify: `Tests/ApplicationOperationsSmoke.cpp`
- Modify: `Tests/AgentHostAdapterSmoke.cpp`
- Modify: `Tests/CLIWorkflowSmoke.cpp`

- [ ] **Step 1: 从 CMake 删除 `ScriptServiceSmoke` target**

删除 `add_executable(ScriptServiceSmoke ...)`、include/link/compile 配置、`configure_smoke_target(ScriptServiceSmoke)` 和 folder property。

- [ ] **Step 2: 删除脚本 smoke 文件**

删除 `Tests/ScriptServiceSmoke.cpp`。

- [ ] **Step 3: 更新依赖脚本的 smoke**

移除 `ValidationServiceSmoke` 中脚本系统绑定和无效脚本组件场景；移除 `ApplicationServicesSmoke` 的 `ScriptRuntimeSystem` attach；移除 `ApplicationOperationsSmoke` 对 `script.attach_runtime` / `script.status` 的 Supports 和调用断言；移除 `AgentHostAdapterSmoke` 与 `CLIWorkflowSmoke` 对 `script.status` 的预期。

- [ ] **Step 4: 验证无脚本残留**

Run: `rg -n "NativeScriptComponent|ScriptableEntity|ScriptService|ScriptRuntimeSystem|script\\." HuaEngine/src Editor/src Tests CMakeLists.txt`

Expected: 无结果；如果文档或历史 fixture 有非编译引用，需确认是否删除或更新。

### Task 4: 构建和回归验证

**Files:**
- No source changes expected beyond earlier tasks.

- [ ] **Step 1: 构建核心和应用 target**

Run:
```powershell
cmake --build build --config Debug --target HuaEngine
cmake --build build --config Debug --target Editor
```

Expected: both targets build successfully.

- [ ] **Step 2: 构建并运行受影响 smoke**

Run:
```powershell
cmake --build build --config Debug --target ValidationServiceSmoke
cmake --build build --config Debug --target ApplicationServicesSmoke
cmake --build build --config Debug --target ApplicationOperationsSmoke
cmake --build build --config Debug --target AgentHostAdapterSmoke
cmake --build build --config Debug --target CLIWorkflowSmoke
.\build\bin\Debug-Windows-x64\smoke\ValidationServiceSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationServicesSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\AgentHostAdapterSmoke.exe
.\build\bin\Debug-Windows-x64\smoke\CLIWorkflowSmoke.exe
```

Expected: all listed smoke executables pass.

- [ ] **Step 3: 提交**

Run:
```powershell
git add <changed files>
git commit -m "refactor(script): remove native script runtime"
```

Expected: commit succeeds; pre-existing unrelated dirty files remain uncommitted.

## Self-Review

- Spec coverage: 覆盖删除组件、runtime、operation、validation、tests 和构建验证。
- Placeholder scan: 无 TBD/TODO/“后续处理”占位。
- Type consistency: 删除 `ScriptStatusReport`、`ScriptService`、`NativeScriptComponent` 后没有替代类型被引用。
