# Inspector 职责拆分实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将混合资产、场景和面板语义的 `InspectorPanel` 拆成纯路由 Panel、独立 Asset Inspector Editor 与独立 Scene Entity Inspector Editor。

**Architecture:** `EditorLayer` 拥有共享 `AssetPickerCatalog` 和两个领域 Editor，`InspectorPanel` 只根据 `EditorSelection` 分发绘制。资产 Dirty 生命周期归 `AssetInspectorEditor`，场景实体和组件 UI 归 `SceneEntityInspectorEditor`，跨文档保存仍由 `EditorLayer` 协调。

**Tech Stack:** C++20、ImGui、HuaEngine Editor Selection、Asset Authoring、ECS Component Registry、Editor Command Stack、CMake smoke targets。

**Spec:** `.workspace/superpower/specs/2026-08-25-inspector-responsibility-split-spec.md`

## 全局约束

- 所有文档主要描述语言使用中文，代码注释使用通俗英文。
- 不改变资产 Apply、Scene Save、Undo/Redo、Selection Guard 的用户行为。
- 不引入通用 `IInspectorEditor` 注册表或额外 Inspector 窗口。
- `InspectorPanel` 不得持有任何 Project、Workbench、SceneDocument、AssetRecord 或 Component Registry 状态。
- `.scene` 文件未打开时仍由 Asset Editor 处理；活动 SceneDocument 的 Entity 由 Scene Entity Editor 处理。
- 每个 P 完成后运行定向 smoke 并独立提交；P3 后运行 Debug `ALL_BUILD` 与 CMake 定义的全部 smoke。
- 用户已明确授权在当前 `master` 分支连续执行并提交；不得覆盖并发出现的无关工作区修改。

---

### P1：AssetPickerCatalog 与 AssetInspectorEditor

**Files:**
- Move: `Editor/src/Panels/AssetPickerModel.h` -> `Editor/src/Assets/AssetPickerModel.h`
- Move: `Editor/src/Panels/AssetPickerModel.cpp` -> `Editor/src/Assets/AssetPickerModel.cpp`
- Create: `Editor/src/Assets/AssetPickerCatalog.h`
- Create: `Editor/src/Assets/AssetPickerCatalog.cpp`
- Create: `Editor/src/Assets/AssetInspectorEditor.h`
- Create: `Editor/src/Assets/AssetInspectorEditor.cpp`
- Modify: `Editor/src/Panels/RuntimeInspector.h`
- Modify: `Editor/src/Panels/RuntimeInspector.cpp`
- Modify: `Editor/src/Panels/InspectorPanel.h`
- Modify: `Editor/src/Panels/InspectorPanel.cpp`
- Modify: `Editor/src/EditorLayer.h`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/EditorAssetPickerSmoke.cpp`
- Modify: `Tests/AssetEditingSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `AssetPickerCatalog::Rebuild`, `Clear`, `Get`。
- Produces: `AssetInspectorEditor::Draw`, `DrawModals`, `HasDirtyEdit`, `Apply`, `Revert`, `RequestDirtyResolution`, `CheckExternalModification`, `BindProject`。
- Consumes: `AssetInspectorHost`、`EditorWorkbenchState`、`ProjectContext`、`AssetPickerCatalog`、`ApplicationOperations`。

- [ ] **Step 1: 先写 AssetPickerCatalog 失败测试**

在 `EditorAssetPickerSmoke` 中改用 `Assets/AssetPickerModel.h`，并加入：

```cpp
HE::Editor::AssetPickerCatalog catalog;
catalog.Rebuild(records);
Require(catalog.Get(HE::AssetKind::Mesh).size() == 2, "Expected catalog mesh options");
Require(catalog.Get(HE::AssetKind::Material).size() == 3, "Expected catalog material options");
Require(catalog.Get(HE::AssetKind::Shader).empty(), "Expected empty shader options");
catalog.Clear();
Require(catalog.Get(HE::AssetKind::Mesh).empty(), "Expected cleared catalog");
```

- [ ] **Step 2: 验证 RED**

Run:

```powershell
cmake --build build --config Debug --target EditorAssetPickerSmoke --parallel 4
```

Expected: 编译失败，缺少 `Assets/AssetPickerModel.h` 或 `AssetPickerCatalog`。

- [ ] **Step 3: 移动 AssetPickerModel 并实现 Catalog**

`AssetPickerCatalog` 内部为 Mesh、Material、Texture2D、Shader 分别保存 `std::vector<AssetPickerOption>`；`Get(AssetKind)` 对不支持类型返回空 span。更新所有 include 和 CMake source 路径，确保 Assets 不再依赖 Panels 中的数据模型。

- [ ] **Step 4: 为 AssetInspectorEditor 写编译契约测试**

在 `AssetEditingSmoke` 中包含新头文件并验证空状态：

```cpp
HE::Editor::AssetPickerCatalog pickerCatalog;
HE::Editor::AssetInspectorEditor assetInspector(pickerCatalog);
Require(!assetInspector.HasDirtyEdit(), "Expected clean asset inspector without selection");
assetInspector.BindProject(nullptr);
Require(!assetInspector.HasDirtyEdit(), "Expected unbound asset inspector to remain clean");
```

Run `AssetEditingSmoke` 构建，Expected: RED，类型尚不存在。

- [ ] **Step 5: 从 InspectorPanel 迁移资产生命周期**

迁移以下实现和状态到 `AssetInspectorEditor`：

- `AssetInspectorHost`。
- 待刷新 GUID。
- Apply、Revert、Reimport、Reload、External Modification。
- Dirty Selection Guard、Dirty Modal 和 continuation。
- 资产标题、导入健康、依赖与具体 `IAssetEditor::Draw`。
- Shader、Texture 选项从 `AssetPickerCatalog` 读取。

`InspectorPanel` 在本阶段只暂时调用 `AssetInspectorEditor::Draw()`；删除其资产成员和资产方法。

- [ ] **Step 6: 改造 EditorLayer 直接持有并调用 AssetInspectorEditor**

将以下调用从 `m_Inspector` 改为 `m_AssetInspectorEditor`：

```cpp
HasDirtyEdit()
Apply(&state)
Revert()
DrawModals()
CheckExternalModification()
BindProject(...)
```

`RefreshInspectorAssetCatalog` 改名为 `RefreshAssetPickerCatalog`，直接调用共享 Catalog。Project 关闭时解绑 Asset Editor 并清空 Catalog。

- [ ] **Step 7: 验证 GREEN 与结构边界**

Run:

```powershell
cmake --build build --config Debug --target EditorAssetPickerSmoke AssetEditingSmoke Editor --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\EditorAssetPickerSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\AssetEditingSmoke.exe
rg -n "AssetInspectorHost|ApplyAssetEdit|OnDirtyAssetPopup|m_.*AssetOptions" Editor/src/Panels/InspectorPanel.*
```

Expected: 两个 smoke 通过，Editor 构建成功，结构扫描在 InspectorPanel 中无匹配。

- [ ] **Step 8: 提交 P1**

```powershell
git add -- CMakeLists.txt Editor/src/Assets Editor/src/Panels/AssetPickerModel.* Editor/src/Panels/RuntimeInspector.* Editor/src/Panels/InspectorPanel.* Editor/src/EditorLayer.* Tests/EditorAssetPickerSmoke.cpp Tests/AssetEditingSmoke.cpp
git commit -m "refactor(editor): extract asset inspector editor"
```

---

### P2：SceneEntityInspectorEditor 与纯路由 InspectorPanel

**Files:**
- Create: `Editor/src/Scene/SceneEntityInspectorEditor.h`
- Create: `Editor/src/Scene/SceneEntityInspectorEditor.cpp`
- Modify: `Editor/src/Panels/InspectorPanel.h`
- Modify: `Editor/src/Panels/InspectorPanel.cpp`
- Modify: `Editor/src/EditorLayer.h`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/EditorInteractionSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `AssetPickerCatalog`、`EditorInteractionHost`、`EditorSelectionService`、`ApplicationOperations`。
- Produces: `SceneEntityInspectorEditor::Draw`、`SetAddComponentCallback`、`SetRemoveComponentCallback`。
- Produces: 只接收两个领域 Editor 引用的 `InspectorPanel` 构造函数。

- [ ] **Step 1: 写 SceneEntityInspectorEditor 失败编译测试**

在 `EditorInteractionSmoke` 中包含 `Scene/SceneEntityInspectorEditor.h`，使用共享 Catalog 构造，并验证无 Host 时不会报告修改：

```cpp
HE::Editor::AssetPickerCatalog catalog;
HE::Editor::SceneEntityInspectorEditor sceneInspector(catalog);
sceneInspector.BindInteractionHost(nullptr);
Require(!sceneInspector.HasEditingContext(), "Expected no scene editing context without host");
```

Run EditorInteractionSmoke 构建，Expected: RED，头文件或类型不存在。

- [ ] **Step 2: 实现场景实体编辑器并迁移组件 UI**

从 InspectorPanel 迁移：

- Runtime type 到 inspectable component 的映射。
- Component header label。
- 单选与多选摘要。
- Runtime Component Inspector Context。
- Material reference/override command 回调。
- Component context menu。
- Add Component 窗口。
- Component Registry 与 Runtime Override Registry。

`HasEditingContext()` 只检查已绑定 Host 是否拥有活动 SceneDocument，供无 ImGui 环境 smoke 验证。

- [ ] **Step 3: 将 InspectorPanel 缩减为路由外壳**

构造函数接收：

```cpp
InspectorPanel(
    Editor::AssetInspectorEditor& assetEditor,
    Editor::SceneEntityInspectorEditor& sceneEditor);
```

`OnGuiRender()` 仅 Begin、读取 Selection variant、调用对应 Draw、绘制空状态、End。Project、Validation、Scene 摘要从 Inspector 顶部删除。

- [ ] **Step 4: 更新 EditorLayer 所有权与场景回调**

`EditorLayer` 按依赖顺序拥有 Catalog、Asset Editor、Scene Entity Editor、InspectorPanel。Add/Remove Component 回调绑定到 Scene Editor；InspectorPanel 不再接触 `EditorInteractionHost`。

- [ ] **Step 5: 验证 GREEN 与场景数据流**

Run:

```powershell
cmake --build build --config Debug --target EditorInteractionSmoke EditorInspectorRuntimeSmoke Editor --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\EditorInteractionSmoke.exe
& .\build\bin\Debug-Windows-x64\smoke\EditorInspectorRuntimeSmoke.exe
rg -n "ComponentRegistry|RuntimeOverrides|InteractionHost|WorkbenchState|ProjectContext|AssetRecord" Editor/src/Panels/InspectorPanel.*
```

Expected: smoke 通过，Editor 构建成功，结构扫描无领域状态匹配。

- [ ] **Step 6: 提交 P2**

```powershell
git add -- CMakeLists.txt Editor/src/Scene Editor/src/Panels/InspectorPanel.* Editor/src/EditorLayer.* Tests/EditorInteractionSmoke.cpp
git commit -m "refactor(editor): extract scene entity inspector"
```

---

### P3：SceneAssetEditor 专属依赖与最终清理

**Files:**
- Modify: `Editor/src/Assets/AssetEditor.h`
- Modify: `Editor/src/Assets/AssetInspectorHost.h`
- Modify: `Editor/src/Assets/AssetInspectorHost.cpp`
- Modify: `Editor/src/Assets/AssetInspectorEditor.h`
- Modify: `Editor/src/Assets/AssetInspectorEditor.cpp`
- Modify: `Editor/src/Assets/Editors/SceneAssetEditor.h`
- Modify: `Editor/src/Assets/Editors/SceneAssetEditor.cpp`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/AssetEditingSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `SceneAssetEditorServices`，包含 OpenScene 与活动 SceneDocument 状态查询。
- Consumes: `AssetInspectorHost(SceneAssetEditorServices services)`。
- Removes: `AssetEditorDrawContext::OpenScene`、`ActiveScenePath`、`ActiveSceneDirty`。

- [ ] **Step 1: 写 SceneAssetEditorServices 失败测试**

在 `AssetEditingSmoke` 中通过专属服务构造 Host：

```cpp
bool sceneOpenRequested = false;
HE::Editor::SceneAssetEditorServices sceneServices{
    .OpenScene = [&](const std::filesystem::path&) { sceneOpenRequested = true; },
    .GetActiveDocument = [] { return HE::Editor::SceneAssetDocumentState{}; }
};
HE::Editor::AssetInspectorHost sceneHost(sceneServices);
```

同时将测试中的通用 `AssetEditorDrawContext` 初始化保持为纯资产字段。Run AssetEditingSmoke build，Expected: RED，专属服务尚不存在。

- [ ] **Step 2: 注入 SceneAssetEditor 专属服务**

定义：

```cpp
struct SceneAssetDocumentState {
    std::filesystem::path ActiveScenePath;
    bool Dirty = false;
};

struct SceneAssetEditorServices {
    std::function<void(const std::filesystem::path&)> OpenScene;
    std::function<SceneAssetDocumentState()> GetActiveDocument;
};
```

`AssetInspectorHost` 注册 Scene editor factory 时按值捕获 services。`SceneAssetEditor::Draw` 只从自身 services 读取场景状态；其他 `IAssetEditor` 不可见这些字段。

- [ ] **Step 3: 清理通用 AssetEditorDrawContext 与 EditorLayer 旧路径**

删除通用 Draw Context 的三个 Scene 字段。`AssetInspectorEditor` 创建 Host 时注入 Scene services，EditorLayer 只提供 Workbench Action callback 和当前 SceneDocument 状态查询。

- [ ] **Step 4: 执行定向与全量验证**

Run:

```powershell
cmake --build build --config Debug --target AssetEditingSmoke Editor --parallel 4
& .\build\bin\Debug-Windows-x64\smoke\AssetEditingSmoke.exe
rg -n "OpenScene|ActiveScenePath|ActiveSceneDirty" Editor/src/Assets/AssetEditor.h
rg -n "ProjectContext|WorkbenchState|SceneDocument|AssetRecord|ComponentRegistry|AssetInspectorHost" Editor/src/Panels/InspectorPanel.*
cmake --build build --config Debug --target ALL_BUILD --parallel 4
```

然后从 CMake 提取全部 `*Smoke` 目标并逐个运行，要求失败数为 0。最后运行：

```powershell
git diff --check
```

- [ ] **Step 5: 提交 P3**

```powershell
git add -- CMakeLists.txt Editor/src/Assets Editor/src/EditorLayer.* Tests/AssetEditingSmoke.cpp
git commit -m "refactor(editor): isolate scene asset editor services"
```

---

## 完成检查

- [ ] `InspectorPanel.cpp` 只包含窗口和 Selection 路由。
- [ ] EditorLayer 直接访问 AssetInspectorEditor，不存在资产操作经 InspectorPanel 转发。
- [ ] SceneEntityInspectorEditor 不持有独立 Scene Dirty 状态。
- [ ] AssetEditorDrawContext 无 Scene 字段。
- [ ] P1、P2、P3 各有独立 commit。
- [ ] Debug `ALL_BUILD` 成功。
- [ ] CMake 当前定义的全部 smoke 通过。
- [ ] 无关工作区变更未被提交。
