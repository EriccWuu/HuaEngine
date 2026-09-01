# Scene 资产创建与通用资产重命名 Spec

## 1. 背景

当前 Editor 顶部存在独立的 `Scene` 菜单，其中包含 `New Scene`、`Open Scene`、`Save Scene`、`Save Scene As` 和 `Validate Scene`。这些操作实际上都围绕 `.scene` 资产及其编辑文档展开，但当前实现仍由 `EditorLayer` 直接管理弹窗、路径、序列化和资产注册。

现有新场景首先以无路径的内存文档存在，首次保存时才写入 `.scene`、创建 `.meta` 并注册 GUID。这与已经建立的资产管理和资产 Inspector 体系不一致，也使 Scene 成为资产系统之外的特殊分支。

## 2. 目标

1. 删除 Editor 顶部 `Scene` 菜单。
2. Scene 通过 Project 面板的通用资产创建入口创建。
3. 点击 `Create > Scene` 后立即创建真实 `.scene` 资产，不出现创建弹窗。
4. 新资产使用默认名称，创建后选中并进入原地重命名，不自动打开。
5. 提供面向项目文件资产的通用 Rename 操作，Rename 后 GUID 保持不变。
6. 统一 `Ctrl+S` 为 `Save All Modified`，不再根据当前选择隐式决定保存对象。
7. 消除无路径 SceneDocument 和 `Save Scene As` 语义。
8. 为后续 Material、Shader 等原生资产创建保留可扩展注册结构。

## 3. 非目标

本阶段不实现以下能力：

- 资产删除、复制或移动目录。
- 文件夹重命名。
- Builtin 资产重命名。
- Material、Shader 等其他原生资产的具体创建器。
- 自动打开新建 Scene。
- 运行时 Scene 切换或 Game 窗口管理。
- 完整跨进程文件事务或崩溃恢复日志。

## 4. 用户交互

### 4.1 创建 Scene

Project 面板中的目录节点和 Assets 空白区域提供右键菜单：

```text
Create
  Scene
Reimport / Reimport All
```

行为如下：

1. 目录节点右键时，以该目录作为创建目标。
2. Assets 空白区域右键时，以当前浏览根目录 `Assets` 作为创建目标。
3. 点击 `Scene` 后立即创建 `New Scene.scene`。
4. 目标名称已存在时依次尝试 `New Scene 1.scene`、`New Scene 2.scene`。
5. 创建成功后刷新资产目录，按 GUID 选中新资产并进入原地重命名。
6. 创建操作不打开 Scene，也不改变当前 SceneDocument 或 dirty 状态。

### 4.2 原地重命名

文件资产右键菜单增加 `Rename`。新建资产成功后也直接进入相同的重命名状态。

- 编辑框只编辑文件名主体，扩展名不可修改。
- `Enter` 提交。
- 编辑框失焦提交。
- `Esc` 取消，资产保留原名称。
- 名称为空、包含路径分隔符、使用 `.`/`..`、Windows 保留名称或与同目录现有资产冲突时拒绝提交。
- 提交失败时保留编辑状态并输出诊断。
- 仅 `AssetSource::File` 且位于当前项目 `Assets` 下的文件资产可重命名。
- Builtin、缺失源文件和存在未应用 Asset Inspector 修改的资产不可重命名。

### 4.3 打开与保存

- 双击 Scene 资产仍通过工作台未保存修改检查后打开。
- 顶部删除整个 `Scene` 菜单。
- 顶部增加 `File > Save All`。
- `Ctrl+S` 绑定到 `editor.document.save_all`。
- `Save All` 先应用当前 Asset Inspector 修改，再保存 dirty SceneDocument。
- 任一保存失败时，对应对象继续保持 dirty，并写入 Console/WorkbenchState 诊断。
- Scene 验证由 Scene Asset Inspector 和统一 Validation 入口提供。

## 5. 架构

### 5.1 AssetCreationRegistry

新增编辑器侧 `AssetCreationRegistry`，用于声明 Project 面板可展示的资产创建项。

每个描述符至少包含：

```cpp
struct AssetCreationDescriptor {
    std::string TypeId;
    AssetKind Kind;
    std::string DisplayName;
    std::string DefaultBaseName;
    std::string Extension;
};
```

首个注册项为 Scene：

```text
TypeId: scene
Kind: Scene
DisplayName: Scene
DefaultBaseName: New Scene
Extension: .scene
```

Registry 只描述可创建类型，不持有 ProjectContext，不执行文件操作。

### 5.2 AssetWorkspaceController

新增编辑器侧 `AssetWorkspaceController`，集中处理资产工作区操作：

- 根据创建描述符和目标目录生成不冲突的默认路径。
- 调用 `ApplicationOperations` 创建资产。
- 调用通用 Rename 操作。
- 将成功结果转换为刷新、选择和开始重命名等编辑器意图。
- 将失败结果保留为 `ResultEnvelope` 供工作台展示。

ProjectPanel 只产生用户意图，不直接依赖 SceneService、AssetService 或 Application 单例。

### 5.3 ProjectPanel

`ProjectPanelAction` 增加：

```text
CreateAsset: TypeId + TargetDirectory
RenameAsset: AssetGuid + NewBaseName
```

ProjectPanel 维护纯 UI 状态：

- 被右键目录。
- 正在重命名的 GUID。
- 重命名输入缓冲区。
- 提交、取消和失焦状态。

创建类型列表由 `AssetCreationRegistry` 提供。Panel 不包含 `if Scene` 创建分支。

### 5.4 ApplicationOperations

新增：

```cpp
ResultEnvelope CreateSceneAsset(
    const ProjectContext& context,
    const std::filesystem::path& assetPath,
    AssetGuid* outGuid = nullptr) const;

ResultEnvelope RenameAsset(
    const ProjectContext& context,
    const AssetGuid& guid,
    std::string_view newBaseName,
    AssetRecord* outRecord = nullptr) const;
```

`CreateSceneAsset` 负责编排 Scene 创建、序列化和资产注册。`RenameAsset` 委托 AssetService 完成通用文件资产改名。

### 5.5 AssetService Rename

Rename 操作必须保持以下不变量：

- GUID 和运行时 AssetHandle 不变。
- 源文件和同名 `.meta` 一起移动。
- `AssetId`、RelativePath 和 AbsolutePath 更新。
- Manifest 与 AssetRegistry 使用新路径记录。
- Library artifact 继续按 GUID 定位，不移动、不重新导入。
- 使用 GUID 的依赖引用无需扫描或重写。

操作开始前完成全部校验。文件移动或记录更新失败时，尽最大可能恢复旧文件、旧 `.meta` 和旧内存记录，不暴露半更新成功结果。

### 5.6 SceneDocument 与会话同步

SceneDocument 不再支持无路径的新场景状态。成功加载到工作台的 Scene 必须对应一个现有 Scene 资产路径。

重命名当前打开 Scene 时，编辑器根据 Rename 成功结果同步：

- `SceneDocument.ScenePath`。
- `SceneDocument.DisplayName`。
- `ProjectSession.LastOpenedScenePath`。
- `PersistedEditorSession.LastScenePath`。
- 对应 `PersistedSceneCameraPose.ScenePath`。

SceneDocument 原 dirty 状态保持不变。后续保存只写入新路径，不重新创建旧文件。

新项目的初始 Scene 属于项目引导流程：先创建真实 Scene 资产，再明确打开。它不复用 Project 面板“创建但不打开”的 UI 行为。

## 6. 数据流

### 6.1 创建

```text
ProjectPanel Create > Scene
  -> ProjectPanelAction(CreateAsset)
  -> AssetWorkspaceController
  -> GenerateUniqueAssetPath
  -> ApplicationOperations::CreateSceneAsset
  -> SceneService::CreateScene
  -> SceneService/Serialization Save
  -> AssetService::RegisterSceneAsset
  -> refresh asset records
  -> select returned GUID
  -> ProjectPanel begin inline rename
```

### 6.2 Rename

```text
ProjectPanel inline rename submit
  -> ProjectPanelAction(RenameAsset)
  -> AssetWorkspaceController
  -> ApplicationOperations::RenameAsset
  -> AssetService preflight
  -> move source + meta
  -> update manifest + registry
  -> return old/new paths + GUID
  -> refresh asset records
  -> restore selection by GUID
  -> synchronize open document/session paths when applicable
```

### 6.3 Save All

```text
File > Save All / Ctrl+S
  -> editor.document.save_all
  -> apply dirty Asset Inspector session
  -> save dirty SceneDocument to its existing asset path
  -> preserve failures as dirty
  -> refresh validation and workbench summaries
```

## 7. 错误处理

- 创建目录不在 `Assets` 下：拒绝。
- 目标目录不存在：仅为合法的 Assets 子目录创建目录；失败则返回诊断。
- 创建目标重名：命名器选择下一个可用默认名称，不覆盖。
- Rename 目标重名：拒绝，不自动递增用户输入名称。
- Rename 修改扩展名：UI 不提供该能力，服务层也拒绝。
- Rename 的 Asset Inspector 会话 dirty：拒绝并提示先 Save All 或 Revert。
- Scene 创建完成但注册失败：删除本次创建的 Scene 与 meta，并恢复注册前状态。
- Rename 中途失败：回滚文件位置和内存记录；回滚失败时返回需要人工处理的详细诊断。

## 8. 移除项

从 EditorLayer 删除：

- 顶部 `Scene` 菜单。
- `editor.scene.new`。
- `editor.scene.open`。
- `editor.scene.save_as`。
- 旧 `editor.scene.save`。
- New Scene、Open Scene、Save Scene As 三个 Modal 及其输入缓冲和请求标记。
- 无路径 `CreateNewSceneDocument` 流程。

保留场景内容命令，例如 Entity 创建删除、Gizmo 模式和 Scene viewport 输入；这些不是 Scene 资产生命周期操作。

## 9. 测试与验收

### 9.1 AssetServiceSmoke

- Scene 资产创建后 `.scene`、`.meta`、Manifest、Registry 一致。
- Rename 后 GUID 与 Handle 不变。
- 旧路径消失，新路径可按 GUID 和 AssetId 解析。
- Library artifact 记录不因 Rename 改变。
- Builtin、越界、缺失源文件、非法名称和重名被拒绝。
- 注入失败时验证回滚结果。

### 9.2 ProjectPanelActionSmoke

- 目录与 Assets 空白区域产生正确 CreateAsset 请求。
- 默认名称按冲突递增。
- 创建成功后按 GUID 进入 Rename。
- Enter、失焦、Esc 和非法名称行为正确。

### 9.3 ProjectWorkbenchSmoke

- 打开中的 Scene Rename 后文档、会话和相机路径同步。
- dirty Scene Rename 后 Save All 只写新路径。
- 新建项目创建并打开真实初始 Scene 资产。

### 9.4 EditorInputSmoke

- `Ctrl+S` 只触发 `editor.document.save_all`。
- 旧 Scene 顶层资产生命周期命令不再注册。

### 9.5 完成标准

- Editor 顶部无 `Scene` 菜单。
- Project 面板可直接创建 Scene 并原地 Rename。
- 创建 Scene 不自动打开。
- SceneDocument 不存在无路径状态。
- Rename 保持 GUID 稳定且所有索引一致。
- 完整 Debug 构建通过。
- 当前全部 smoke 通过。

