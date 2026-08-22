# HuaEngine 内置资产文件化与导入设计

## 1. 状态

- 设计状态：已确认
- 日期：2026-08-23
- 范围：内置 Mesh、Material、Shader 源资产，构建资源复制，项目 AssetLibrary 导入，运行时解析

## 2. 背景

当前引擎通过 C++ 代码硬编码内置资产：

- `SeedBuiltinAssets()` 在代码中构造 builtin Manifest 记录。
- `AssetResolver` 调用 `Mesh::CreateQuad/CreateCube/CreateSphere` 创建 builtin Mesh。
- `AssetResolver` 内嵌 GLSL 字符串并按颜色创建默认材质和错误材质。
- `AssetImportService` 只导入 `AssetSource::File`，builtin 资产不进入项目 Library。

这使 builtin 资产绕过了 importer、artifact、AssetLibrary 和统一缓存链路。新增或调整内置资产必须修改并重新编译 C++，也无法验证发布产物中是否包含完整资产数据。

## 3. 目标

1. 内置资产使用真实源文件，不再把 Mesh 数据、Material 数据或 Shader 源码硬编码在资产解析代码中。
2. 内置资产源文件统一保存在仓库 `Resources/BuiltinAssets`。
3. 构建后将 `Resources` 复制到引擎公共产物目录。
4. 项目初始化资产系统时，同时导入项目资产和引擎内置资产。
5. 内置资产的 artifact 写入当前项目 `{ProjectRoot}/Library`。
6. Mesh 和 Material Resolver 对 builtin 与项目资产使用同一套 Library artifact 解码路径。
7. 保持现有 builtin GUID 稳定，已有 Scene 引用无需迁移。

## 4. 非目标

- 不建立独立的全局 Engine Library。
- 不把 builtin 源文件复制到项目 `Assets`。
- 不实现 builtin 资源热更新或文件监听。
- 不实现按平台区分的 builtin artifact。
- 不删除 `Mesh::CreateQuad/CreateCube/CreateSphere` 等程序化几何 API；它们只是不再用于 builtin 资产解析。
- 不在本阶段处理 FBX、后处理资产或 Shader importer。

## 5. 目录与所有权

### 5.1 仓库源文件

```text
Resources/
  BuiltinAssets/
    manifest.json
    Meshes/
      Quad.mesh
      Cube.mesh
      Sphere.mesh
      Fallback.mesh
    Materials/
      Default.material
      Fallback.material
    Shaders/
      UnlitColor.glsl
```

`Resources/BuiltinAssets` 属于引擎源代码和发布内容，不属于任何用户项目。

### 5.2 构建产物

以 Debug Windows x64 为例：

```text
build/bin/Debug-Windows-x64/
  Editor.exe
  ProjectHub.exe
  HuaEngineCLI.exe
  Resources/
    BuiltinAssets/
      ...
```

所有同配置 Host 共享公共产物目录中的一份 `Resources`。

### 5.3 项目生成数据

```text
ProjectRoot/
  Assets/
  Library/
    AssetLibrary.bin
    Artifacts/
      <builtin-guid>.huamesh
      <builtin-guid>.huamat
      <project-guid>.*
```

项目 Library 同时保存 builtin 和项目资产的 artifact。Library 可删除，并可由引擎 Resources 与项目 Assets 完整重建。

## 6. Builtin Manifest

### 6.1 定位

Builtin Manifest 固定为：

```text
{EngineResourceRoot}/BuiltinAssets/manifest.json
```

`EngineResourceRoot` 通过 `ResourcePaths::GetEngineResourceRoot()` 获取。正式 Host 优先使用可执行文件同级的 `Resources`；smoke 等位于产物子目录的工具允许查找父目录中的 `Resources`。

### 6.2 记录结构

Builtin Manifest 复用资产 Manifest 的核心字段：

```json
{
  "version": 1,
  "assets": [
    {
      "guid": "builtin-mesh-quad",
      "asset_id": "builtin/mesh/quad",
      "kind": "mesh",
      "source": "builtin",
      "relative_path": "Meshes/Quad.mesh",
      "builtin_name": "",
      "import_state": "builtin"
    }
  ]
}
```

约束：

- 所有记录必须为 `source=builtin`。
- `relative_path` 必须是相对路径，且不能逃逸 `BuiltinAssets` 根目录。
- GUID 与 Asset ID 必须唯一。
- Kind 必须能由现有 importer 处理。
- `builtin_name` 仅为旧 Manifest 格式兼容字段，不再参与资产创建或合法性白名单判断。

### 6.3 合并规则

项目 Manifest 加载后，再加载 Builtin Manifest 并合并到内存 Manifest：

- builtin GUID 和 Asset ID 是引擎保留命名空间。
- builtin 记录覆盖项目 Manifest 中同 GUID、同 Asset ID 的旧 builtin 记录。
- 项目资产不得占用 builtin GUID 或 `builtin/` Asset ID。
- 冲突记录导致资产初始化失败，不允许静默选择一方。
- 旧项目中 `relative_path` 为空的 builtin 记录会被新版 builtin 记录替换，因此已有项目无需手工迁移。

第一阶段延续现有行为：合并后的 builtin 记录可以随项目 Manifest 保存。Builtin Manifest 仍是 builtin 元数据的权威来源，每次加载都会重新覆盖项目中的缓存副本。

## 7. 内置资产内容

### 7.1 Mesh

- Quad、Cube、Sphere 使用现有 `.mesh` 源格式。
- Fallback 使用独立的 `Fallback.mesh` 文件；初始几何可以与 Cube 相同，但拥有独立源文件和稳定 GUID。
- `MeshAssetImporter` 无需增加 builtin 专用逻辑。

### 7.2 Material

- `Default.material` 使用白色 `u_Color`。
- `Fallback.material` 使用洋红色 `u_Color`。
- 两者引用共享的 `Shaders/UnlitColor.glsl`。
- Shader 路径写为可由 `ResourcePaths::ResolveRuntimePath()` 解析的产物相对路径，例如 `Resources/BuiltinAssets/Shaders/UnlitColor.glsl`。
- `MaterialAssetImporter` 继续生成现有 Material artifact，不增加 builtin 专用 artifact 格式。

## 8. 导入流程

### 8.1 源路径解析

引入统一的资产源路径解析规则：

```text
AssetSource::File
  -> {ProjectAssetRoot}/{record.RelativePath}

AssetSource::Builtin
  -> {EngineResourceRoot}/BuiltinAssets/{record.RelativePath}
```

两种源路径都必须经过规范化和根目录越界检查。

### 8.2 ImportMissingAssets

`AssetImportService::ImportMissingAssets()` 收集所有可导入的 `File` 和 `Builtin` 记录，而不是只收集项目文件记录。

对每条记录执行：

1. 根据 Source 解析源路径。
2. 根据 Kind 与扩展名查找 importer。
3. 检查项目 AssetLibrary 中是否已有兼容 artifact。
4. 缺失时调用 importer。
5. 将 artifact 提交到 `{ProjectRoot}/Library/Artifacts`。
6. 批次结束后保存 `AssetLibrary.bin`。

导入报告区分项目文件资产数和 builtin 资产数，同时 Imported、Skipped、Failed 统计整个批次。

### 8.3 Reimport

- Project 面板的文件/目录 Reimport 继续只操作项目 `Assets`。
- 启动导入会在 importer 或 artifact 版本变化后自动重建 builtin artifact。
- 底层按 GUID 强制导入接口允许处理 builtin 记录，便于测试和未来提供“重导内置资产”命令。

## 9. Resolver 统一

`AssetResolver` 的 Mesh 和 Material 解析流程统一为：

1. 要求 `AssetService` 已完成项目 Manifest 与 Library 初始化。
2. 根据 GUID 查找合并后的 Manifest 记录。
3. 校验 Kind。
4. 查询 Runtime Cache。
5. 从项目 AssetLibrary 读取 artifact。
6. 使用现有 `DecodeMeshArtifact()` 或 `DecodeMaterialArtifact()` 构造运行时对象。
7. 写入 Runtime Cache。

需要删除的 builtin 旁路：

- `MakeBuiltinRecord()`。
- `IsBuiltinGuid()`。
- `AssetResolver::CreateBuiltinMesh()`。
- `AssetResolver::CreateBuiltinMaterial()`。
- Resolver 中内嵌的 vertex/fragment GLSL 字符串。
- Manifest 未加载时直接创建 builtin 对象的兼容路径。

Texture Resolver 暂不新增 builtin texture；但统一源路径和导入逻辑必须允许未来在 Builtin Manifest 中增加 PNG Texture，而无需修改 Resolver 分支。

## 10. 构建资源复制

CMake 增加唯一的 `HuaEngineResources` 资源目标：

- 将仓库 `Resources` 复制到当前配置公共运行时产物目录的 `Resources`。
- Editor、ProjectHub、HuaEngineCLI 依赖该目标。
- 删除 Editor 和 ProjectHub 当前重复的资源 POST_BUILD 命令。
- 资源目标只有一份，避免多个 Host 并行构建时同时删除或覆盖相同目录。
- 构建单个 Host target 时，也必须先完成资源复制。

## 11. 错误处理

以下情况视为资产初始化失败：

- Builtin Manifest 缺失、损坏或版本不支持。
- builtin 记录不是 `AssetSource::Builtin`。
- builtin 路径为绝对路径或逃逸 `BuiltinAssets`。
- builtin GUID/Asset ID 与项目资产冲突。
- builtin 源文件缺失。
- builtin Kind 没有可用 importer。
- builtin artifact 无法提交或 Library catalog 无法保存。

不得回退到 C++ 硬编码 Mesh、Material 或 Shader。诊断必须包含 GUID、Asset ID 或源路径，以便定位发布资源缺失问题。

## 12. 兼容性

- 保持 `BuiltinAssetGuids` 的字符串值不变。
- 保持 Scene 中现有 GUID 引用不变。
- 保持 `AssetSource::Builtin` 和 `AssetImportState::Builtin`。
- `builtin_name` 暂时保留在 Manifest 序列化结构中，但不再驱动运行时创建。
- 现有项目第一次由新版本打开时，会为 6 个 builtin 资产生成 Library artifact。
- 依赖 builtin 解析但未初始化项目资产服务的调用需要改为先执行 `InitializeProjectAssets()`。

## 13. 测试与验收

### 13.1 自动化测试

1. Builtin Manifest 能加载 6 条合法记录。
2. builtin 路径越界、重复 GUID、错误 Source 和缺失源文件会失败。
3. 空项目第一次初始化会导入 6 个 builtin artifact。
4. 第二次初始化会跳过 6 个兼容 artifact。
5. 删除一个 builtin artifact 后，下一次初始化只重建缺失项。
6. builtin Mesh artifact 能解码为正确 Mesh。
7. 默认与 fallback Material artifact 能解码出正确 Shader 路径和颜色。
8. Resolver 对 builtin 与项目资产都只读取 Library artifact。
9. 删除或禁用 Resolver 硬编码工厂后，相关解析测试仍通过。
10. 构建 Editor、ProjectHub 或 HuaEngineCLI 后，公共产物目录存在完整 `Resources/BuiltinAssets`。

### 13.2 回归测试

- `AssetImportSmoke`
- `AssetLibrarySmoke`
- `AssetServiceSmoke`
- `ValidationServiceSmoke`
- `ApplicationServicesSmoke`
- `ApplicationOperationsSmoke`
- `RenderingOperationsSmoke`
- `EditorAssetPickerSmoke`
- `HostConsistencySmoke`

### 13.3 手工验收

1. 删除测试项目 `Library` 后启动 Editor。
2. 确认 Library 自动生成 builtin Mesh 和 Material artifact。
3. 打开已有场景，Quad、Cube、Sphere 正常显示。
4. 将实体材质切换为 default/fallback，分别显示白色和洋红色。
5. Inspector 资产选择器继续显示 builtin Mesh 和 Material。

## 14. 实施阶段

### P1：Builtin 源资产与构建复制

- 创建 `Resources/BuiltinAssets` 内容和 manifest。
- 建立统一 `HuaEngineResources` CMake 目标。
- 验证公共产物目录资源完整性。
- 完成后单独提交。

### P2：Manifest 合并与 Library 导入

- 加载并校验 Builtin Manifest。
- 合并 builtin 与项目 Manifest。
- 按 Source 解析源路径。
- `AssetImportService` 导入 builtin artifact。
- 更新验证和导入报告。
- 完成后单独提交。

### P3：Resolver 统一与硬编码移除

- Mesh、Material Resolver 统一读取 Library。
- 删除 builtin 运行时工厂和嵌入 Shader。
- 更新依赖旧旁路的测试与调用方。
- 执行完整回归和手工验收。
- 完成后单独提交。

## 15. 完成定义

满足以下条件后，本设计完成：

- 仓库和构建产物均包含完整 builtin 源资产。
- 空项目能把全部 builtin 资产导入自己的 Library。
- builtin Mesh/Material 的运行时对象只由 Library artifact 创建。
- Resolver 中不存在 builtin Mesh、Material 或 Shader 数据硬编码。
- 已有 Scene 的 builtin GUID 引用保持可用。
- 所列自动化测试通过，每个 P 均有独立提交。
