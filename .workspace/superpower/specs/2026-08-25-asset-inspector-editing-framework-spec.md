# HuaEngine 资产 Inspector 编辑框架 Spec

状态：P1-P5 已实施

日期：2026-08-25

基线提交：`05f6e05 fix(editor): keep grid visible at distance`

## 1. 背景

HuaEngine 当前已经具备以下资产基础能力：

- 以 GUID 表达场景与材质中的资产引用。
- 使用 `.huaengine/assets.json` 维护项目资产 manifest。
- 使用 `{ProjectRoot}/Library` 保存导入后的二进制 Artifact 与依赖记录。
- 支持 OBJ Mesh、PNG Texture2D、Material 和 Shader 导入。
- 支持内容哈希、Importer/Artifact 版本、依赖 DAG、last-good Artifact 和 RuntimeCache 失效。
- Project Panel 支持浏览 Assets、Reimport 与 Reimport All。
- 实体 Inspector 支持组件字段、Mesh/Material/Texture 资产选择和材质参数 override。

但当前编辑器选择状态仅表达 ECS Entity。Project Panel 点击普通资产不会建立资产选择，Inspector 也没有资产编辑会话，因此存在以下缺口：

1. Native Material 只能通过外部文本编辑器修改 `.material`。
2. OBJ/PNG 没有可持久化、可版本控制的 Import Settings。
3. Shader 的编译状态、反射接口和 diagnostics 无法在 Inspector 查看。
4. Project Panel、Inspector、AssetService 与 Importer 之间缺少稳定的资产编辑边界。
5. 若直接在 `InspectorPanel.cpp` 中按扩展名堆叠 UI、文件 IO 和重导入逻辑，会形成难以扩展到 FBX、更多 Texture 格式和新 Native Asset 的集中式分支。

本 Spec 建立一套适合 HuaEngine 当前规模的类型化资产编辑框架。目标是保持设计现代、边界清晰、可扩展，但不提前实现商业引擎级的完整 Asset Database、批量编辑和复杂预览系统。

## 2. 已确认决策

本设计已确认以下产品与架构决策：

1. 按资产类别区分编辑语义：
   - Native Material 编辑源资产内容。
   - Native Scene 单击时作为资产进入 Inspector，首期展示只读源文件摘要；只有双击才打开场景文档。
   - OBJ/PNG 编辑 Import Settings，不编辑 Mesh 顶点或 PNG 像素。
   - Shader 首期只读展示编译状态、接口和 diagnostics，源码交给外部编辑器。
2. 所有编辑都使用显式 `Apply / Revert`，拖动字段时不写磁盘、不触发重复导入。
3. 所有文件资产使用同目录 `.meta` sidecar；`.meta` 是 GUID、Importer 身份和 Import Settings 的权威来源。
4. 增加编辑器级统一选择状态，Entity 与 Asset 选择互斥。
5. 采用类型化 `AssetEditorRegistry + IAssetEditor`，不在 Inspector 中使用集中式 `switch`，首期也不建设完全反射驱动的资产编辑系统。
6. 资产编辑会话使用 working copy、dirty 状态和 optimistic concurrency hash。
7. 作者文件保存成功但导入失败时，保留作者修改和 last-good Artifact，并显示结构化失败状态。
8. 首期资产 Apply 不进入场景 Undo 栈。
9. Project Panel 中所有已注册文件的单击语义统一为资产选择，包括 `.scene`；打开场景是 `.scene` 独有的双击语义。
10. Editor 与资产核心层之间使用稳定的只读 `AssetInspectionSnapshot`，Editor 不直接拼接 Manifest、Library 和 ImporterRegistry 数据。
11. 导入失败 diagnostics 需要持久化，重启 Editor 后仍能正确表达 `LastGoodWithFailure`。

## 3. 目标与非目标

### 3.1 必须完成

- Project Panel 点击已注册资产时建立 AssetSelection，并让 Inspector 切换到资产模式。
- `.scene` 必须注册为 `AssetKind::Scene`，使用稳定 ImporterId `scene.native`；单击只选择，双击才打开。
- EntitySelection 与 AssetSelection 具有唯一权威状态且互斥。
- Inspector 通过 Registry 分发到对应的类型化资产编辑器。
- AssetEditSession 支持 working copy、dirty、Apply、Revert、外部修改冲突和 diagnostics。
- 所有文件资产生成并消费 `.meta` sidecar。
- 迁移已有 `.huaengine/assets.json` GUID 时不破坏现有场景和材质引用。
- Importer 真正消费 Import Settings，并把规范化 settings 纳入 Import Fingerprint。
- Material Inspector 能编辑 Shader、参数默认值和 Texture 引用。
- OBJ/PNG Inspector 能编辑首期 Import Settings 并展示 Artifact 统计。
- Shader Inspector 能展示编译健康、stages、反射资源和 diagnostics。
- Scene Inspector 首期能展示 GUID、路径、源文件状态和可安全读取的场景摘要，不因单击而替换当前活动场景。
- Apply 后按依赖 DAG 重导入，并在成功发布新 Artifact 后使相关 RuntimeCache 失效。
- 导入失败时继续提供 last-good Artifact。
- 未保存资产切换、场景切换、工程关闭时提供 `Apply / Discard / Cancel`。

### 3.2 明确非目标

- 不在首期实现 FBX 导入；框架必须允许未来按 ImporterId 接入 FBX。
- 不在首期实现多资产批量 Import Settings 编辑。
- 不在首期实现 Mesh 顶点、拓扑或 Texture 像素编辑。
- 不在首期实现引擎内 Shader 文本编辑器。
- 不在首期实现完整 Asset Undo History；场景 Undo 栈不记录资产 Apply。
- 不在首期实现 Sampler Asset。Texture Filter/Wrap 不错误塞入 Texture Import Settings。
- 不在首期实现文件变化监听。外部修改只在 Apply、Reload 或 Project Refresh 时检测。
- 不在首期实现缩略图数据库和复杂 3D Preview Scene；允许 Material 使用轻量独立预览。
- 不把 Library 或 RuntimeCache 变成作者数据权威来源。

## 4. 总体架构

```text
ProjectPanel / HierarchyPanel / SceneViewport
                    |
                    v
          EditorSelectionService
                    |
                    v
              InspectorPanel
             /              \
            v                v
 Entity Inspector      AssetInspectorHost
                              |
                              v
                    AssetEditorRegistry
                   /      |       |      \
                  v       v       v       v
             Material    OBJ     PNG    Shader
              Editor    Editor  Editor  Editor
                   \      |       |      /
                    v     v       v     v
                     AssetEditSession
                              |
                         BuildCommit
                              |
                              v
                   AssetAuthoringService
                              |
             atomic source/meta write
                              |
                              v
                 AssetService / Import DAG
                              |
                    Library + RuntimeCache
```

### 4.1 EditorSelectionService

统一选择状态建议表达为：

```cpp
struct NoEditorSelection {};

struct EntitySelection {
    std::vector<EntityUuid> Entities;
};

struct AssetSelection {
    AssetGuid Guid;
};

using EditorSelection = std::variant<
    NoEditorSelection,
    EntitySelection,
    AssetSelection>;
```

约束：

- 同一时刻只能存在一种选择类型。
- 点击 Project 资产会清除 EntitySelection；Gizmo 随之隐藏。
- 点击 Hierarchy 或 Scene 物体会清除 AssetSelection。
- SceneViewport picking 只产生 EntitySelection。
- 首期 AssetSelection 只允许单选。
- 现有静态 `Selection` 在迁移期只能作为 EntitySelection 的兼容 facade，其全部读写必须委托给同一个 `EditorSelectionService`；原有 `m_SelectedEntityUuids` 等独立容器必须移除，不得保留双权威状态。
- 兼容 facade 在 Editor 外的 smoke 中必须有明确可用的默认 service 或显式绑定机制，不能因未初始化而静默丢失选择操作。
- 最终 Inspector、Hierarchy、SceneViewport、命令路由和快捷键都从统一服务读取选择。

### 4.2 InspectorPanel

`InspectorPanel` 只负责：

- 绘制 Project/Scene/Validation 通用头部。
- 根据 `EditorSelection` 路由实体或资产 Inspector。
- 持有或引用 `AssetInspectorHost`。
- 将 `Ctrl+S` 等高层命令路由到当前选择对应的保存行为。

`InspectorPanel` 不负责：

- 解析 Material YAML 或 `.meta`。
- 调用 `ofstream` 写作者文件。
- 根据扩展名构造 Import Settings。
- 直接使 RuntimeCache 失效。
- 实现 Material、OBJ、PNG、Shader 的具体字段 UI。

### 4.3 AssetInspectorHost

Host 管理当前资产编辑生命周期：

- 根据 AssetSelection 打开或复用 AssetEditSession。
- 通过 AssetEditorRegistry 创建正确的 IAssetEditor。
- 绘制公共资产头部、dirty 状态、Import Health、diagnostics 和 Apply/Revert/Reimport 命令。
- 切换选择前执行未保存检查。
- 检测外部修改并提供 Reload。
- 将编辑器生成的 AssetEditCommit 提交给 AssetAuthoringService。
- 把 ResultEnvelope 记录到 EditorWorkbenchState/Console。

Host 不理解具体资产字段，也不通过 `dynamic_cast` 判断 Material/OBJ/PNG。

### 4.4 AssetEditorRegistry

Registry 使用 `AssetKind + ImporterId` 进行匹配：

```cpp
struct AssetEditorDescriptor {
    AssetKind Kind;
    std::string ImporterId;
    std::function<std::unique_ptr<IAssetEditor>()> Factory;
};
```

匹配规则：

1. Native Material、Shader 等可按 Kind 和具体 ImporterId 注册。
2. Mesh 必须按 `mesh.obj`、未来 `mesh.fbx` 等 ImporterId 区分。
3. Texture 必须按 `texture.png` 等 ImporterId 区分。
4. 找不到专用 Editor 时使用只读 GenericAssetInspector，不能让 Inspector 崩溃或显示空白。
5. 重复注册同一 key 必须失败并输出稳定 diagnostic。

### 4.5 IAssetEditor

建议接口职责如下：

```cpp
class IAssetEditor {
public:
    virtual ~IAssetEditor() = default;

    virtual ResultEnvelope Open(const AssetEditorOpenContext&) = 0;
    virtual void Draw(AssetEditorDrawContext&) = 0;
    virtual ResultEnvelope Validate() const = 0;
    virtual AssetEditCommit BuildCommit() const = 0;
    virtual bool IsDirty() const = 0;
    virtual void Revert() = 0;
};
```

约束：

- `Open()` 只读取形成 working copy 所需的数据。
- `Draw()` 只能修改 working copy 和 preview，不写磁盘。
- `Validate()` 返回结构化 diagnostics。
- `BuildCommit()` 生成不含 ImGui 类型的提交描述。
- `Revert()` 恢复 session 打开或最近 Apply 后的基准快照。
- IAssetEditor 位于 Editor 模块；Asset/RHI/Rendering 核心模块不得依赖 ImGui。

### 4.6 AssetInspectionSnapshot

Editor 不直接读取或组合 `AssetManifest`、`AssetLibrary`、`AssetImporterRegistry` 和 RuntimeCache 内部状态。资产核心层提供稳定的只读查询模型：

```cpp
struct AssetInspectionSnapshot {
    AssetRecord Asset;
    std::string ImporterId;
    uint32_t ImporterVersion = 0;
    uint32_t SettingsVersion = 0;
    AssetImportHealth Health;
    std::string ImportFingerprint;
    std::filesystem::path ArtifactRelativePath;
    std::vector<AssetGuid> Dependencies;
    std::vector<AssetGuid> Dependents;
    std::vector<DiagnosticEntry> Diagnostics;
};
```

由 `AssetService::InspectAsset()` 聚合并通过 `ApplicationOperations::InspectAsset()` 暴露。不存在可导入 Artifact 的 Native Scene 使用明确的 `NotApplicable` Artifact 状态，不能伪装成 `Missing` 或导入失败。

P1 即需要让 `.scene` 拥有 GUID 才能建立 `AssetSelection`。在 P2 sidecar 迁移落地前，Native Scene 先通过专用 source registration 能力进入现有 manifest；已有 manifest GUID 必须复用，新场景只生成一次 GUID。P2 创建 `.scene.meta` 时沿用该 GUID，不得二次生成。Native Scene registration 与 Artifact importer 分离，不能为了复用 Reimport 执行流而生成无意义的 Scene Artifact。

### 4.7 Project Panel 资产目录索引

Project Panel 从 EditorLayer 提供的 `AssetRecord` catalog 建立 normalized relative path 到 GUID 的索引。文件点击只查该内存索引，不临时扫描磁盘、不直接读取 manifest，也不调用 AssetService。

- 已注册文件单击产生 `AssetSelection(Guid)`。
- `.scene` 单击与其他资产一致，只产生 AssetSelection；双击产生独立的 `OpenScene` action。
- 未注册文件不进入 Asset Inspector，显示稳定的未注册状态。
- `.meta` 是 source 的附属文件，不作为独立条目选择。

## 5. 数据权威与目录契约

### 5.1 权威关系

```text
Assets source file     作者数据
Assets source.meta     GUID、Importer、Import Settings
.huaengine/assets.json 派生项目索引
Library                可再生 Artifact、依赖和 fingerprint
RuntimeCache           可再生运行时对象
```

具体规则：

- 文件资产的 GUID 以 `.meta` 为权威。
- Native Material 的实际参数仍以 `.material` 为权威；`.material.meta` 只保存身份和 importer metadata。
- Native Scene 的实体和组件内容仍以 `.scene` 为权威；`.scene.meta` 保存 GUID、`scene.native` importer 身份和空 settings。Scene 首期不生成 Library Artifact。
- OBJ/PNG 的 DCC/图片内容以源文件为权威，导入策略以 `.meta.settings` 为权威。
- `.huaengine/assets.json` 可用于快速启动和诊断，但必须能由 Assets 扫描与 `.meta` 重建。
- Library 可以完整删除，重新启动或 Reimport All 后必须恢复等价 Artifact。
- Builtin assets 继续由引擎 BuiltinAssetCatalog 提供 GUID 和 importer metadata，不在项目 Assets 下生成副本 `.meta`。

### 5.2 `.meta` 文件格式

首版使用 YAML sidecar，示例：

```yaml
meta_version: 1
guid: 7c8ee70eb6cd4e289d513ef54cd02784
importer: texture.png
settings_version: 1
settings:
  color_space: srgb
  generate_mipmaps: true
  alpha_mode: preserve
  max_size: 4096
  compression: none
```

约束：

- sidecar 名固定为 `<source filename>.meta`，例如 `Cube.obj.meta`。
- 不在 `.meta` 重复保存可从路径/Importer 推导的 AssetKind、文件名和相对路径。
- `meta_version` 管公共 envelope；`settings_version` 由具体 Importer 管理。
- `settings` 必须以稳定字段名和规范化顺序序列化，以获得稳定 diff 与 hash。
- Native asset 没有设置时仍保留空 `settings: {}`，保持统一 envelope。
- 未知公共版本、未来更高 settings version、空 GUID、未知 Importer 或非法字段必须输出 diagnostic，不静默使用默认值覆盖。
- Importer 可迁移旧 settings version；迁移只更新 working copy，用户 Apply 后才写回新版本。

### 5.3 现有项目迁移

首次加载旧项目时按以下顺序迁移：

1. 扫描 Assets 下 Importer 支持的源文件，忽略 `.meta` 本身。
2. 若 sidecar 存在，验证并使用其中 GUID。
3. 若 sidecar 不存在，但旧 `assets.json` 中存在同一 normalized relative path，则沿用旧 GUID 并创建默认 `.meta`。
4. 若 sidecar 和旧 manifest 均不存在，则生成新 GUID 和默认 settings。
5. 若 sidecar GUID 与旧 manifest 同路径 GUID 不同，停止该资产迁移并报告冲突，不自动选择任一方。
6. 若多个 sidecar 使用同一 GUID，相关资产均标记为冲突，不生成新 GUID 掩盖问题。
7. 成功扫描后重新生成派生 `assets.json`。
8. 迁移必须使用临时文件和原子 rename，任一 sidecar 写入失败时不能发布半份新 manifest。

验收要求：已有 `.scene`、`.material` 和其他 GUID 引用在迁移后保持不变。

## 6. Import Settings 契约

### 6.1 Importer 扩展

Importer 需要新增以下概念能力，具体 C++ 可采用 type-erased settings handle，避免中央 variant 每增加 Importer 都修改：

```cpp
class AssetImporter {
public:
    virtual std::unique_ptr<AssetImportSettings> CreateDefaultSettings() const = 0;
    virtual ResultEnvelope DecodeSettings(
        const AssetMetaSettingsNode& source,
        std::unique_ptr<AssetImportSettings>& output) const = 0;
    virtual ResultEnvelope EncodeSettings(
        const AssetImportSettings& settings,
        AssetMetaSettingsNode& output) const = 0;
    virtual ResultEnvelope ValidateSettings(
        const AssetImportSettings& settings) const = 0;
};
```

Import 阶段通过 `AssetImportContext` 获得已经验证、不可变的 settings。Importer 不在 `Import()` 内重新猜测默认值。

首期不要求通用反射自动生成 settings UI。OBJ/PNG Editor 可以使用类型化 working settings；ImporterId 是 Editor 与 Importer 共享的稳定选择 key。

### 6.2 Fingerprint

Import Fingerprint 必须包含：

- ImporterId。
- ImporterVersion。
- ArtifactVersion。
- settings schema version。
- canonical settings digest。
- 根源文件内容 hash。
- 传递源文件和依赖 Artifact digest。

只改变 Inspector 展示状态、折叠状态或 preview camera 不得影响 fingerprint。

相同源文件与相同规范化 settings 必须生成相同 fingerprint；settings 字段序列化顺序不同不能导致重复导入。

### 6.3 Texture 与 Sampler 边界

PNG Import Settings 首期包含：

- `color_space`: `srgb | linear`
- `generate_mipmaps`: bool
- `alpha_mode`: `preserve | opaque`
- `max_size`: 有上限的正整数
- `compression`: 首期仅 `none`，保留未来枚举语义

Texture filter、address/wrap mode、anisotropy 不属于 Texture Import Settings。这些是 Sampler 状态，继续由当前 canonical sampler policy 提供；未来通过 Sampler Asset 或 Material sampler 配置扩展。

若某个设置对应的底层 RHI/Artifact 能力尚未实现，该设置不能伪装成功：必须暂时禁用并显示原因，或与底层能力在同一 P 内落地。P4 不默认承担新的纹理后端能力；只有已经获得 Artifact 与运行时支持的设置才允许启用。

### 6.4 OBJ Mesh Settings

首期包含：

- `import_scale`: 大于零的 float
- `up_axis`: `x | y | z`
- `forward_axis`: 带正负方向的 axis enum，不能与 up axis 共线
- `flip_uv_v`: bool
- `generate_normals_when_missing`: bool
- `recalculate_normals`: bool
- `reverse_winding`: bool

Importer 必须定义 HuaEngine 目标坐标系，并对 position、normal、tangent 和 winding 执行一致转换。禁止只转换 position 导致光照或剔除错误。

## 7. AssetEditSession 与提交模型

建议会话状态：

```cpp
struct AssetEditSessionState {
    AssetGuid Guid;
    AssetKind Kind;
    std::filesystem::path SourcePath;
    std::string ImporterId;
    std::string OriginalSourceHash;
    std::string OriginalMetaHash;
    AssetImportHealth Health;
    bool Dirty;
    bool ExternallyModified;
    std::vector<DiagnosticEntry> Diagnostics;
};
```

working document 由具体 IAssetEditor 类型化持有，不塞进全局 `std::variant<Material, Obj, Png, ...>`。

### 7.1 AssetEditCommit

```cpp
enum class AssetEditTarget {
    Source,
    Metadata
};

struct AssetEditCommit {
    AssetGuid Guid;
    AssetEditTarget Target;
    std::string ExpectedSourceHash;
    std::string ExpectedMetaHash;
    std::vector<uint8_t> SerializedContent;
};
```

提交内容不得包含 ImGui、GPU Resource 或 Runtime Material 指针。

### 7.2 Apply 两阶段语义

Apply 不是跨作者文件与 Artifact 的单一原子事务，而是两个明确阶段：

#### 阶段 A：保存作者数据

1. 验证当前项目、GUID、AssetKind、ImporterId 和目标路径。
2. 重新计算 source/meta hash。
3. hash 与 session expected hash 不一致时返回 `Conflict`，不覆盖外部修改。
4. IAssetEditor Validate 必须成功。
5. 内容写入同目录临时文件，flush 后原子替换目标源文件或 `.meta`。
6. 写入成功后更新 session 基准 hash。

原子替换必须封装为跨平台文件写服务。Windows 实现不能假设普通 `std::filesystem::rename(temp, target)` 能覆盖已有目标，必须使用平台支持的替换语义并测试替换失败不破坏旧文件。

#### 阶段 B：导入与发布

1. 以 GUID 强制导入目标资产。
2. 按 AssetLibrary dependency DAG 收集并拓扑排序 dependents。
3. 每个新 Artifact 先写临时文件并校验 decode，再原子替换 last-good Artifact。
4. 成功发布 Artifact 后更新 Library catalog/fingerprint。
5. 仅对成功发布的新 Artifact 及受影响 dependents 失效 RuntimeCache。
6. 返回完整 ImportReport 和 diagnostics。

结果状态至少区分：

- `Applied`：作者数据保存且导入成功。
- `NoChanges`：working copy 与基准一致。
- `Conflict`：外部修改，未保存。
- `ValidationFailed`：working copy 非法，未保存。
- `SaveFailed`：作者文件保存失败，未导入。
- `SavedButImportFailed`：作者文件已保存，last-good Artifact 继续生效。

发生 `SavedButImportFailed` 时 session 的作者数据 dirty 状态清除，但 Import Health 显示 `LastGoodWithFailure` 或 `Missing`。不得谎报 Apply 完全成功。

### 7.3 Revert 与 Reload

- `Revert` 放弃 working copy，恢复最近一次成功保存的磁盘内容。
- 检测到外部修改时显示 `Reload / Cancel`；Reload 读取外部内容并更新基准 hash。
- 首期不提供三方 merge。
- Reimport 不应覆盖 dirty working copy；用户必须先 Apply、Discard 或 Cancel。

## 8. 未保存状态与命令路由

以下操作在存在 dirty AssetEditSession 时必须进入统一确认流程：

- 选择另一个资产。
- 选择 Entity。
- 打开或新建 Scene。
- 关闭当前 Project。
- 退出 Editor。

确认选项：

- `Apply`：成功或 SavedButImportFailed 后继续原操作；Validation/Save/Conflict 失败时留在当前选择。
- `Discard`：Revert 后继续原操作。
- `Cancel`：取消原操作，保持当前资产选择。

快捷键：

- AssetSelection 下 `Ctrl+S` 调用当前资产 Apply。
- EntitySelection 下 `Ctrl+S` 继续保存 Scene。
- `Ctrl+Z/Ctrl+Y` 首期只操作 Scene command stack，不撤销已 Apply 的资产文件。

## 9. 各资产编辑器

### 9.1 通用资产头部

显示：

- 文件名和 AssetKind。
- GUID。
- source relative path。
- ImporterId 和 settings version。
- Import Health。
- Artifact 当前/陈旧/缺失/last-good 状态。
- 直接依赖和 dependent 数量。
- diagnostics 摘要与详情。
- Apply、Revert、Reimport、Reload。

GUID 和路径默认只读。重命名/移动应由未来 Asset Browser 文件操作负责，不能通过文本框修改 manifest。

### 9.2 MaterialAssetEditor

编辑 `.material` 的 `MaterialSourceData`：

- name。
- Shader 资产引用，复用现有 Asset Picker。
- 由 Shader authoring interface 驱动的 Material 参数默认值。
- Texture 参数复用 Texture Asset Picker。
- Color、Range、枚举和只读信息使用 Shader authoring metadata。

行为：

- 切换 Shader 时针对 working copy 执行参数 reconcile。
- 丢弃旧 Shader 不再定义的参数，并在 Apply 前展示清理摘要。
- 缺少必需参数、类型错误、Texture Kind 错误或 Shader 不健康时 Validate 失败。
- 增加 `SaveMaterialSourceData()`，输出稳定、可读的 canonical YAML；不能继续复用只面向旧 Runtime Material 的序列化路径。
- Material working copy 预览只进入独立 AssetPreviewContext；场景主路径只在 Apply/import 成功后更新。

### 9.3 ObjMeshImportEditor

编辑 `.obj.meta` 的类型化 settings，并显示只读 Artifact 统计：

- vertex/index count。
- AABB。
- 是否包含 UV、normal、tangent。
- 当前 Artifact/fingerprint 状态。

设置合法性在 UI 和 Importer 两侧都要校验，Importer 是最终权威。

### 9.4 PngTextureImportEditor

编辑 `.png.meta` 的类型化 settings，并显示：

- source width/height/channel count。
- Artifact width/height/format。
- mip count。
- alpha presence。
- 当前 Artifact/fingerprint 状态。

Preview 使用 working source 和当前 settings 的轻量结果，但首期允许只在 Apply 后更新完整 mip/resize 结果。

### 9.5 ShaderAssetEditor

首期只读显示：

- source descriptor 和 HLSL 文件。
- vertex/fragment entry point。
- SPIR-V/generated GLSL/DXIL payload availability。
- 编译 diagnostics。
- Shader stages。
- Constant buffers、members、offset 和 size。
- Texture/Sampler resources 的 set/binding/visibility。
- Material exposed parameters、类型、默认值和范围。
- interface digest/signature。

命令：

- `Open Source`：通过平台 HostLaunch 打开外部编辑器。
- `Reimport`。
- `Copy Diagnostic`。

ShaderAssetEditor 不直接修改 HLSL 或 `.shader`。

### 9.6 SceneAssetEditor

首期只读展示：

- scene GUID、source relative path 和文件状态。
- 场景名称、序列化格式版本和可安全读取的实体数量摘要。
- 当前是否为活动场景、活动场景是否 dirty。
- 外部文件诊断。

单击 `.scene` 只打开 SceneAssetEditor，不加载或替换当前 SceneDocument。`Open Scene` 作为显式按钮提供，与 Project Panel 双击使用同一高层 action。SceneAssetEditor 首期没有 Apply/Revert/Reimport，也不生成 Library Artifact；未来增加 Native Scene authoring 字段时仍通过 AssetEditSession 和 AssetAuthoringService 接入。

### 9.7 GenericAssetInspector

未注册专用 Editor、Builtin 或不支持编辑的资产使用只读 fallback，显示通用头部和 diagnostics。Builtin 首期只读，禁止通过 Project Inspector 修改引擎安装目录。

## 10. Runtime 刷新与依赖传播

当前 RuntimeCache 按 GUID 缓存 Mesh、Material、Texture 和 Shader。资产 Apply 后必须遵守：

1. 新 Artifact 成功发布前不使旧 cache 失效，保证 last-good 仍可使用。
2. 发布成功后失效目标 GUID。
3. Shader 变化导致 Material dependent 重导入成功后，同时失效 Shader 与对应 Material GUID。
4. Material 变化只失效 Material；实体仍通过 GUID 引用，下一帧重新 Resolve。
5. Mesh/Texture 变化失效对应 runtime resource。
6. RenderResourceResolver 不长期持有越过 cache generation 的裸引用；若存在跨帧缓存，需要增加 generation/version 校验。
7. Runtime refresh 失败必须输出 diagnostic，不能让作者 Apply 结果显示为完全成功。
8. 最近一次导入失败 diagnostics 与失败对应的 source/settings fingerprint 必须持久化到 Library catalog 或等价可再生状态；重启 Editor 后仍能展示 `LastGoodWithFailure`，成功导入后再清除。

## 11. UI 行为

- Project Panel 单击任意已注册文件都只建立 AssetSelection，`.scene` 与 Material/OBJ/PNG/Shader 行为一致。
- Project Panel 双击 `.scene` 才打开场景；Scene Inspector 的 `Open Scene` 按钮走同一 action。单击 `.scene` 绝不替换当前 SceneDocument。
- 当前选择使用稳定高亮，不能只根据 `m_CurrentScenePath` 判断。
- 双击 Material/OBJ/PNG 首期仍只选择；Shader 可双击 Open Source，但单击只选择；只有 `.scene` 双击具有打开场景的强制语义。
- Inspector 资产字段采用与组件 Inspector 一致的 label/value 两列布局。
- Apply/Revert 固定在资产 Inspector 头部，布局稳定，不因 diagnostics 数量移动到不可预测位置。
- Import Health、外部冲突和 last-good 状态使用清晰文本和颜色，但不能只依赖颜色表达。
- diagnostics 可折叠，默认展示错误摘要。
- 不使用嵌套卡片；通用头部、编辑字段、Artifact 信息和 diagnostics 使用普通分段。
- 长路径、GUID 和错误文本必须换行或裁剪并提供 tooltip，不能撑破 Inspector。

## 12. 建议文件组织

```text
Editor/src/Selection/
  EditorSelectionService.h/.cpp
  EditorSelectionTypes.h

Editor/src/Assets/
  AssetInspectorHost.h/.cpp
  AssetEditorRegistry.h/.cpp
  AssetEditSession.h/.cpp
  AssetPreviewContext.h/.cpp
  Editors/
    GenericAssetInspector.h/.cpp
    MaterialAssetEditor.h/.cpp
    ObjMeshImportEditor.h/.cpp
    PngTextureImportEditor.h/.cpp
    ShaderAssetEditor.h/.cpp
    SceneAssetEditor.h/.cpp

HuaEngine/src/HuaEngine/Asset/Metadata/
  AssetMeta.h/.cpp
  AssetMetaMigration.h/.cpp
  AssetImportSettings.h

HuaEngine/src/HuaEngine/Asset/Authoring/
  AssetAuthoringService.h/.cpp
  AssetEditCommit.h
```

可根据现有 CMake glob 和模块习惯微调目录，但职责边界不得退化为 InspectorPanel 集中实现。

## 13. 分阶段实施

每完成一个 P 必须保持可构建、可测试。P1、P2 进一步拆成下面列出的独立小提交，避免把选择迁移、UI 路由和资产身份迁移耦合在一个超大提交中。

### P1：统一选择与只读资产 Inspector

内容：

- P1.1：EditorSelectionService、EditorSelection variant、Selection compatibility facade 和纯模型测试。
- P1.2：`AssetInspectionSnapshot`、AssetService/ApplicationOperations 查询边界和查询测试。
- P1.3：AssetInspectorHost、Registry、Session 骨架和 GenericAssetInspector。
- P1.4：Project Panel catalog 索引与资产选择 action、Inspector Entity/Asset 路由。
- P1.5：Scene/Shader/Material/Mesh/Texture 通用只读信息和 Import Health。
- 增加 `AssetKind::Scene` 和 `scene.native` 身份；Scene Artifact 状态为 `NotApplicable`。

验收：

- 点击资产后 Inspector 不再显示 `No entity selected`。
- 单击 `.scene` 只显示 Scene Asset Inspector，双击才打开场景。
- 点击 Entity 后资产 Inspector 关闭，Gizmo 恢复。
- Scene picking、Hierarchy 和 Project selection 互斥且无双权威。
- 未注册专用 Editor 时稳定 fallback。

### P2：`.meta` 与 Import Settings 基础设施

内容：

- P2.1：AssetMeta envelope、canonical codec、校验和原子文件写工具。
- P2.2：所有文件资产 sidecar 扫描、旧 manifest GUID 安全迁移和 assets.json 派生重建。
- P2.3：Importer settings create/decode/encode/validate 与类型身份校验。
- P2.4：settings digest 进入 fingerprint、optimistic concurrency hash 和冲突测试。

验收：

- 旧 TestProj 迁移后 GUID 引用不变。
- 删除 Library 后可完整重建。
- 修改 settings 会重导入；仅 YAML key 顺序变化不会改变 fingerprint。
- duplicate GUID、未知 version 和外部修改稳定失败。

### P3：Material 编辑

内容：

- MaterialAssetEditor。
- canonical `SaveMaterialSourceData()`。
- Shader/Texture Asset Picker。
- Shader authoring metadata 驱动参数 UI。
- Apply/Revert、dirty guard 和 Ctrl+S 路由。
- AssetAuthoringService Source commit。
- Material dependent reimport 与 RuntimeCache refresh。

验收：

- Inspector 修改 Material 参数并 Apply 后源 YAML、Artifact 和场景像素一致更新。
- Revert 不写文件。
- Shader 切换清理不兼容参数并展示摘要。
- 外部文件冲突不被覆盖。
- 导入失败保留 last-good。

### P4：OBJ/PNG Import Settings

内容：

- ObjMeshImportSettings 与 PngTextureImportSettings。
- 对应类型化 Editor。
- Importer 消费 settings。
- Artifact 统计查询。
- Metadata commit 与重导入。

验收：

- scale/axis/winding/normal/UV 设置实际改变 Mesh Artifact。
- 所有已启用的 color space/mipmap/alpha/max size 设置都实际改变 Texture Artifact。
- 尚无 Artifact 或运行时能力支撑的 Texture 设置保持禁用，并在 UI 中展示明确原因。
- 不支持的底层设置不会伪装生效。
- Filter/Wrap 不出现在 Texture Inspector。

### P5：Shader 只读 Inspector 与收尾

内容：

- ShaderAssetEditor。
- Shader interface/diagnostic 查询模型。
- Open Source、Reimport、Copy Diagnostic。
- 统一 dirty modal 覆盖场景切换、工程关闭和退出。
- Asset editing smoke 与文档更新。

验收：

- Shader 编译失败能在 Inspector 定位到 stage/source diagnostic。
- 反射资源与 Artifact 内容一致。
- 所有离开 dirty session 的入口都经过统一确认。
- P1-P5 相关 smoke 全部通过。

## 14. 测试策略

### 14.1 纯模型测试

- EditorSelection 状态转换和互斥。
- AssetEditorRegistry 注册、查找、重复 key 和 fallback。
- AssetEditSession dirty、Revert、hash conflict。
- 未保存确认状态机的 Apply/Discard/Cancel。

### 14.2 AssetMeta 测试

- canonical round-trip。
- 默认 settings。
- settings version migration。
- unknown future version rejection。
- duplicate GUID detection。
- sidecar/path 逃逸防护。
- 临时写失败不破坏原文件。

### 14.3 导入测试

- settings digest 参与 fingerprint。
- 相同规范化 settings skip。
- OBJ 每项设置对 Artifact 的可观察影响。
- PNG 每项已支持设置对 Artifact 的可观察影响。
- dependent DAG 顺序。
- failed import 保留 last-good Artifact 与 catalog。
- 成功发布后 RuntimeCache 失效。

### 14.4 Material 测试

- load/edit/save canonical YAML。
- Shader/Texture GUID kind 校验。
- Shader interface reconcile。
- range/default/type validation。
- Apply 后 MaterialDefinition 与运行时 Material 一致。

### 14.5 Editor smoke

- Project click -> AssetSelection -> 正确 Inspector。
- Scene 单击 -> Scene Asset Inspector，且当前 SceneDocument 不变；Scene 双击 -> 打开目标场景。
- Asset click -> Entity click -> Gizmo 与 Inspector 切换。
- dirty session 切换弹窗。
- Apply/Revert/Reimport 命令路由。
- Shader diagnostics 和 Artifact 统计可见。

测试不得依赖仓库 `Resources` 中的临时文件；需要的资产 fixture 放在 Tests 下或测试临时目录生成。

## 15. 失败与诊断契约

建议稳定 diagnostic code：

- `asset.meta.missing`
- `asset.meta.invalid`
- `asset.meta.version_unsupported`
- `asset.meta.guid_conflict`
- `asset.meta.importer_mismatch`
- `asset.edit.validation_failed`
- `asset.edit.external_conflict`
- `asset.edit.save_failed`
- `asset.edit.saved_import_failed`
- `asset.edit.editor_unavailable`
- `asset.edit.runtime_refresh_failed`

所有失败通过 ResultEnvelope 返回，并至少包含 GUID、source path、ImporterId 和可执行建议。UI 不根据英文 Summary 文本解析状态。

## 16. 风险与控制

### 16.1 `.meta` 引入造成 GUID churn

控制：先读取旧 manifest，按 normalized relative path 迁移旧 GUID；冲突时停止而非重新生成。

### 16.2 选择系统重构影响 Gizmo/Picking

控制：P1 先建立单一服务和兼容 facade，增加 Entity/Asset 往返 smoke，禁止维护两套实体 UUID 列表。

### 16.3 Apply 与导入失败状态混淆

控制：明确 `SavedButImportFailed`，作者文件 dirty 与 Import Health 分开表达。

### 16.4 Runtime 仍持有旧资源

控制：发布后按 GUID invalidation；审计 RenderResourceResolver 的跨帧引用并在必要处增加 generation。

### 16.5 完全通用化导致复杂度失控

控制：首期采用类型化 Editor；只复用现有字段控件，不建设完整反射式 Asset Editor DSL。

### 16.6 Import Settings UI 与 Importer 语义漂移

控制：ImporterId 和 settings schema 是共同契约；Importer 负责最终 Decode/Validate，Editor 不复制隐藏默认值。

## 17. 最终验收标准

完成 P1-P5 后必须满足：

1. Project Panel 中单击 Scene、Material、OBJ、PNG、Shader 均能在 Inspector 获得正确体验；仅双击 Scene 才打开场景。
2. Entity 与 Asset selection 互斥，Scene picking、Hierarchy、Gizmo 无回归。
3. 所有文件资产拥有可版本控制的稳定 `.meta`，现有 GUID 引用迁移后不变。
4. Material 可在 Inspector 编辑并通过显式 Apply/Revert 管理。
5. 所有已启用的 OBJ/PNG Import Settings 真正影响 Artifact，不是仅有 UI；未获底层支持的设置必须禁用并解释原因。
6. Shader 编译健康、反射接口和 diagnostics 可查看。
7. 外部修改不会被静默覆盖。
8. 删除 Library 后能够从 Assets 与 `.meta` 完整重建。
9. 导入失败继续使用 last-good Artifact，并准确展示失败状态。
10. 新增 FBX 时无需修改 InspectorPanel，只需增加 Importer、settings 和 AssetEditor 注册。

满足以上标准后，HuaEngine 才具备一套基础但现代的资产 Inspector 编辑框架；批量编辑、Sampler Asset、缩略图数据库、复杂 Preview Scene 和 Asset Undo History 作为后续扩展项处理。
