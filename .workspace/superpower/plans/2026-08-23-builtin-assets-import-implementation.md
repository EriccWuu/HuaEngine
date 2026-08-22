# HuaEngine 内置资产文件化与导入实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 builtin Mesh、Material 和 Shader 从 C++ 硬编码迁移为引擎 Resources 中的真实源资产，并在项目初始化时导入项目 Library，由 Resolver 统一读取 artifact。

**Architecture:** 仓库 `Resources/BuiltinAssets` 保存 builtin manifest 与源资产，统一 CMake 资源目标将其复制到公共运行时产物目录。项目加载时合并 builtin manifest，`AssetImportService` 根据 `AssetSource` 选择项目或引擎资源根并导入同一个项目 Library，`AssetResolver` 最终只保留 artifact 解码路径。

**Tech Stack:** C++20、CMake、JSON Manifest、YAML Mesh/Material、现有 AssetImporter/AssetLibrary/AssetResolver、Smoke 测试

**Spec:** `.workspace/superpower/specs/2026-08-23-builtin-assets-import-design.md`

## Global Constraints

- builtin 源资产固定保存在仓库和产物的 `Resources/BuiltinAssets`。
- builtin artifact 固定写入当前项目 `{ProjectRoot}/Library`，不创建独立 Engine Library。
- 保持现有 6 个 builtin GUID 不变。
- Resolver 不得保留 builtin Mesh、Material 或 Shader 数据硬编码回退。
- 代码注释使用通俗英文，文档主体使用中文。
- 不暂存或修改用户已有的 `Tests/TestProj` 运行数据。
- 每个 P 完成验证后单独提交。

---

### P1：Builtin 源资产与统一构建复制

**Files:**
- Create: `Resources/BuiltinAssets/manifest.json`
- Create: `Resources/BuiltinAssets/Meshes/Quad.mesh`
- Create: `Resources/BuiltinAssets/Meshes/Cube.mesh`
- Create: `Resources/BuiltinAssets/Meshes/Sphere.mesh`
- Create: `Resources/BuiltinAssets/Meshes/Fallback.mesh`
- Create: `Resources/BuiltinAssets/Materials/Default.material`
- Create: `Resources/BuiltinAssets/Materials/Fallback.material`
- Create: `Resources/BuiltinAssets/Shaders/UnlitColor.glsl`
- Modify: `CMakeLists.txt`
- Modify: `Editor/CMakeLists.txt`
- Modify: `ProjectHub/CMakeLists.txt`
- Modify: `HuaEngine/src/HuaEngine/Core/ResourcePaths.cpp`
- Create: `Tests/BuiltinAssetResourcesSmoke.cpp`

**Interfaces:**
- Consumes: `ResourcePaths::GetEngineResourceRoot()`、现有公共运行时输出目录。
- Produces: `HuaEngineResources` 构建目标和可定位的 `Resources/BuiltinAssets` 文件树。

- [ ] 新增 `BuiltinAssetResourcesSmoke`，检查 builtin manifest、4 个 Mesh、2 个 Material 和 Shader 均可从 EngineResourceRoot 定位。
- [ ] 构建并运行测试，确认因资源目录或父级资源定位缺失而失败。
- [ ] 创建 builtin manifest、Mesh、Material 和 Shader 源文件；Fallback Mesh 使用独立源文件。
- [ ] 在根 CMake 创建唯一 `HuaEngineResources` 目标，将仓库 `Resources` 复制到配置对应的公共产物目录。
- [ ] 让 Editor、ProjectHub、HuaEngineCLI 和资源 smoke 依赖该目标，删除 Editor/ProjectHub 重复 POST_BUILD 拷贝。
- [ ] 扩展 `ResourcePaths`，允许产物子目录中的 smoke 查找父级公共 `Resources`。
- [ ] 构建 `Editor ProjectHub HuaEngineCLI BuiltinAssetResourcesSmoke` 并运行 smoke。
- [ ] 检查 `build/bin/Debug-Windows-x64/Resources/BuiltinAssets` 内容完整，执行 `git diff --check`。
- [ ] 提交：`feat(asset): package builtin asset sources`

### P2：Builtin Manifest 合并与项目 Library 导入

**Files:**
- Create: `HuaEngine/src/HuaEngine/Asset/BuiltinAssetCatalog.h`
- Create: `HuaEngine/src/HuaEngine/Asset/BuiltinAssetCatalog.cpp`
- Create: `HuaEngine/src/HuaEngine/Asset/AssetSourcePath.h`
- Create: `HuaEngine/src/HuaEngine/Asset/AssetSourcePath.cpp`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetManifest.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetManifest.cpp`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Asset/Import/AssetImportService.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/Import/AssetImportService.cpp`
- Modify: `Tests/AssetImportSmoke.cpp`
- Modify: `Tests/AssetServiceSmoke.cpp`

**Interfaces:**
- Consumes: `Resources/BuiltinAssets/manifest.json`、`ResourcePaths::GetEngineResourceRoot()`、现有 importer registry。
- Produces: `LoadBuiltinAssetCatalog()`、`MergeBuiltinAssetCatalog()`、`ResolveAssetSourcePath()`，以及同时覆盖 File/Builtin 的导入报告。

- [ ] 为 catalog 加载、路径越界、Source 错误、项目冲突和 6 个 builtin 首次导入编写失败测试。
- [ ] 构建并运行 `AssetImportSmoke AssetServiceSmoke`，确认新增断言失败。
- [ ] 抽取可从任意路径加载 Manifest 的解析入口，保持项目 Manifest v1 兼容。
- [ ] 实现 builtin catalog 加载与合并，builtin 记录覆盖旧 builtin 缓存记录，拒绝项目占用保留 GUID/Asset ID。
- [ ] 将 builtin 合并接入可写和只读 Manifest 加载路径。
- [ ] 实现 `ResolveAssetSourcePath()`：File 使用项目 Assets，Builtin 使用 EngineResourceRoot/BuiltinAssets，并校验路径不逃逸。
- [ ] 扩展 `AssetImportReport`，分别统计项目 File 和 Builtin 数量。
- [ ] 修改 `ImportMissingAssets()` 和 `ImportAssets()`，让两种 Source 共用 importer、兼容性检查、artifact 提交与保存逻辑。
- [ ] builtin catalog/source/importer/artifact 失败时令初始化失败并保留明确诊断；普通项目单资产失败维持批次诊断语义。
- [ ] 更新 AssetRegistry 的 builtin 绝对路径和磁盘存在状态，验证逻辑检查 builtin importer 与 artifact。
- [ ] 运行 `AssetImportSmoke AssetLibrarySmoke AssetServiceSmoke ValidationServiceSmoke ApplicationServicesSmoke ApplicationOperationsSmoke`。
- [ ] 执行 `git diff --check` 并提交：`feat(asset): import builtin assets into project library`

### P3：Resolver 统一与硬编码旁路移除

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetResolver.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetResolver.cpp`
- Modify: `Editor/src/EditorLayer.cpp`
- Modify: `Tests/AssetServiceSmoke.cpp`
- Modify: `Tests/RenderingOperationsSmoke.cpp`
- Modify: `Tests/AssetImportSmoke.cpp`

**Interfaces:**
- Consumes: 已合并 Manifest、项目 AssetLibrary、现有 Mesh/Material/Texture artifact decoder。
- Produces: 不区分 File/Builtin 创建方式的 Resolver。

- [ ] 新增测试：builtin Mesh/Material 必须存在 Library artifact，未初始化 Manifest 时解析失败，删除 artifact 后不得硬编码回退。
- [ ] 构建并运行相关 smoke，确认旧 Resolver 使测试失败。
- [ ] 将 Mesh Resolver 的 File/Builtin 分支合并为统一 `ReadArtifact + DecodeMeshArtifact`。
- [ ] 将 Material Resolver 的 File/Builtin 分支合并为统一 `ReadArtifact + DecodeMaterialArtifact`，保留 Shader 路径解析和材质参数构造。
- [ ] 让 Texture Resolver 对未来 builtin Texture 复用 artifact 路径，不保留 builtin unsupported 特判。
- [ ] 删除 `MakeBuiltinRecord()`、`IsBuiltinGuid()`、`CreateBuiltinMesh()`、`CreateBuiltinMaterial()` 和内嵌 GLSL。
- [ ] 移除 Editor `WarmupSceneAssets()` 中对默认 Mesh/Material 工厂的 builtin 预热旁路，资产可用性统一由 AssetService/Resolver 保证。
- [ ] 更新所有依赖“未初始化也能解析 builtin”的测试和调用点。
- [ ] 运行 `AssetImportSmoke AssetLibrarySmoke AssetServiceSmoke ValidationServiceSmoke ApplicationServicesSmoke ApplicationOperationsSmoke RenderingOperationsSmoke EditorAssetPickerSmoke HostConsistencySmoke`。
- [ ] 构建 `Editor`，执行 `git diff --check`，检查 Resolver 不再包含 builtin 工厂或嵌入 Shader。
- [ ] 提交：`refactor(asset): resolve builtin assets from artifacts`

### 最终验证

**Files:**
- Verify only

- [ ] 删除临时测试项目 Library 后运行资产初始化测试，确认 6 个 builtin artifact 可重建。
- [ ] 运行 `ctest -C Debug --output-on-failure`，若全量测试受环境限制，记录未运行或失败项及原因。
- [ ] 构建 `Editor ProjectHub HuaEngineCLI`。
- [ ] 确认公共产物目录存在完整 builtin 源资产。
- [ ] 确认 `git status` 只剩用户原有 `Tests/TestProj` 数据改动。
- [ ] 汇总 P1、P2、P3 提交和验证结果。
