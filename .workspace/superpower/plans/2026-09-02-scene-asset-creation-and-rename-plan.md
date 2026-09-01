# Scene 资产创建与通用 Rename 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 删除 Editor 顶部 Scene 菜单，使 Scene 通过 Project 面板立即创建为真实资产，并提供保持 GUID 稳定的通用文件资产 Rename 与 Save All 流程。

**Architecture:** 引擎侧由 `ApplicationOperations::CreateSceneAsset` 编排 Scene 序列化与注册，由 `AssetService::RenameAsset` 原子更新源文件、meta、Manifest 和 Registry。编辑器侧以 `AssetCreationRegistry` 声明创建类型，以 `AssetWorkspaceController` 执行创建与 Rename，ProjectPanel 只产生 UI action，EditorLayer 仅协调文档和会话状态。

**Tech Stack:** C++20、Dear ImGui、HuaEngine AssetService/SceneService/ApplicationOperations、JSON Manifest、YAML Scene/Meta、CMake smoke targets。

**Spec:** `.workspace/superpower/specs/2026-09-02-scene-asset-creation-and-rename-spec.md`

## 全局约束

- 只有真正位于当前项目 `Assets` 下的 `AssetSource::File` 可 Rename。
- Rename 必须保持 AssetGuid 与 AssetHandle 不变，且不得移动 Library artifact。
- 新建 Scene 立即持久化和注册，但不自动打开；新项目 bootstrap 明确创建并打开初始 Scene。
- ProjectPanel 不直接调用 Application、SceneService 或 AssetService。
- 创建与 Rename 不覆盖已有文件。
- 代码注释使用简明英文。
- 每个 P 按 RED、GREEN、定向验证、独立 commit 的顺序完成。
- 不提交执行期间发现的无关工作区变更。

---

### P1：保持身份的通用文件资产 Rename

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetManifest.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetManifest.cpp`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetRegistry.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetService.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `Tests/AssetServiceSmoke.cpp`

**Interfaces:**
- Produces: `AssetManifest::ReplaceByGuid(AssetManifestRecord record)`。
- Produces: `AssetRegistry::ReplaceByGuid(AssetRecord record)`。
- Produces: `AssetService::RenameAsset(const ProjectContext&, const AssetGuid&, std::string_view, AssetRecord*)`。
- Produces: 同签名的 `ApplicationOperations::RenameAsset` facade。

- [ ] **Step 1: 在 AssetServiceSmoke 写 Rename RED 测试**

创建并注册一个 `Assets/Scenes/Original.scene`，随后验证期望接口：

```cpp
HE::AssetRecord before;
Require(assetService.ResolveAsset("Scenes/Original.scene", before).Succeeded(), "Expected source record");

HE::AssetRecord renamed;
auto rename = assetService.RenameAsset(context, before.Guid, "Renamed", &renamed);
Require(rename.Succeeded(), "Expected file asset rename to succeed");
Require(renamed.Guid == before.Guid, "Expected stable guid");
Require(renamed.Handle == before.Handle, "Expected stable handle");
Require(renamed.AssetId == "Scenes/Renamed.scene", "Expected renamed asset id");
Require(!std::filesystem::exists(originalPath), "Expected old source to disappear");
Require(std::filesystem::exists(renamed.AbsolutePath), "Expected new source");
Require(std::filesystem::exists(HE::GetAssetMetaPath(renamed.AbsolutePath)), "Expected moved meta");
```

同一测试覆盖空名称、`..`、路径分隔符、同目录重名、Builtin GUID 和缺失源文件均失败，且失败后旧记录仍可解析。

- [ ] **Step 2: 构建并确认 RED**

```powershell
cmake --build build --config Debug --target AssetServiceSmoke --parallel 8
```

预期：因 `RenameAsset` 和 identity replacement 接口不存在而编译失败。

- [ ] **Step 3: 实现 Manifest 与 Registry 的 identity replacement**

`ReplaceByGuid` 只允许已有 GUID；先检查新 AssetId 未被其他 GUID 占用，再移除旧 AssetId 索引、写入记录并建立新索引。Registry 复用原 Handle，规则与 Manifest 一致。

```cpp
[[nodiscard]] bool ReplaceByGuid(AssetManifestRecord record);
[[nodiscard]] bool ReplaceByGuid(AssetRecord record);
```

- [ ] **Step 4: 实现 AssetService::RenameAsset**

按以下顺序执行：

1. 通过 GUID 解析旧记录并校验 Source、源文件、Assets 根目录和名称。
2. 保留原扩展名，以 `parent / (newBaseName + extension)` 生成目标。
3. 检查目标源文件与目标 meta 均不存在。
4. 复制 Manifest/Registry 快照。
5. `std::filesystem::rename` 移动源文件，再移动 meta。
6. Replace Manifest 与 Registry，并 `SaveAssetManifest`。
7. 任一步失败时恢复快照并将已经移动的文件移回旧路径。
8. 成功结果 payload 写入 `asset_guid`、`old_asset_id`、`asset_id`、`old_asset_path`、`asset_path`。

- [ ] **Step 5: 接入 ApplicationOperations 并验证 GREEN**

```powershell
cmake --build build --config Debug --target AssetServiceSmoke ApplicationOperationsSmoke --parallel 8
& .\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
```

- [ ] **Step 6: 提交 P1**

```powershell
git add HuaEngine/src/HuaEngine/Asset/AssetManifest.* HuaEngine/src/HuaEngine/Asset/AssetRegistry.h HuaEngine/src/HuaEngine/Asset/AssetService.* HuaEngine/src/HuaEngine/Application/ApplicationOperations.* Tests/AssetServiceSmoke.cpp
git commit -m "feat(asset): add identity-preserving asset rename"
```

---

### P2：原子创建 Scene 资产

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `Tests/ApplicationOperationsSmoke.cpp`

**Interfaces:**
- Consumes: `SceneService::CreateScene`、`SceneService::SaveScene`、`AssetService::RegisterSceneAsset`。
- Produces: `ApplicationOperations::CreateSceneAsset(const ProjectContext&, const std::filesystem::path&, AssetGuid*)`。

- [ ] **Step 1: 写 CreateSceneAsset RED 测试**

```cpp
const auto scenePath = projectContext.GetAssetRootPath() / "Scenes" / "Created.scene";
HE::AssetGuid sceneGuid;
auto create = operations.CreateSceneAsset(projectContext, scenePath, &sceneGuid);
Require(create.Succeeded() && !sceneGuid.empty(), "Expected scene asset creation");
Require(std::filesystem::is_regular_file(scenePath), "Expected scene source");
Require(std::filesystem::is_regular_file(HE::GetAssetMetaPath(scenePath)), "Expected scene meta");

HE::AssetRecord record;
std::vector<HE::AssetRecord> records;
Require(operations.ListAssets(projectContext, records).Succeeded(), "Expected asset listing");
const auto recordIt = std::find_if(records.begin(), records.end(), [&](const auto& candidate) { return candidate.Guid == sceneGuid; });
Require(recordIt != records.end(), "Expected scene registry record");
record = *recordIt;
Require(record.Kind == HE::AssetKind::Scene, "Expected scene kind");
Require(!operations.CreateSceneAsset(projectContext, scenePath).Succeeded(), "Expected no overwrite");
```

- [ ] **Step 2: 构建并确认 RED**

```powershell
cmake --build build --config Debug --target ApplicationOperationsSmoke --parallel 8
```

- [ ] **Step 3: 实现 CreateSceneAsset**

预检要求目标位于 Assets 下、扩展名为 `.scene` 且源文件/meta 均不存在。Scene 名使用 `assetPath.stem().string()`。Save 成功而 Register 失败时，仅删除本操作新建的 Scene 与 meta；成功结果透传 GUID、AssetId 和绝对路径。

- [ ] **Step 4: 验证 GREEN**

```powershell
cmake --build build --config Debug --target ApplicationOperationsSmoke AssetServiceSmoke --parallel 8
& .\build\bin\Debug-Windows-x64\smoke\ApplicationOperationsSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\AssetServiceSmoke.exe
```

- [ ] **Step 5: 提交 P2**

```powershell
git add HuaEngine/src/HuaEngine/Application/ApplicationOperations.* Tests/ApplicationOperationsSmoke.cpp
git commit -m "feat(asset): create persisted scene assets"
```

---

### P3：资产创建注册、默认命名与 ProjectPanel 原地 Rename

**Files:**
- Create: `Editor/src/Assets/AssetCreationRegistry.h`
- Create: `Editor/src/Assets/AssetCreationRegistry.cpp`
- Create: `Editor/src/Assets/AssetWorkspaceController.h`
- Create: `Editor/src/Assets/AssetWorkspaceController.cpp`
- Create: `Editor/src/Panels/ProjectAssetNaming.h`
- Create: `Editor/src/Panels/ProjectAssetNaming.cpp`
- Modify: `Editor/src/Panels/ProjectPanel.h`
- Modify: `Editor/src/Panels/ProjectPanel.cpp`
- Modify: `Tests/ProjectPanelActionSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `AssetCreationDescriptor` 与 `AssetCreationRegistry::Register/Find/GetAll`。
- Produces: `GenerateUniqueAssetPath(directory, baseName, extension)` 与 `ValidateAssetBaseName(name)`。
- Produces: `ProjectPanelActionType::CreateAsset`、`RenameAsset`。
- Produces: `ProjectPanel::SetCreationRegistry`、`BeginRename`、`CancelRename`。
- Produces: `AssetWorkspaceController::RegisterCreator/CreateAsset/SetRenameHandler/RenameAsset`。

- [ ] **Step 1: 扩展 ProjectPanelActionSmoke 为 RED**

```cpp
HE::Editor::AssetCreationRegistry registry;
Require(registry.Register({ "scene", HE::AssetKind::Scene, "Scene", "New Scene", ".scene" }).Succeeded(), "Expected scene creator registration");

const auto first = HE::Editor::GenerateUniqueAssetPath(assetDirectory, "New Scene", ".scene");
Require(first.filename() == "New Scene.scene", "Expected default scene name");
std::ofstream(first.string()).put('\n');
const auto second = HE::Editor::GenerateUniqueAssetPath(assetDirectory, "New Scene", ".scene");
Require(second.filename() == "New Scene 1.scene", "Expected incremented scene name");

Require(HE::Editor::ValidateAssetBaseName("Renamed").Succeeded(), "Expected valid base name");
Require(HE::Editor::ValidateAssetBaseName("../Renamed").Failed(), "Expected separator rejection");
```

同时验证 `MakeProjectCreateAssetAction(typeId, directory)` 和 `MakeProjectRenameAssetAction(guid, baseName)` 保留完整参数。

- [ ] **Step 2: 构建并确认 RED**

```powershell
cmake --build build --config Debug --target ProjectPanelActionSmoke --parallel 8
```

- [ ] **Step 3: 实现 Registry、Naming 与 Controller**

Controller 不依赖 Application 单例，通过以下 handler 注入真实操作，因此新增资产类型时不修改 Controller 分支：

```cpp
using AssetCreateHandler = std::function<ResultEnvelope(const std::filesystem::path&, AssetGuid*)>;
using AssetRenameHandler = std::function<ResultEnvelope(const AssetGuid&, std::string_view, AssetRecord*)>;

ResultEnvelope RegisterCreator(std::string typeId, AssetCreateHandler handler);
void SetRenameHandler(AssetRenameHandler handler);
AssetWorkspaceMutation CreateAsset(const AssetCreationRegistry&, std::string_view typeId, const std::filesystem::path& targetDirectory) const;
AssetWorkspaceMutation RenameAsset(const AssetGuid&, std::string_view newBaseName) const;
```

`ProjectPanelActionSmoke` 使用计数 lambda 验证 Controller 将唯一默认路径传给创建 handler，并将 GUID、OldPath、NewPath 和 `BeginRename` 放入返回结果。EditorLayer 在 P4 捕获当前 `ProjectContext` 注入 `CreateSceneAsset` 和 `RenameAsset` handler。

- [ ] **Step 4: 实现 ProjectPanel UI**

- 目录节点与 Assets 空白区域右键显示 Registry 中的 `Create` 子菜单。
- 文件右键增加 `Rename`，Builtin 或无记录文件禁用。
- 正在 Rename 的 GUID 使用固定宽度 `ImGui::InputText` 替代 Selectable。
- Enter 和失焦产生 RenameAsset action；Esc 取消。
- 新建成功后外部调用 `BeginRename(guid)`，Panel 在下一帧聚焦输入框并全选名称主体。

- [ ] **Step 5: 验证 P3**

```powershell
cmake --build build --config Debug --target ProjectPanelActionSmoke Editor --parallel 8
& .\build\bin\Debug-Windows-x64\smoke\ProjectPanelActionSmoke.exe
```

- [ ] **Step 6: 提交 P3**

```powershell
git add CMakeLists.txt Editor/src/Assets/AssetCreationRegistry.* Editor/src/Assets/AssetWorkspaceController.* Editor/src/Panels/ProjectAssetNaming.* Editor/src/Panels/ProjectPanel.* Tests/ProjectPanelActionSmoke.cpp
git commit -m "feat(editor): add project asset creation and rename"
```

---

### P4：工作台集成、Save All 与 Scene 菜单退役

**Files:**
- Modify: `Editor/src/Workbench/SceneDocument.h`
- Modify: `Editor/src/Workbench/EditorSessionStorage.h`
- Modify: `Editor/src/EditorLayer.h`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/ProjectWorkbenchSmoke.cpp`
- Modify: `Tests/EditorInputSmoke.cpp`
- Modify: `Tests/ProjectPanelActionSmoke.cpp`

**Interfaces:**
- Consumes: P1 `RenameAsset`、P2 `CreateSceneAsset`、P3 ProjectPanel actions 和 Controller。
- Produces: `PersistedEditorSession::RenameScenePath(oldPath, newPath)`。
- Produces: `editor.document.save_all` Command 和 Ctrl+S binding。
- Removes: 无路径 SceneDocument、Scene 顶部菜单和三个 Scene Modal。

- [ ] **Step 1: 写工作台状态 RED 测试**

在 `ProjectWorkbenchSmoke` 验证路径迁移：

```cpp
persisted.LastScenePath = oldPath.generic_string();
persisted.UpsertSceneCameraPose({ .ScenePath = oldPath.generic_string(), .PositionX = 4.0f });
persisted.RenameScenePath(oldPath, newPath);
Require(persisted.LastScenePath == newPath.generic_string(), "Expected last scene rename");
Require(persisted.FindSceneCameraPose(newPath.generic_string()) != nullptr, "Expected camera pose rename");
Require(persisted.FindSceneCameraPose(oldPath.generic_string()) == nullptr, "Expected old camera pose removal");
```

在 `EditorInputSmoke` 将保存命令与 binding 改为 `editor.document.save_all` 并断言 Ctrl+S 只执行一次。

- [ ] **Step 2: 构建并确认 RED**

```powershell
cmake --build build --config Debug --target ProjectWorkbenchSmoke EditorInputSmoke --parallel 8
```

- [ ] **Step 3: 集成 ProjectPanel actions**

EditorLayer 初始化 Scene creation descriptor，将 Registry 与 InputService 注入 ProjectPanel。处理 CreateAsset 时调用 Controller，成功后刷新 catalog、选择 GUID 并 `BeginRename`；处理 RenameAsset 前检查同 GUID 的 Asset Inspector dirty 状态，成功后刷新并恢复 GUID 选择。

当 Rename 返回旧/新路径且旧路径等于当前 SceneDocument 路径时，更新 SceneDocument DisplayName/ScenePath、ProjectSession、PersistedEditorSession 和相机 pose，保持 dirty 状态。

- [ ] **Step 4: 将新项目 bootstrap 改为真实 Scene 资产**

用以下流程替代 `CreateNewSceneDocument`：

```text
GenerateUniqueAssetPath(Assets, InitialSceneName, .scene)
ApplicationOperations::CreateSceneAsset
OpenSceneDocument(createdPath)
```

普通 ProjectPanel 创建不调用 OpenSceneDocument。

- [ ] **Step 5: 实现 Save All 并删除 Scene 菜单**

注册：

```cpp
registerCommand({
    "editor.document.save_all", "Save All", "File",
    [this]() { return m_SceneDocument.IsLoaded() || m_AssetInspectorEditor->HasDirtyEdit(); },
    [this]() { SaveAllModified(); }
});
```

`SaveAllModified` 先 Apply dirty Asset Inspector，再在前一步成功时保存 dirty Scene。删除旧 Scene new/open/save/save_as/validate 顶层命令、Modal 请求标记和输入缓冲；Dockspace 菜单改为 `File > Save All`，不再绘制 `Scene` 菜单。

`SceneDocument` 删除 `NewScene` source；`SetSceneDocument` 对空路径返回失败或断言，所有调用点只传现有资产路径。

- [ ] **Step 6: 验证 P4 定向测试与结构扫描**

```powershell
cmake --build build --config Debug --target ProjectWorkbenchSmoke EditorInputSmoke ProjectPanelActionSmoke Editor --parallel 8
& .\build\bin\Debug-Windows-x64\smoke\ProjectWorkbenchSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\EditorInputSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\ProjectPanelActionSmoke.exe
rg -n 'editor\.scene\.(new|open|save_as)|BeginMenu\("Scene"|New Scene"|Open Scene"|Save Scene As"' Editor/src
```

预期：结构扫描无旧顶层 Scene 生命周期入口匹配；允许 Scene Asset Inspector 的 `Open Scene` 按钮存在，因此扫描限定 EditorLayer 和命令 ID 时应无匹配。

- [ ] **Step 7: 完整验证**

```powershell
cmake --build build --config Debug --parallel 8
```

从根 `CMakeLists.txt` 当前 `add_executable(...Smoke)` 提取正式 smoke 目标，并从仓库根目录逐个执行，要求全部退出码为 0。

- [ ] **Step 8: 提交 P4**

```powershell
git diff --check
git add Editor/src/Workbench/SceneDocument.h Editor/src/Workbench/EditorSessionStorage.h Editor/src/EditorLayer.* Tests/ProjectWorkbenchSmoke.cpp Tests/EditorInputSmoke.cpp Tests/ProjectPanelActionSmoke.cpp
git commit -m "refactor(editor): make scene lifecycle asset-centric"
```

---

## 完成检查

- [ ] P1-P4 各有独立提交。
- [ ] Scene 创建后立即存在 `.scene`、`.meta`、Manifest 和 Registry 记录。
- [ ] ProjectPanel 创建 Scene 后选中并 Rename，但不自动打开。
- [ ] Rename 保持 GUID 与 Handle，不移动 Library artifact。
- [ ] 打开 Scene Rename 后文档、项目会话和相机 pose 路径同步。
- [ ] 不再存在无路径 SceneDocument。
- [ ] 顶部 Scene 菜单和三个 Scene Modal 已删除。
- [ ] Ctrl+S 唯一映射到 Save All。
- [ ] Debug 全量构建通过。
- [ ] 当前正式 smoke 全部通过。
