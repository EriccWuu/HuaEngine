# Editor Asset Reimport Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Editor Project 面板中支持文件/文件夹 Reimport 和 Reimport All，并由资产系统完成自动注册、强制产物覆盖与运行时缓存失效。

**Architecture:** Editor 只提交目标路径；`ApplicationOperations` 将请求转交 `AssetService`。资产层先扫描并注册全部受支持源文件，再通过带 `Force` 策略的 `AssetImportService` 覆盖 Library 产物，最后按成功 GUID 失效 RuntimeCache 并返回批量报告。

**Tech Stack:** C++20、ImGui、现有 AssetManifest/AssetLibrary/AssetImporterRegistry、CMake smoke tests。

**Spec:** `.workspace/superpower/specs/2026-08-22-editor-asset-reimport-design.md`

## Global Constraints

- 文档主体使用中文；代码注释只能使用通俗英文。
- Reimport 只能访问当前项目 `Assets/` 内部路径。
- 已注册资产必须保持 GUID 稳定。
- 不支持 OBJ、FBX、TGA、MTL；首版仅支持 `.mesh`、`.material/.mat`、`.png`。
- 文件夹批量操作跳过不支持格式；单文件不支持格式返回 ManualIntervention。
- importer 或单产物提交失败不得删除旧 Library 产物，也不得清除旧 RuntimeCache。
- 每个任务完成测试后单独提交。

---

### Task 1: Importer 类型推断与 RuntimeCache 失效

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Asset/Import/AssetImporterRegistry.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/Import/AssetImporterRegistry.cpp`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetRuntimeCache.h`
- Test: `Tests/AssetImportSmoke.cpp`

**Interfaces:**
- Produces: `std::optional<AssetImporterMatch> AssetImporterRegistry::FindByExtension(std::string_view extension) const`
- Produces: `void AssetRuntimeCache::Invalidate(const AssetGuid& guid)`

- [ ] **Step 1: 写 importer 类型推断失败测试**

在 `TestImporterSelection()` 中加入：

```cpp
const auto meshMatch = registry.FindByExtension(".MESH");
Require(meshMatch && meshMatch->Kind == HE::AssetKind::Mesh, "Expected mesh kind inference");
const auto pngMatch = registry.FindByExtension(".png");
Require(pngMatch && pngMatch->Kind == HE::AssetKind::Texture2D, "Expected texture kind inference");
const auto materialMatch = registry.FindByExtension(".MAT");
Require(materialMatch && materialMatch->Kind == HE::AssetKind::Material, "Expected material kind inference");
Require(!registry.FindByExtension(".obj"), "Expected unsupported extension inference rejection");
```

- [ ] **Step 2: 写 RuntimeCache 失效失败测试**

在独立测试函数中存入可构造的 mesh 和 material，调用 `Invalidate("asset-guid")` 后断言 `FindMesh()`、`FindMaterial()` 均返回空；另一个 GUID 的缓存仍保留。实现仍必须同时清除 texture map。

- [ ] **Step 3: 运行测试确认红灯**

Run:

```powershell
cmake --build build --config Debug --target AssetImportSmoke --parallel 8
```

Expected: 编译失败，提示 `FindByExtension`、`AssetImporterMatch` 和 `Invalidate` 尚不存在。

- [ ] **Step 4: 实现最小接口**

在 registry 中增加：

```cpp
struct AssetImporterMatch {
    AssetKind Kind = AssetKind::Unknown;
    const AssetImporter* Importer = nullptr;
};

[[nodiscard]] std::optional<AssetImporterMatch> FindByExtension(std::string_view extension) const;
```

实现按 `Mesh -> Material -> Texture2D` 的稳定顺序调用现有 `Find()`；扩展名仍由 registry 统一做大小写归一化。`Invalidate()` 对三个 map 执行 `erase(guid)`。

- [ ] **Step 5: 运行 smoke 确认绿灯**

```powershell
cmake --build build --config Debug --target AssetImportSmoke --parallel 8
& build/bin/Debug-Windows-x64/smoke/AssetImportSmoke.exe
```

Expected: `AssetImportSmoke passed`。

- [ ] **Step 6: 提交**

```powershell
git add HuaEngine/src/HuaEngine/Asset/Import/AssetImporterRegistry.* HuaEngine/src/HuaEngine/Asset/AssetRuntimeCache.h Tests/AssetImportSmoke.cpp
git commit -m "feat(asset): infer importer kinds and invalidate runtime cache"
```

---

### Task 2: 强制导入策略

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Asset/Import/AssetImportService.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/Import/AssetImportService.cpp`
- Test: `Tests/AssetImportSmoke.cpp`

**Interfaces:**
- Consumes: `AssetImporterRegistry::Find()`、`AssetLibrary::CommitArtifact()`
- Produces: `enum class AssetImportPolicy { MissingOnly, Force }`
- Produces: `ImportAssets(context, manifest, assetGuids, policy, report)`
- Preserves: `ImportMissingAssets()` 的启动补缺行为

- [ ] **Step 1: 写强制覆盖失败测试**

建立 `.mesh` 源文件并完成首次 `InitializeProjectAssets()`；保存 GUID 和首次 artifact payload。用不同名称/顶点数据覆盖同一路径，再执行：

```cpp
const std::array<HE::AssetGuid, 1> guids = { meshRecord.Guid };
HE::AssetImportReport forceReport;
const auto forceResult = importService.ImportAssets(
    context,
    assetService.GetManifest(),
    guids,
    HE::AssetImportPolicy::Force,
    &forceReport);
```

断言 `ImportedAssets == 1`、`SkippedAssets == 0`，重新读取 artifact 后 payload 已变化。

- [ ] **Step 2: 写失败保留旧产物测试**

首次成功后将源 `.mesh` 写成无效内容，Force import；断言失败计数为 1，并且 `ReadArtifact(guid)` 仍返回首次成功 payload。

- [ ] **Step 3: 运行测试确认红灯**

```powershell
cmake --build build --config Debug --target AssetImportSmoke --parallel 8
```

Expected: 编译失败，强制导入接口不存在。

- [ ] **Step 4: 提取单记录导入并实现策略**

接口使用 GUID 集合，避免 manifest vector 变更导致悬空指针：

```cpp
enum class AssetImportPolicy { MissingOnly, Force };

ResultEnvelope ImportAssets(
    const ProjectContext& context,
    const AssetManifest& manifest,
    std::span<const AssetGuid> assetGuids,
    AssetImportPolicy policy,
    AssetImportReport* outReport = nullptr) const;
```

`MissingOnly` 在兼容产物存在时计入 skipped；`Force` 总是调用 importer。只有 importer 成功后才调用 `CommitArtifact()`；循环结束统一 `AssetLibrary::Save()`。在 report 增加 `ImportedAssetGuids`，仅记录产物提交成功的 GUID。

- [ ] **Step 5: 让 ImportMissingAssets 复用新入口**

从 manifest 收集全部 file-source GUID，调用 `ImportAssets(..., MissingOnly, ...)`，保持现有 payload key 和 operation id。

- [ ] **Step 6: 运行回归测试**

```powershell
cmake --build build --config Debug --target AssetImportSmoke AssetLibrarySmoke --parallel 8
& build/bin/Debug-Windows-x64/smoke/AssetImportSmoke.exe
& build/bin/Debug-Windows-x64/smoke/AssetLibrarySmoke.exe
```

Expected: 两个 smoke 均 passed。

- [ ] **Step 7: 提交**

```powershell
git add HuaEngine/src/HuaEngine/Asset/Import/AssetImportService.* Tests/AssetImportSmoke.cpp
git commit -m "feat(asset): support forced artifact reimport"
```

---

### Task 3: 批量扫描、自动注册与应用接口

**Files:**
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetService.h`
- Modify: `HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.h`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp`
- Modify: `HuaEngine/src/HuaEngine/Application/ApplicationOperations.cpp` operation registry section
- Test: `Tests/AssetImportSmoke.cpp`
- Test: `Tests/ApplicationOperationsSmoke.cpp`

**Interfaces:**
- Consumes: `FindByExtension()`、`ImportAssets(..., Force, ...)`、`RuntimeCache::Invalidate()`
- Produces: `AssetReimportReport`
- Produces: `AssetService::ReimportAssets()`
- Produces: `ApplicationOperations::ReimportAssets()` with operation id `asset.reimport`
- Produces: `AssetService::CanImportSource()` 与 `ApplicationOperations::CanImportAssetSource()`

- [ ] **Step 1: 写文件夹自动注册失败测试**

在临时项目 `Assets/ReimportBatch/` 中创建一个 `.mesh`、一个 `.png` 和一个 `.txt`，不预写 manifest。调用 `AssetService::ReimportAssets(context, batchDirectory, &report)`，断言：

```cpp
Require(report.ScannedFiles == 3, "Expected recursive scan count");
Require(report.SupportedFiles == 2, "Expected supported asset count");
Require(report.RegisteredAssets == 2, "Expected unregistered assets to be registered");
Require(report.ReimportedAssets == 2, "Expected supported assets to be imported");
Require(report.SkippedFiles == 1, "Expected unsupported file skip");
```

同时断言两个 AssetId 可解析，重复调用后 GUID 保持不变。

- [ ] **Step 2: 写路径边界与单文件不支持测试**

调用 AssetRoot 外部路径，断言 Failed 且 manifest 数量不变；对 AssetRoot 内单个 `.txt` 调用，断言 ManualIntervention。

- [ ] **Step 3: 写缓存失效测试**

首次解析 mesh 以填充 RuntimeCache；覆盖源并 Reimport；断言 `FindMesh(guid)` 为空，重新 Resolve 后得到新 mesh 名称。将源破坏后再次 Reimport，断言旧缓存不被清除。

- [ ] **Step 4: 运行测试确认红灯**

```powershell
cmake --build build --config Debug --target AssetImportSmoke ApplicationOperationsSmoke --parallel 8
```

Expected: 编译失败，`AssetReimportReport` 和 `ReimportAssets` 不存在。

- [ ] **Step 5: 实现扫描和两阶段注册**

`AssetService::ReimportAssets()`：

1. 规范化 AssetRoot/target，拒绝越界和不存在目标。
2. 文件或递归目录枚举，按 AssetId 排序。
3. 使用 `FindByExtension()` 分类，统计 unsupported。
4. 先为所有 supported 文件 upsert manifest/registry，已有 AssetId 复用 GUID。
5. 保存 manifest。
6. 按 Texture2D、Mesh、Material 排序 GUID，调用 Force import。
7. 对 `ImportedAssetGuids` 调用 RuntimeCache::Invalidate。
8. 填充 report 和 ResultEnvelope payload。

同时增加只读查询：

```cpp
[[nodiscard]] bool CanImportSource(const std::filesystem::path& sourcePath) const;
```

它仅通过 importer registry 推断扩展名，不访问或修改磁盘状态。

- [ ] **Step 6: 暴露 ApplicationOperations 接口**

```cpp
[[nodiscard]] ResultEnvelope ReimportAssets(
    const ProjectContext& context,
    const std::filesystem::path& targetPath,
    AssetReimportReport* outReport = nullptr) const;

[[nodiscard]] bool CanImportAssetSource(const std::filesystem::path& sourcePath) const;
```

两个接口转发到 AssetService；Reimport 统一 operation id 为 `asset.reimport` 并注册到 OperationRegistry。

- [ ] **Step 7: 运行资产与应用 smoke**

```powershell
cmake --build build --config Debug --target AssetImportSmoke ApplicationOperationsSmoke ValidationServiceSmoke --parallel 8
& build/bin/Debug-Windows-x64/smoke/AssetImportSmoke.exe
& build/bin/Debug-Windows-x64/smoke/ApplicationOperationsSmoke.exe
& build/bin/Debug-Windows-x64/smoke/ValidationServiceSmoke.exe
```

Expected: 三个 smoke 均 passed。

- [ ] **Step 8: 提交**

```powershell
git add HuaEngine/src/HuaEngine/Asset/AssetService.* HuaEngine/src/HuaEngine/Application/ApplicationOperations.* Tests/AssetImportSmoke.cpp Tests/ApplicationOperationsSmoke.cpp
git commit -m "feat(asset): reimport files and directories"
```

---

### Task 4: ProjectPanel 右键菜单与 Editor 编排

**Files:**
- Modify: `Editor/src/Panels/ProjectPanel.h`
- Modify: `Editor/src/Panels/ProjectPanel.cpp`
- Modify: `Editor/src/EditorLayer.h`
- Modify: `Editor/src/EditorLayer.cpp`
- Create: `Tests/ProjectPanelActionSmoke.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ApplicationOperations::ReimportAssets()`
- Produces: `ProjectPanelActionType::ReimportPath`、`ProjectPanelActionType::ReimportAll`
- Preserves: scene 单击/双击打开行为和现有 Refresh Project 行为

- [ ] **Step 1: 写 action 语义失败测试**

将 action 构造提取成不依赖 ImGui 的静态函数：

```cpp
const auto fileAction = MakeProjectReimportAction(filePath, false);
Require(fileAction.Type == HE::ProjectPanelActionType::ReimportPath, "Expected file reimport action");
Require(fileAction.Path == filePath, "Expected file target path");
const auto allAction = MakeProjectReimportAction({}, true);
Require(allAction.Type == HE::ProjectPanelActionType::ReimportAll, "Expected reimport all action");
```

测试函数只验证 action 数据契约，不模拟 ImGui。

- [ ] **Step 2: 运行测试确认红灯**

```powershell
cmake --build build --config Debug --target ProjectPanelActionSmoke --parallel 8
```

Expected: 编译失败，新 action 类型/构造函数不存在。

- [ ] **Step 3: 增加右键菜单**

- 文件 `Selectable` 后调用 `BeginPopupContextItem()`；支持格式触发 `ReimportPath`，不支持格式显示 disabled。
- 目录 `TreeNode` 后调用独立 popup；触发 `ReimportPath`。
- `Assets` header popup 和 Assets 区域空白 popup 触发 `ReimportAll`。
- popup ID 使用完整规范化路径，避免同名文件/目录冲突。
- `ProjectPanel::SetCanReimportCallback()` 接收查询 callback；EditorLayer 将其绑定到 `ApplicationOperations::CanImportAssetSource()`，UI 不复制扩展名表。

- [ ] **Step 4: EditorLayer 处理 action**

增加：

```cpp
void ReimportProjectAssets(const std::filesystem::path& targetPath);
```

`ReimportPath` 传 action.Path；`ReimportAll` 传当前 AssetRoot。执行后调用 `CaptureOperationResult()`、`RefreshInspectorAssetCatalog()`、`RefreshWorkbenchValidation()`。

- [ ] **Step 5: 构建并运行回归 smoke**

```powershell
cmake --build build --config Debug --target Editor ProjectPanelActionSmoke EditorAssetPickerSmoke ApplicationOperationsSmoke --parallel 8
& build/bin/Debug-Windows-x64/smoke/ProjectPanelActionSmoke.exe
& build/bin/Debug-Windows-x64/smoke/EditorAssetPickerSmoke.exe
& build/bin/Debug-Windows-x64/smoke/ApplicationOperationsSmoke.exe
git diff --check
```

Expected: Editor 构建成功，三个 smoke passed，diff check 无错误。

- [ ] **Step 6: 手工交互验证**

启动：

```powershell
build/bin/Debug-Windows-x64/Editor.exe --project Tests/TestProj
```

验证文件、文件夹、Assets 根节点三个右键入口；确认新 `.mesh` Reimport 后无需重启即可出现在 Inspector Mesh 选择器。不支持 `.obj` 的菜单项必须禁用。

- [ ] **Step 7: 独立代码审查并修正 findings**

审查重点：路径越界、GUID 稳定、旧产物保留、缓存失效时机、ProjectPanel popup ID、未跟踪 `Tests/TestProj/Assets/models/` 不进入提交。

- [ ] **Step 8: 提交**

```powershell
git add Editor/src/Panels/ProjectPanel.* Editor/src/EditorLayer.* Tests/ProjectPanelActionSmoke.cpp CMakeLists.txt
git commit -m "feat(editor): reimport assets from project panel"
```
