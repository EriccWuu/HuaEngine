# 资源系统地基重构设计

## 背景

当前资源系统已经有 `AssetService`、`AssetRegistry`、CLI asset operation 和若干 smoke 测试，但资源身份、组件序列化和运行时对象缓存还没有清晰分层。

主要问题：

- `AssetRecord` 同时保存 metadata 和 `Ref<Mesh>` / `Ref<Material>` / `Ref<Texture2D>` runtime payload，导致 registry 既像资产数据库，又像运行时缓存。
- `AssetHandle` 当前由进程内自增产生，不适合作为 scene/component 的长期序列化身份。
- `MeshComponent` 和 `MaterialComponent` 仍保存或暴露 runtime 对象、全局 manager 查询和缓存字段。
- `Serializer<Ref<T>>` 会序列化对象本体，不适合表达“资源引用”。
- `RenderResourceResolver` 仍依赖 `MeshManager`，并在失败时回读 `MeshComponent::m_CachedVertexArray`。
- 项目没有持久化 asset manifest，无法稳定记录资源 GUID、路径、类型和 builtin 资源。

本设计选择先投入完整资源数据库地基：使用稳定 `AssetGuid`、项目级 manifest、metadata/runtime cache 分离、组件纯数据化、resolver 懒加载和 fallback 渲染。自动目录扫描、删除检测、移动检测、热重载和完整编辑器资产面板不进入本轮。

## 目标

- 引入稳定 `AssetGuid` 作为长期资源身份。
- 使用项目级 `.hua/assets.json` 作为 asset manifest。
- 明确 `AssetHandle` 只用于 runtime session，不落盘，不写入 scene。
- 将 `AssetRegistry` 纯化为 metadata registry。
- 将 runtime 对象缓存移入独立 cache/resolver 层。
- 让 scene/component 只保存稳定资源引用和纯数据 override。
- 让渲染资源解析统一通过 asset resolver，不再从组件回读 runtime cache。
- 保留旧 scene 的一次性读取迁移能力，保存后只写新格式。
- 运行时资源缺失时使用 fallback 资源继续渲染，同时保留明确诊断。

## 非目标

- 不做全目录自动扫描和批量导入。
- 不做资产删除检测、移动检测、GUID 冲突自动合并。
- 不做热重载。
- 不做完整编辑器资产浏览器或 asset picker 交互。
- 不把 `AssetHandle` 设计成跨进程或跨 session 稳定身份。

## 资产身份和 Manifest

### 长期身份

- `AssetGuid` 是唯一长期身份，scene/component 只保存 GUID。
- `AssetId` 是项目内可读路径或别名，例如 `Meshes/Player.mesh`。它可以用于 CLI 查询、诊断和人工阅读，但不作为长期引用主键。
- `AssetHandle` 保留为 runtime session handle，只用于当前进程内快速索引或兼容现有 API。

### Manifest 路径

项目级 manifest 固定为：

```text
.hua/assets.json
```

manifest 由项目加载或 asset manifest init 流程创建和加载。路径解析基于 `ProjectContext` 的 project root 和 asset root。

### Manifest 记录

每条记录至少包含：

```json
{
  "guid": "asset-guid-string",
  "asset_id": "Meshes/Player.mesh",
  "kind": "mesh",
  "source": "file",
  "relative_path": "Meshes/Player.mesh",
  "builtin_name": "",
  "import_state": "imported"
}
```

字段含义：

- `guid`：稳定资源 GUID。
- `asset_id`：项目内可读路径或别名。
- `kind`：`mesh`、`material`、`texture2d`、未来扩展类型。
- `source`：初期支持 `file` 和 `builtin`。
- `relative_path`：`source=file` 时指向 asset root 下的相对路径。
- `builtin_name`：`source=builtin` 时记录内置资源名称。
- `import_state`：记录导入或注册状态，初期用于 validate 和诊断。

### Builtin 资源

内置资源也进入 manifest，并拥有稳定 GUID。

初期 builtin 资源包括：

- Quad mesh
- Cube mesh
- Sphere mesh
- default material
- fallback mesh
- fallback material

默认组件和 fallback 渲染都引用这些 builtin manifest 记录，避免长期保留特殊 name fallback 路径。

## Registry、Cache 和 Resolver 架构

### AssetRegistry

`AssetRegistry` 只保存 metadata：

- GUID 到 record 的映射。
- AssetId 到 GUID 的映射。
- 可选 runtime handle 到 GUID 的映射。

它不再持有 `Ref<Mesh>`、`Ref<Material>` 或 `Ref<Texture2D>`。

### AssetRuntimeCache

新增或重塑 runtime cache，按 `AssetGuid` 缓存已加载对象：

- mesh cache
- material cache
- texture cache
- material instance cache 或 resolver 内部实例缓存

cache 是 runtime 层，不参与 manifest 持久化。

### AssetService

`AssetService` 负责项目资产数据库的稳定 API：

- manifest 初始化、加载、保存。
- 单文件 import/register。
- builtin 记录确保存在。
- GUID、AssetId、runtime handle 查询。
- metadata validate。
- runtime validate 调度。
- 兼容旧 asset operation。

### AssetResolver

`AssetResolver` 负责按 GUID 获取 runtime 资源：

1. 查询 `AssetRuntimeCache`。
2. cache miss 时查询 `AssetRegistry` metadata。
3. `source=file` 时按 asset kind 加载文件。
4. `source=builtin` 时创建或获取 builtin runtime 对象。
5. 加载成功后写回 cache。
6. 加载失败时返回诊断，并允许调用方使用 fallback。

短期内 `MeshManager` 和 `MaterialLibrary` 可以作为底层兼容注册点，但 scene/component 不再以它们作为资源身份来源。后续可以把它们收缩成 resolver 内部实现细节。

## 组件和 Scene Contract

### 资源引用类型

新增轻量资源引用类型：

```cpp
struct AssetReference {
    AssetGuid Guid;
};

struct MeshAssetRef {
    AssetReference Reference;
};

struct MaterialAssetRef {
    AssetReference Reference;
};

struct TextureAssetRef {
    AssetReference Reference;
};
```

实际命名可以在实现计划中结合现有命名风格微调，但 contract 必须表达 typed asset reference，而不是 runtime object pointer。

### MeshComponent

目标字段：

- `MeshAssetRef Mesh`

需要移除或停止暴露：

- `MeshAssetName`
- `m_CachedVertexArray`
- `GetMesh()`
- `GetVertexArray()`
- `SetMesh(Ref<Mesh>)`
- 依赖 `MeshManager::Instance()` 的组件行为

组件只表达“这个实体引用哪个 mesh asset”，不负责加载或缓存。

### MaterialComponent

目标字段：

- `MaterialAssetRef Material`
- `MaterialOverrideSet Overrides`
- `MaterialBlendMode BlendMode`

`MaterialInstance` 不再作为 reflected/serialized 字段。base material 由 `MaterialAssetRef` 指向 asset system，实体级参数覆盖由 `Overrides` 保存。resolver 在渲染阶段结合 base material 和 overrides 生成或缓存 `MaterialInstance`。

### CameraComponent

`CameraComponent` 纯化为相机参数和开关字段，不保存 `Ref<Camera>` 作为长期状态。runtime camera 对象由系统根据组件参数构建或同步。

初期至少保留：

- `Primary`
- `FixedAspectRatio`

如果当前 `Camera` 内有 projection 参数需要持久化，应在组件上显式建字段，而不是序列化 `Ref<Camera>`。

## Scene 迁移策略

读取旧 scene 时做一次性兼容迁移：

- 旧 `MeshAssetName` 通过 manifest 的 `asset_id`、旧 mesh name 或兼容 alias 解析为 `MeshAssetRef.Guid`。
- 旧 `MaterialInstance` 尽量解析 base material 到 `MaterialAssetRef.Guid`。
- 旧 material parameter overrides 迁移到 `MaterialOverrideSet Overrides`。
- 无法迁移的引用写入空 GUID，并记录诊断。

保存 scene 时只写新格式：

- mesh GUID
- material GUID
- material overrides
- camera 纯数据字段

保存后不再写 `MeshAssetName` 或 `MaterialInstance` 对象。旧格式兼容只存在于读取路径，不成为长期双格式 contract。

## 渲染数据流

### Extraction

`SceneRenderExtractor` 从组件抽取纯数据：

- source entity
- transform
- mesh GUID
- material GUID
- material overrides
- blend mode

### RenderItem

`RenderItem` 不再携带：

- `MeshAssetName`
- `Ref<MaterialInstance>`

它只携带 asset reference 和 override 数据。

### Resource Resolve

`RenderResourceResolver` 依赖 asset resolver 接口：

- resolve mesh GUID 到 `VertexArray` 或 mesh runtime object。
- resolve material GUID 到 base material。
- 用 material overrides 生成或缓存 `MaterialInstance`。
- 不直接访问 `MeshManager`。
- 不回读 `MeshComponent::m_CachedVertexArray`。

### Fallback 行为

渲染缺失或加载失败时使用 fallback 资源继续渲染：

- mesh 缺失或加载失败：使用 builtin fallback mesh。
- material 缺失或加载失败：使用 builtin fallback material。
- shader 或 base material 无效：使用 fallback material。

同时记录 `RenderDiagnostic`：

- 原始 GUID。
- 解析出的 asset id。
- 失败原因。
- fallback 资源 GUID。
- source entity。

渲染操作默认仍可成功，但 `RenderResult.Diagnostics` 和 stats 必须能看出 fallback 次数。CLI validate 和 runtime validate 不因为 fallback 而吞掉错误。

## CLI 和 Application API

本轮保留并扩展 asset operation：

- `asset.manifest.init`：初始化 `.hua/assets.json`，确保 builtin 记录存在。
- `asset.import` 或 `asset.register`：单文件导入或注册，生成 GUID，写入 manifest。
- `asset.resolve`：支持 GUID 和 AssetId 查询。
- `asset.validate`：输出 metadata 和 runtime 两类问题。
- `asset.list`：列出 manifest 资产，建议加入以支持 smoke 和人工检查。

旧 operation 兼容映射：

- `asset.create_builtin_mesh`：创建或确保 builtin manifest 记录，并可按需生成项目文件。
- `asset.register_mesh`
- `asset.load_mesh`
- `asset.register_material`
- `asset.load_material`
- `asset.register_texture`

这些旧 API 内部应逐步改为写 manifest 和 runtime cache，而不是把 runtime payload 塞进 `AssetRecord`。

## Validation

### Metadata Validate

检查：

- manifest JSON 可解析。
- GUID 非空且唯一。
- AssetId 非空且唯一或符合明确 alias 规则。
- `kind` 合法。
- `source` 合法。
- file 资源路径不逃逸 asset root。
- file 资源存在性。
- builtin 资源名称合法。

### Runtime Validate

检查：

- mesh 可反序列化并提供 vertex array。
- material 可反序列化并有 base shader。
- texture source 可加载或至少能明确报告 source-only 状态。
- builtin 资源可创建。
- fallback 资源可用。

runtime validate 可以按指定 GUID、指定 kind 或全部资产运行。

## 测试策略

需要覆盖：

- manifest 初始化和 builtin 记录写入。
- GUID 唯一性、AssetId 查询、路径越界校验。
- 单文件 import/register 和 manifest 保存加载 round-trip。
- `AssetRegistry` 不再保存 runtime payload。
- `AssetResolver` cache miss 懒加载和 cache hit 复用。
- scene 旧格式读取迁移。
- scene 新格式保存不回写旧字段。
- `MeshComponent` / `MaterialComponent` / `CameraComponent` 新 reflected fields。
- `MaterialComponent` override round-trip。
- render resolver 对 file、builtin、fallback 的行为。
- CLI import/resolve/validate/list 工作流。
- 既有 smoke 中依赖 `MeshAssetName`、`MaterialInstance`、`asset.resolve_mesh` 等行为的断言更新。

## 分阶段落地建议

虽然本设计覆盖完整地基，但实现应拆小步：

1. 定义 `AssetGuid`、manifest record、manifest load/save 和 builtin seed。
2. 纯化 `AssetRegistry`，引入 runtime cache/resolver。
3. 更新 `AssetService` 和 CLI/API 到 manifest 模型。
4. 引入 typed asset reference 和组件新字段。
5. 实现 scene 旧格式迁移和新格式保存。
6. 改造 render extraction/resolver/fallback。
7. 更新 reflection generated 文件和相关 smoke 测试。

## 验收标准

- `.hua/assets.json` 能初始化、保存、加载，并包含稳定 builtin GUID。
- scene/component 不再把 `AssetHandle` 或 runtime `Ref<T>` 作为资源引用持久化格式。
- `AssetHandle` 在文档和 API 中明确为 runtime-only。
- `AssetRegistry` 不再持有 mesh/material/texture runtime payload。
- `MeshComponent` 不再依赖 `MeshManager` 或缓存 `VertexArray`。
- `MaterialComponent` 不再序列化 `Ref<MaterialInstance>`。
- 旧 scene 可读取并迁移，保存后只写新字段。
- renderer 通过 asset resolver 解析资源。
- 资源缺失或加载失败时 renderer 使用 fallback，并产生诊断。
- `asset.validate` 能区分 metadata 问题和 runtime load 问题。
- 相关 smoke 测试通过，至少覆盖 asset service、application operations、scene serialization、reflection generated、serialization policy、editor inspector runtime、CLI workflow 和 render resolver 行为。

