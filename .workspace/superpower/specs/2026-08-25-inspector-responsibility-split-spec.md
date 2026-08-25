# Inspector 职责拆分规格

## 1. 背景

当前 `InspectorPanel` 同时承担以下职责：

- ImGui Inspector 窗口的创建与选择路由。
- 资产工作副本加载、Apply、Revert、Reimport 和外部修改检测。
- 资产未保存修改的 Selection Guard 与确认弹窗。
- 活动场景实体的单选、多选和组件字段编辑。
- 组件添加、删除和上下文菜单。
- Mesh、Material、Texture、Shader 资产选择列表维护。
- Project、Scene 和 Validation 状态展示。

这些职责跨越资产编辑、场景实体编辑和 Workbench 协调三个领域，使 `InspectorPanel` 无法作为纯粹的 UI 路由容器，也迫使 `EditorLayer` 通过 Inspector 间接访问资产编辑状态。

## 2. 目标

将 Inspector 重构为组合式双编辑器架构：

1. `InspectorPanel` 只创建 Inspector 窗口并按 Selection 类型分发绘制。
2. `AssetInspectorEditor` 完整拥有磁盘资产工作副本的编辑生命周期。
3. `SceneEntityInspectorEditor` 完整拥有活动场景实体和组件的 Inspector UI。
4. `EditorLayer` 直接协调资产工作副本与 `SceneDocument` 的跨文档保存行为。
5. 使用 `AssetPickerCatalog` 为资产编辑器和场景实体编辑器提供共享、只读的资产选择数据。

本次重构必须保持现有编辑器行为，不新增资产格式、组件类型或场景功能。

## 3. 非目标

- 不建立通用 `IInspectorEditor` 插件注册表。
- 不拆出独立的 Asset Inspector 和 Entity Inspector 窗口。
- 不把 `SceneDocument` 所有权移出 `EditorLayer`。
- 不修改资产导入、资产 Library、ECS 或 RenderGraph 的运行时语义。
- 不改变 Undo/Redo、场景保存和资产 Apply 的用户交互结果。

## 4. 核心边界

### 4.1 磁盘资产与活动场景

- Project 面板中尚未打开的 `.scene` 文件仍属于磁盘资产，由 `AssetInspectorEditor` 中的 `SceneAssetEditor` 展示摘要并提供打开命令。
- 已打开的 `SceneDocument`、实体 Selection 和组件字段由 `SceneEntityInspectorEditor` 处理。
- `SceneEntityInspectorEditor` 不负责打开、保存或关闭场景文档。
- `AssetInspectorEditor` 不直接编辑活动场景中的实体或组件。

### 4.2 InspectorPanel

`InspectorPanel` 只允许承担以下职责：

- `ImGui::Begin("Inspector")` 与 `ImGui::End()`。
- 读取当前 `EditorSelection`。
- 将 Asset Selection 分发给 `AssetInspectorEditor::Draw()`。
- 将 Entity Selection 分发给 `SceneEntityInspectorEditor::Draw()`。
- 无选择时绘制空状态。
- 返回场景实体字段本帧是否发生修改。

`InspectorPanel` 不得持有 Project、SceneDocument、WorkbenchState、AssetRecord、ComponentRegistry、资产工作副本或 Dirty Modal 状态。

### 4.3 EditorLayer

`EditorLayer` 继续负责跨文档协调：

- 关闭工程、切换场景和退出前，同时检查资产工作副本与 `SceneDocument` 的 Dirty 状态。
- `Ctrl+S` 根据当前编辑目标调用资产 Apply 或场景保存。
- 将打开 Scene 资产的请求转换为 Workbench Action。
- 在项目资产清单刷新后重建共享 `AssetPickerCatalog`。

`EditorLayer` 必须直接访问 `AssetInspectorEditor`，不得再通过 `InspectorPanel` 转发资产 Apply、Revert 或 Dirty 查询。

## 5. 组件设计

### 5.1 AssetPickerCatalog

位置：

```text
Editor/src/Assets/AssetPickerCatalog.h
Editor/src/Assets/AssetPickerCatalog.cpp
Editor/src/Assets/AssetPickerModel.h
Editor/src/Assets/AssetPickerModel.cpp
```

职责：

- 从 `std::span<const AssetRecord>` 一次性构建各 `AssetKind` 的 `AssetPickerOption`。
- 为调用方返回只读 `std::span<const AssetPickerOption>`。
- 项目关闭时清空所有选项。

建议接口：

```cpp
class AssetPickerCatalog {
public:
    void Rebuild(std::span<const AssetRecord> records);
    void Clear();
    [[nodiscard]] std::span<const AssetPickerOption> Get(AssetKind kind) const;
};
```

Asset Editor 消费 Shader 和 Texture 选项；Scene Entity Editor 消费 Mesh 和 Material 选项。

现有 `Panels/AssetPickerModel.h/.cpp` 是无 UI 的资产选择数据模型，应在本阶段移动到 `Assets/AssetPickerModel.h/.cpp`。资产领域代码不得为了使用 `AssetPickerOption` 反向依赖 `Panels` 目录。

### 5.2 AssetInspectorEditor

位置：

```text
Editor/src/Assets/AssetInspectorEditor.h
Editor/src/Assets/AssetInspectorEditor.cpp
```

职责：

- 持有 `AssetInspectorHost`。
- 根据当前 Asset Selection 打开或刷新资产工作副本。
- 绘制资产身份、导入健康状态、依赖关系和具体 `IAssetEditor`。
- 处理 Apply、Revert、Reimport、Reload 和外部修改检测。
- 持有资产 Dirty Modal、延迟 continuation 和待刷新 GUID。
- 在自身生命周期内安装并移除资产 Selection Guard。
- 项目切换或关闭时清理工作副本和待执行状态。

对 `EditorLayer` 暴露：

```cpp
bool Draw();
void DrawModals();
bool HasDirtyEdit() const;
ResultEnvelope Apply(AssetApplyState* outState = nullptr);
void Revert();
bool RequestDirtyResolution(std::function<void()> continuation);
void BindProject(const ProjectContext* projectContext);
void SetOpenSceneCallback(std::function<void(const std::filesystem::path&)> callback);
```

资产操作结果可写入 `EditorWorkbenchState`，但该依赖只属于 Asset Editor，不得回流到 `InspectorPanel`。

### 5.3 SceneEntityInspectorEditor

位置：

```text
Editor/src/Scene/SceneEntityInspectorEditor.h
Editor/src/Scene/SceneEntityInspectorEditor.cpp
```

职责：

- 持有 Core Component Registry 与 Runtime Component Override Registry。
- 解析活动 `SceneDocument` 中的 Entity Selection。
- 绘制单实体、多实体摘要和组件字段。
- 绘制组件上下文菜单与 Add Component 窗口。
- 通过 `EditorInteractionHost` 和 Editor Command 路径修改场景。
- 使用共享 `AssetPickerCatalog` 提供 Mesh 与 Material 选择。

该类不持有独立 Dirty 标记。组件编辑产生的命令由 `EditorInteractionHost` 更新 CommandStack，再同步到 `SceneDocument::Dirty`。

### 5.4 SceneAssetEditor 专属依赖

通用 `AssetEditorDrawContext` 不得继续包含：

- `OpenScene`
- `ActiveScenePath`
- `ActiveSceneDirty`

这些依赖只服务于 `SceneAssetEditor`。应通过创建 `SceneAssetEditor` 时注入的专属服务或回调提供，避免所有资产编辑器共享 Scene 语义。

Material 和 Shader 编辑器仍可通过通用资产 Draw Context 获取 Shader、Texture 选择列表及资产相关操作。

## 6. Selection 与 Dirty 数据流

### 6.1 Asset Selection 切换

1. Selection Service 收到目标选择。
2. `AssetInspectorEditor` 安装的 Change Guard 检查当前工作副本。
3. 工作副本无修改时允许切换。
4. 工作副本有修改时拒绝本次切换，保存目标 Selection 并打开 Dirty Modal。
5. Apply 或 Discard 成功后，通过 continuation 接受原目标 Selection。
6. Cancel 保持当前资产选择和工作副本。

`InspectorPanel` 不参与该流程。

### 6.2 Scene Dirty

1. `SceneEntityInspectorEditor` 生成字段修改或组件结构命令。
2. `EditorInteractionHost` 执行命令。
3. CommandStack 更新 Dirty 状态。
4. `SceneDocument` 接收同步后的 Dirty 状态。
5. `EditorLayer` 在离开文档前统一处理保存提示。

### 6.3 跨文档离开保护

`EditorLayer` 分别查询：

```cpp
const bool assetDirty = m_AssetInspectorEditor->HasDirtyEdit();
const bool sceneDirty = m_SceneDocument.IsLoaded() && m_SceneDocument.Dirty;
```

Apply/Save 和 Discard 必须分别调用两个领域对象，不得再调用 `InspectorPanel` 的资产代理接口。

## 7. 生命周期

- `EditorLayer` 拥有 `AssetPickerCatalog`、`AssetInspectorEditor`、`SceneEntityInspectorEditor` 和 `InspectorPanel`。
- `InspectorPanel` 只持有两个 Editor 的非拥有引用。
- Project 打开后，`EditorLayer` 绑定 ProjectContext 并重建 AssetPickerCatalog。
- Project 关闭时，先让 AssetInspectorEditor 解除项目绑定并清空工作副本，再清空 AssetPickerCatalog。
- `SceneEntityInspectorEditor` 通过 `EditorInteractionHost` 获取当前 SceneDocument，不缓存跨场景 Entity 实例。
- Selection Guard 必须由 AssetInspectorEditor 析构或解绑时移除，禁止留下捕获失效 `this` 的回调。

## 8. 目录结构

```text
Editor/src/
  Assets/
    AssetInspectorEditor.h
    AssetInspectorEditor.cpp
    AssetPickerCatalog.h
    AssetPickerCatalog.cpp
    AssetPickerModel.h
    AssetPickerModel.cpp
    Editors/
      SceneAssetEditor.h
      SceneAssetEditor.cpp
  Scene/
    SceneEntityInspectorEditor.h
    SceneEntityInspectorEditor.cpp
  Panels/
    InspectorPanel.h
    InspectorPanel.cpp
```

不新增通用 Inspector 插件目录，也不移动与本次职责拆分无关的文件。

## 9. 实施阶段

### P1：资产编辑器独立

- 建立 AssetPickerCatalog。
- 将无 UI 的 AssetPickerModel 从 Panels 移入 Assets。
- 建立 AssetInspectorEditor。
- 从 InspectorPanel 迁移完整资产工作副本生命周期。
- EditorLayer 改为直接访问 AssetInspectorEditor。
- 完成定向测试并提交。

### P2：场景实体编辑器独立

- 建立 SceneEntityInspectorEditor。
- 迁移实体选择、组件字段、上下文菜单和 Add/Remove Component UI。
- InspectorPanel 缩减为窗口与 Selection 路由。
- 完成定向测试并提交。

### P3：跨域接口清理

- 从通用 AssetEditorDrawContext 移除 Scene 字段。
- 为 SceneAssetEditor 注入专属 Scene 服务。
- 删除 InspectorPanel 的 Project、Workbench、Interaction、AssetRecord 和组件相关接口。
- 清理 EditorLayer 旧转发路径。
- 执行全量构建和全部 smoke 后提交。

## 10. 测试策略

- 扩展 `AssetEditingSmoke`，验证 AssetPickerCatalog 分类、资产 Dirty 状态判断和工作副本生命周期。
- 扩展 Editor Interaction 相关 smoke，验证组件修改仍进入命令路径并更新 Scene Dirty。
- 增加可独立测试的 Selection 路由判定，不对 ImGui 像素或布局细节编写脆弱测试。
- 每个 P 运行对应 smoke。
- P3 完成后运行 Debug `ALL_BUILD`、CMake 当前定义的全部 smoke，以及 `git diff --check`。

## 11. 验收条件

1. InspectorPanel 不再包含任何资产 Apply、Reimport、外部修改或 Dirty Modal 实现。
2. InspectorPanel 不再包含任何组件 Registry、Entity 解析或 Add Component 实现。
3. InspectorPanel 不再持有 ProjectContext、WorkbenchState、EditorInteractionHost 或 AssetRecord 列表。
4. EditorLayer 不再通过 InspectorPanel 查询或修改资产工作副本。
5. AssetEditorDrawContext 不再包含 Scene 专属字段。
6. SceneAssetEditor 仍能打开 `.scene` 资产并展示活动场景状态。
7. 实体组件编辑、Undo/Redo、Scene Dirty、资产 Apply 和未保存修改保护行为保持不变。
8. `InspectorPanel.cpp` 仅保留窗口与路由逻辑，不以转发方法隐藏领域依赖。
9. Debug `ALL_BUILD` 和全部 smoke 通过。

## 12. 风险控制

- Dirty Guard 迁移时必须先建立回归测试，避免切换 Selection 丢失资产修改。
- EditorLayer 成员析构顺序必须保证 InspectorPanel 不晚于其引用的两个 Editor 使用依赖。
- Project 关闭路径必须显式解绑 AssetInspectorEditor，避免跨项目保留 GUID 或工作副本。
- Scene Entity Editor 不缓存 Entity 引用，避免场景切换后访问失效 World。
- 本次只拆职责，不引入通用插件化抽象，以免扩大改动面。
