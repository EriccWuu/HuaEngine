# Editor 资产 Reimport 设计

## 状态

- 日期：2026-08-22
- 状态：已确认，待实现
- 范围：Project 面板文件/文件夹右键 Reimport、Reimport All，以及可复用的资产系统强制重导能力

## 背景

当前资产管线已经具备以下基础能力：

- `AssetManifest` 维护资产 GUID、AssetId、类型和源文件信息。
- `AssetLibrary` 保存导入后的运行时二进制产物。
- `AssetImporterRegistry` 注册 Mesh、Material 和 PNG Texture importer。
- `AssetImportService::ImportMissingAssets()` 在工程启动时补齐缺失产物。
- `ApplicationOperations::ImportAsset()` 可以通过 CLI 注册并导入单个资产。

但 Editor 的 Project 面板目前只是文件浏览器。将文件复制到 `Assets/` 后，用户仍需调用 CLI；已有 Library 产物也会被 `ImportMissingAssets()` 跳过，无法强制重导。Inspector Mesh 选择器因此只能看到已经注册到 manifest 的资产。

## 目标

1. 普通文件右键支持 `Reimport`。
2. 文件夹右键支持递归 `Reimport`。
3. `Assets` 根节点和 Project 面板空白区域支持 `Reimport All`。
4. 尚未注册的受支持文件在 Reimport 时自动注册并生成 GUID。
5. 已注册资产重导时保持 GUID 不变，强制覆盖 Library 产物。
6. 重导成功后使对应运行时缓存失效，后续解析得到新对象。
7. 资产系统提供与 Editor 解耦的批量 Reimport 接口，供 CLI、文件监控等后续能力复用。
8. 批量操作产生结构化汇总和逐文件诊断，接入现有 Console/Validation 流程。

## 非目标

- 不实现文件变化监听和自动重导。
- 不实现 OBJ、FBX、TGA、MTL importer。
- 不实现导入设置窗口、缩略图和资产预览。
- 不删除源文件消失后遗留的 manifest 或 Library 记录。
- 不保证当前帧已经持有的旧运行时对象立即销毁。
- 不实现跨文件事务回滚；批量操作采用逐文件提交和汇总报告。

## 用户交互

### 文件

- 右键受支持文件时显示可用的 `Reimport`。
- 右键不支持文件时显示禁用的 `Reimport`，tooltip 说明当前没有匹配 importer。
- `.scene`、shader 和其他非资产源文件不会进入导入管线。

### 文件夹

- 任意 `Assets/` 子文件夹右键显示 `Reimport`。
- 操作递归扫描该目录下的普通文件。
- 支持格式进入注册和导入流程，不支持格式计入 skipped，不产生逐文件警告刷屏。

### Reimport All

- `Assets` 根节点右键显示 `Reimport All`。
- Project 面板 Assets 区域空白处右键也显示 `Reimport All`。
- 目标路径为整个 `{ProjectRoot}/Assets`。

### 操作反馈

- 不弹出阻塞式完成窗口。
- 结果通过现有 `ResultEnvelope` 进入 Console 和 Validation。
- 汇总至少包含：`scanned_files`、`supported_files`、`registered_assets`、`reimported_assets`、`skipped_files`、`failed_assets`。
- 操作结束后立即刷新 Inspector 资产目录；Project 文件树本身每帧读取磁盘，无需缓存刷新。

## 支持格式与类型推断

首版映射如下：

| 扩展名 | AssetKind | Importer |
| --- | --- | --- |
| `.mesh` | Mesh | `MeshAssetImporter` |
| `.material`、`.mat` | Material | `MaterialAssetImporter` |
| `.png` | Texture2D | `PngTextureImporter` |

扩展名匹配采用 ASCII 大小写不敏感规则。类型推断由资产模块提供，Editor 不维护第二份扩展名表。

未来增加 OBJ 或 FBX importer 后，只需向 importer registry 注册对应 importer 和类型映射，Project 面板无需修改。

## 公共接口

在资产和应用操作层增加批量接口：

```cpp
struct AssetReimportReport {
    uint32_t ScannedFiles = 0;
    uint32_t SupportedFiles = 0;
    uint32_t RegisteredAssets = 0;
    uint32_t ReimportedAssets = 0;
    uint32_t SkippedFiles = 0;
    uint32_t FailedAssets = 0;
};

ResultEnvelope ReimportAssets(
    const ProjectContext& context,
    const std::filesystem::path& targetPath,
    AssetReimportReport* outReport = nullptr);
```

`targetPath` 可以是 `Assets` 内的文件、子目录或 `Assets` 根目录。接口负责规范化和边界校验，不信任 Editor 传入的路径。

## 资产系统处理流程

### 1. 路径校验

- 工程必须已加载。
- 将项目 AssetRoot 和目标路径解析为绝对、词法规范化路径。
- 目标必须等于 AssetRoot 或位于 AssetRoot 内部。
- 目标不存在、不是普通文件/目录或路径越界时整体失败，不修改 manifest 和 Library。

### 2. 扫描与分类

- 文件目标只产生一个候选项。
- 文件夹目标递归枚举普通文件。
- 候选项按规范化 AssetId 稳定排序，确保报告和测试可重复。
- 通过 importer registry 推断 `AssetKind`；不支持文件计入 skipped。
- 单文件目标若不支持，返回 `ManualIntervention`，而不是无提示成功。

### 3. 第一阶段：注册

在导入任何产物之前，先注册本次扫描出的全部受支持文件：

- manifest 已有相同 AssetId：复用原 GUID，并校验类型一致。
- manifest 尚无记录：生成 GUID，写入 file-source manifest 记录。
- GUID 或 AssetId 冲突计为该资产失败。
- 所有成功注册的记录先写入 manifest 和 registry。

两阶段设计保证同一批次中新 Material 能通过 AssetId 找到同批次中新 Texture。

### 4. 第二阶段：强制导入

- 不调用 `IsArtifactAvailable()` 的跳过分支。
- 每个记录直接调用匹配 importer。
- 导入顺序为 Texture2D、Mesh、Material；同类型内部按 AssetId 排序。
- importer 成功后调用 `AssetLibrary::CommitArtifact()` 原子覆盖对应产物。
- 批次结束后统一保存 `AssetLibrary.bin`。
- importer 失败时保留该 GUID 原有的最后可用 Library 产物，不删除旧文件。
- 单个资产失败不会阻止其他候选项继续处理。

### 5. 运行时缓存失效

`AssetRuntimeCache` 增加按 GUID 失效接口。仅在新产物提交成功后失效：

```cpp
void Invalidate(const AssetGuid& guid);
```

该接口清除 Mesh、Material 和 Texture 三类缓存中的同 GUID 条目。已被当前帧或其他对象持有的 `Ref` 继续存活；后续 `AssetResolver` 查询会从新 Library 产物重建运行时对象。

重导失败时不清缓存，继续使用最后可用对象。

## AssetImportService 重构

避免复制 importer 调用和 Library commit 逻辑：

- 提取单记录导入函数，参数明确表达 `MissingOnly` 或 `Force` 策略。
- `ImportMissingAssets()` 保持现有启动语义，只补缺失产物。
- `ReimportAssets()` 使用 `Force` 策略。
- Library 保存由批次入口负责，单记录函数不反复写 catalog。

策略必须使用强类型枚举，不使用含义不清的布尔参数。

## Editor 接入

`ProjectPanelActionType` 增加：

- `ReimportPath`
- `ReimportAll`

`ProjectPanelAction::Path` 对文件和文件夹保存绝对目标路径。`ReimportAll` 的目标由 EditorLayer 使用当前项目 AssetRoot 生成，避免 UI 自行推导工程上下文。

`EditorLayer` 收到 action 后：

1. 调用 `ApplicationOperations::ReimportAssets()`。
2. 使用 `CaptureOperationResult()` 记录结果。
3. 无论部分成功还是全部成功，都刷新 Inspector 资产目录。
4. 刷新 Workbench Validation。
5. 基础设施级失败时保留旧目录和运行时缓存状态。

## 错误与部分成功语义

- 路径越界、目标不存在、manifest 无法保存、Library catalog 无法保存：`Failed`。
- 单个 importer 失败、类型冲突、单个产物提交失败：继续批次，增加 `failed_assets` 并附加诊断。
- 批次存在单资产失败时返回可被 Console 明确展示的部分成功摘要，不隐藏失败计数。
- 不支持格式：文件夹批量操作计入 skipped；单文件显式操作返回 `ManualIntervention`。
- 新资产注册成功但导入失败时保留 manifest 记录，便于修复源文件后再次 Reimport。

## 测试策略

### 资产层

1. 已导入资产强制重导后 GUID 不变，Library 产物内容更新。
2. Library 产物已存在时仍调用 importer，不计入 skipped。
3. 未注册 `.mesh`、`.material/.mat`、`.png` 通过 Reimport 自动注册。
4. 文件夹递归扫描支持文件并跳过不支持文件。
5. Reimport All 覆盖完整 AssetRoot。
6. 目标不存在和越过 AssetRoot 的路径被拒绝。
7. importer 失败时旧 Library 产物仍可读取。
8. 成功后对应 RuntimeCache 条目失效；失败时缓存保留。
9. 批次中的 Texture 在 Material 之前完成注册和导入。

### Editor 层

1. 文件右键产生 `ReimportPath` action。
2. 文件夹右键产生 `ReimportPath` action。
3. Assets 根节点和空白区域产生 `ReimportAll` action。
4. 不支持文件不能触发有效 Reimport。
5. 操作后 Inspector 资产列表刷新。

### 回归验证

- 构建 `Editor`。
- 运行新增 Reimport smoke。
- 运行 `AssetImportSmoke`、`ApplicationOperationsSmoke`、`EditorAssetPickerSmoke` 和 ProjectPanel 相关 smoke。
- 执行 `git diff --check`。

## 验收标准

1. 用户把受支持文件放进 `Assets/` 后，无需 CLI 即可通过右键 Reimport 导入。
2. 文件夹 Reimport 和 Reimport All 能递归处理受支持资产。
3. 重导不改变已存在资产 GUID，场景引用保持有效。
4. 修改源文件后 Reimport 会覆盖 Library 产物，而不是因为产物存在被跳过。
5. 重导成功后 Inspector 立即出现新 Mesh；运行时后续解析使用新对象。
6. 不支持文件不会进入 manifest，也不会导致批量操作整体失败。
7. 路径越界无法修改项目外文件或 Library 状态。
8. 单资产失败不会破坏其旧的可用产物，也不会阻止同批次其他资产导入。
