# HLSL、SPIR-V 与材质管线审查修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 修复 `ba74465..8fd2e4c` 审查发现的 8 个 MAJOR 与 2 个 MINOR，使 HLSL、SPIR-V、ShaderInterface、OpenGL UBO、Asset DAG 和材质 Inspector 满足原 Spec 的完成定义。

**架构：** 修复按契约边界拆为五个可独立验证阶段。Shader 导入阶段负责严格类型和 Artifact contract；RHI 使用后端中立 stage binary 与完整 GPU interface；OpenGL 只负责消费 OpenGL stage 和 native resource map；Asset Library 以 payload hash 和原子 catalog 发布实现 last-good；Editor 通过只读 import health 与 MaterialDefinition 呈现状态。

**技术栈：** C++20、DXC、SPIRV-Cross、glslang、OpenGL 3.3、ImGui、自定义 Asset Library/Smoke 测试。

**Spec：** `.workspace/superpower/specs/2026-08-24-hlsl-spirv-shader-material-pipeline-spec.md`

## 全局约束

- 短期运行后端仍是 OpenGL，生成目标固定为 GLSL 330。
- HLSL 使用仓库固定 DXC，SPIR-V target environment 固定为 Vulkan 1.2。
- Frame、Material、Object 分别使用 set/space 0、1、2。
- 每个阶段先添加失败测试，确认 RED 后再修改生产代码。
- 每个阶段完成后独立提交一次；不得把 `.tasks/` 审查产物混入源码提交。
- 代码注释仅使用通俗英文。

---

### R1：严格 ShaderInterface、SPIR-V 类型和 Artifact V2 contract

**文件：**
- 修改：`HuaEngine/src/HuaEngine/Rendering/Shader/ShaderInterface.h`
- 修改：`HuaEngine/src/HuaEngine/Rendering/Shader/ShaderInterface.cpp`
- 修改：`HuaEngine/src/HuaEngine/Rendering/Shader/SpirvShaderReflector.cpp`
- 修改：`HuaEngine/src/HuaEngine/Asset/Artifact/ShaderArtifact.h`
- 修改：`HuaEngine/src/HuaEngine/Asset/Artifact/ShaderArtifact.cpp`
- 修改：`HuaEngine/src/HuaEngine/Asset/Import/HlslShaderImporter.cpp`
- 测试：`Tests/ShaderInterfaceSmoke.cpp`
- 测试：`Tests/AssetImportSmoke.cpp`

**接口：**
- 产出：`ResultEnvelope ValidateShaderGpuInterface(const ShaderGpuInterface&)`。
- 产出：`ResultEnvelope ValidateShaderArtifactV2Contract(const ShaderArtifactDataV2&)`。
- 约束：首版只接受 `int`、`float`、`float2/3/4`、`float4x4`、`Texture2D`、`SamplerState`；拒绝数组、非 32-bit scalar、整数向量和非 4x4 矩阵。
- 约束：Artifact 必须恰好包含唯一 Vertex 和 Fragment stage，SPIR-V magic 正确，generated GLSL 非空，枚举和 StageMask 合法，重新 Finalize 后 digest 完全一致。

- [x] **步骤 1：添加 Artifact 损坏与不支持 SPIR-V 类型测试**

  在 `ShaderInterfaceSmoke` 中构造重复 Vertex、空 GLSL、越界枚举和不支持 constant member type；使用临时 HLSL 编译 `int2`、`float3x4`、数组，断言反射或导入返回稳定失败 diagnostic。

- [x] **步骤 2：运行测试确认 RED**

  运行：`cmake --build build --config Debug --target ShaderInterfaceSmoke && build/bin/Debug-Windows-x64/smoke/ShaderInterfaceSmoke.exe`

  预期：新增断言因当前静默降级或解码放行而失败。

- [x] **步骤 3：实现统一 Shader contract 校验**

  `ValidateShaderGpuInterface()` 检查所有枚举、stage 唯一性、location、resource set/binding/type/StageMask、constant member 类型/offset/size/matrix stride；`FinalizeShaderInterface()` 先调用结构校验再计算 canonical digest。

- [x] **步骤 4：让反射类型解析显式失败**

  将 `ValueType()` 改为返回 `ResultEnvelope + outType`，完整检查 SPIR-V scalar width/sign、vector element/count、matrix rows/columns、array count；禁止类型直接终止导入，不再回退为 Float。

- [x] **步骤 5：统一 Artifact encode/decode/candidate contract**

  encode 和 decode 都调用 `ValidateShaderArtifactV2Contract()`；decode 在任何 `static_cast` 前检查范围，拒绝重复 stage、空 GLSL、非法 digest 和 trailing data。

- [x] **步骤 6：统一 include roots**

  从 descriptor source root、HLSL parent 和 project/builtin asset root 构造同一份有序规范化 roots；依赖收集和 DXC compile request 共用该列表，递归收集实际 include 输入并检测 include 环。

- [x] **步骤 7：验证并提交 R1**

  运行：`ShaderInterfaceSmoke.exe`、`AssetImportSmoke.exe`。

  提交：`fix(shader): enforce SPIR-V and artifact contracts`

### R2：后端中立 ShaderProgram 与 Pipeline layout 校验

**文件：**
- 修改：`HuaEngine/src/HuaEngine/Rendering/RHI/ShaderProgram.h`
- 修改：`HuaEngine/src/HuaEngine/Rendering/RHI/PipelineState.h`
- 修改：`HuaEngine/src/HuaEngine/Asset/AssetResolver.cpp`
- 修改：`HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
- 修改：`HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
- 修改：`HuaEngine/src/HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.cpp`
- 测试：`Tests/RHIResourceCreationSmoke.cpp`
- 测试：`Tests/RHICommandListBindingSmoke.cpp`
- 测试：`Tests/RenderingOperationsSmoke.cpp`

**接口：**
- 产出：`ShaderStageCodeFormat { OpenGlGlsl, SpirV, Dxil }`。
- 产出：`ShaderStageBinary { ShaderStage Stage; ShaderStageCodeFormat Format; std::string EntryPoint; std::vector<uint8_t> Code; }`。
- 产出：`ShaderResourceMap`，记录 logical set/binding 到 native binding，以及 OpenGL combined sampler 映射。
- 变更：`ShaderProgramDesc { std::vector<ShaderStageBinary> Stages; ShaderGpuInterface Interface; ShaderResourceMap ResourceMap; }`。

- [x] **步骤 1：添加错误 Pipeline layout 拒绝测试**

  覆盖 slot 缺失、set/binding 错误、binding type 错误、visibility 错误、buffer size 不足和 interface digest 不匹配；断言 `CreateGraphicsPipeline()` 返回空。

- [x] **步骤 2：运行 RHI smoke 确认 RED**

  运行：`RHIResourceCreationSmoke.exe` 与 `RHICommandListBindingSmoke.exe`。

- [x] **步骤 3：替换 ShaderProgramDesc**

  AssetResolver 将 generated GLSL 编码为 `OpenGlGlsl` stage bytes，并携带完整 `ShaderGpuInterface` 与映射；OpenGL backend 解码自身支持的 stage format，公共 RHI 不再暴露 VertexSource/FragmentSource。

- [x] **步骤 4：实现 ShaderInterface 到 BindGroupLayout 的一致性校验**

  Pipeline 创建时按 scope/set 投影 Shader interface，逐 slot 比较 layout entry 的 binding、类型、visibility、minimum buffer size，并比较完整 SHA-256 digest；64-bit signature 只作为快速索引。

- [x] **步骤 5：迁移 Forward 和测试调用点**

  删除从扁平 GLSL uniform 描述推导 contract 的路径；RenderResourceResolver 从 ShaderProgram interface/resource map 获取 block 和 texture binding。

- [x] **步骤 6：验证并提交 R2**

  运行：`RHIResourceCreationSmoke.exe`、`RHICommandListBindingSmoke.exe`、`RenderingOperationsSmoke.exe`。

  提交：`refactor(rhi): make shader programs backend neutral`

### R3：OpenGL fence 失败时保证 UBO Arena 安全

**文件：**
- 修改：`HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.h`
- 修改：`HuaEngine/src/Platform/OpenGL/RHI/OpenGLRenderDevice.cpp`
- 修改：`HuaEngine/src/HuaEngine/Rendering/RenderPipeline/UniformBufferArena.h`
- 修改：`HuaEngine/src/HuaEngine/Rendering/RenderPipeline/UniformBufferArena.cpp`
- 测试：`Tests/RHIResourceCreationSmoke.cpp`
- 测试：`Tests/RenderingOperationsSmoke.cpp`

**接口：**
- 产出：`OpenGLFence::SignalCompleted(uint64_t)`，用于 GPU 已同步完成但没有 GLsync handle 的提交。
- 约束：`glFenceSync` 失败后执行明确同步退路，再发布 completed timeline value；不得返回一个会让 Arena 丢失已 replay 使用区间的无 fence 失败状态。

- [x] **步骤 1：添加 completed signal 与 Arena 回收测试**

  断言 signal 未完成前 Arena 不复用；同步完成信号发布后才能 reset；重复和倒退 signal 被拒绝或忽略。

- [x] **步骤 2：运行测试确认 RED**

  运行：`RHIResourceCreationSmoke.exe`。

- [x] **步骤 3：实现安全 fallback**

  OpenGL submit 在 `glFenceSync == nullptr` 时调用 `glFinish()`，通过 `SignalCompleted(signalValue)` 发布已完成 timeline，然后返回成功提交；记录稳定 diagnostic/log。

- [x] **步骤 4：验证并提交 R3**

  运行：`RHIResourceCreationSmoke.exe`、`RenderingOperationsSmoke.exe`。

  提交：`fix(opengl): preserve uniform arena safety on fence failure`

### R4：内容寻址 last-good 事务与通用 Import DAG

**文件：**
- 修改：`HuaEngine/src/HuaEngine/Asset/Library/AssetLibrary.h`
- 修改：`HuaEngine/src/HuaEngine/Asset/Library/AssetLibrary.cpp`
- 修改：`HuaEngine/src/HuaEngine/Asset/Import/AssetImporter.h`
- 修改：`HuaEngine/src/HuaEngine/Asset/Import/MaterialAssetImporter.h`
- 修改：`HuaEngine/src/HuaEngine/Asset/Import/MaterialAssetImporter.cpp`
- 修改：`HuaEngine/src/HuaEngine/Asset/Import/AssetImportService.h`
- 修改：`HuaEngine/src/HuaEngine/Asset/Import/AssetImportService.cpp`
- 修改：`HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- 测试：`Tests/AssetLibrarySmoke.cpp`
- 测试：`Tests/AssetImportSmoke.cpp`

**接口：**
- 产出：`AssetImporter::CollectDependencies(const AssetImportContext&, std::vector<AssetGuid>&)`，默认无依赖。
- 产出：Material importer 从 source 收集 Shader GUID 与 Texture GUID。
- 变更：Artifact 路径使用完整二进制 Artifact payload SHA-256，而不是 import fingerprint。
- 变更：`CommitArtifact()` 先写候选、语义解码、构造 candidate catalog、原子保存 catalog，最后发布内存 records；失败时保持旧 record 并清理新候选。

- [x] **步骤 1：添加 last-good 失败测试**

  覆盖同 fingerprint 强制重导产生不同 payload、Shader candidate 语义解码失败、catalog 路径不可写；断言旧 catalog、旧内存 record 和旧 Artifact 均保持可读。

- [x] **步骤 2：添加通用 DAG 测试**

  使用测试 importer 构造 A -> B -> C 和 A -> B -> A；断言拓扑顺序稳定，环路在任何 commit 前失败，diagnostic 同时包含 GUID 与 source path 链。

- [x] **步骤 3：运行 Asset smoke 确认 RED**

  运行：`AssetLibrarySmoke.exe`、`AssetImportSmoke.exe`。

- [x] **步骤 4：实现内容寻址原子发布**

  对完整 Artifact binary 计算 SHA-256；候选读回后按 kind 调用对应 decoder，Shader 必须走 R1 contract；catalog 保存成功后才替换 `m_Records`。

- [x] **步骤 5：实现导入计划与拓扑排序**

  在 import 前解析请求闭包、收集 dependencies、验证 manifest/importer/source，使用 DFS 三色标记生成拓扑序并输出环链；计划验证成功后才开始现有逐资产 import/commit。

- [x] **步骤 6：删除 AssetService 的特例前置导入**

  Reimport 统一调用 DAG planner，不再手工先导 Shader 再反查 Material；Shader 成功后 dependent Material 按 Library dependency 进入计划。

- [x] **步骤 7：验证并提交 R4**

  运行：`AssetLibrarySmoke.exe`、`AssetImportSmoke.exe`、`AssetServiceSmoke.exe`。

  提交：`fix(assets): make imports transactional and dependency ordered`

### R5：Inspector range、override 提示与 last-good health

**文件：**
- 修改：`HuaEngine/src/HuaEngine/Asset/AssetService.h`
- 修改：`HuaEngine/src/HuaEngine/Asset/AssetService.cpp`
- 修改：`Editor/src/Panels/RuntimeInspector.h`
- 修改：`Editor/src/Panels/RuntimeInspector.cpp`
- 修改：`Editor/src/Panels/InspectorPanel.h`
- 修改：`Editor/src/Panels/InspectorPanel.cpp`
- 测试：`Tests/EditorInspectorRuntimeSmoke.cpp`
- 测试：`Tests/EditorInteractionSmoke.cpp`

**接口：**
- 产出：`AssetImportHealth { Current, LastGoodWithFailure, Missing, Stale }` 与最近一次 diagnostics。
- 变更：`GetMaterialDefinition()` 可同时返回 definition 和只读 health；last-good 仍可显示参数。
- 约束：Range 为两个值时传给数值控件 min/max；Material 切换删除不兼容 override 时发布一次性用户可见 diagnostic。

- [x] **步骤 1：添加 editor model 测试**

  覆盖 Range/Step 映射、last-good-with-failure 仍返回 definition、Missing/Stale 区分，以及 reconcile 删除 override 后只产生一次提示。

- [x] **步骤 2：运行 Editor smoke 确认 RED**

  运行：`EditorInspectorRuntimeSmoke.exe`、`EditorInteractionSmoke.exe`。

- [x] **步骤 3：实现 import health 与 Inspector 状态**

  AssetService 在 import/reimport 后按 GUID 更新 session health；成功清除旧失败，失败且 Library 有可用 Artifact 时标记 LastGoodWithFailure。Inspector 保留参数控件并显示最近失败摘要。

- [x] **步骤 4：应用 Range 并发布 override 删除提示**

  数值 widget 使用 definition 的 min/max/step；Editor command 返回 reconcile 结果，InspectorPanel 通过现有 diagnostics/notification 通道发布一次性消息。

- [x] **步骤 5：全量验证、更新 Spec 状态并提交 R5**

  运行：构建 `HuaEngine`、`Editor` 和全部 smoke target；逐个执行新增及受影响 smoke；执行 `git diff --check`。

  只有全部验证通过后，才修正 Spec 的阶段状态与完成说明。

  提交：`fix(editor): surface material import health and ranges`

## 最终验收

- 所有审查 MAJOR 均有直接回归测试。
- Shader Artifact 损坏或不支持类型在导入阶段稳定失败。
- 公共 RHI 不包含 VertexSource/FragmentSource 等 OpenGL 专用字段。
- Pipeline layout 与完整 ShaderGpuInterface 不一致时创建失败。
- GLsync 失败不会导致 UBO 区间在 GPU 完成前复用。
- 导入环在任何 Artifact commit 前失败，last-good catalog 与内存状态一致。
- Inspector 能区分 current、last-good compile failure、missing、stale，并正确应用 Range。
- Debug 构建、受影响 smoke 和 `git diff --check` 全部通过。
