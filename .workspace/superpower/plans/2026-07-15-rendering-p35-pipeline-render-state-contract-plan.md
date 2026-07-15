# P35 PipelineState Render State Contract 计划

状态：执行中
日期：2026-07-15

## 目标

补齐 `PipelineStateDesc` 的 render state contract，让 pipeline 不再只描述 shader、vertex layout、topology 和 bind group layout。

## 本阶段范围

- 新增 color target state，至少声明 color attachment format、blend 开关和写 mask。
- 新增 depth/stencil state，至少声明 depth/stencil attachment format、depth test/write 和 compare op。
- 新增 raster state，至少声明 cull mode、front face、fill mode。
- OpenGL backend 在 pipeline 创建时校验明显非法的 render state contract。
- smoke 覆盖默认值、显式 contract round-trip、非法 format 拒绝。

## 非目标

- 不实现完整 blend factor/op 到 OpenGL 状态应用。
- 不实现 render pass active target 与 pipeline format 的 draw-time mismatch 检查。
- 不引入 shader reflection。
- 不重构 render target/resource model。

## 验收标准

- `PipelineStateDesc` 包含 `ColorTargets`、`DepthStencil`、`Raster`。
- 旧的 pipeline 创建路径保持可用，并拥有兼容默认 render state。
- 显式 color/depth/raster contract 能从 `PipelineState::GetDesc()` round-trip。
- `CreatePipelineState` 拒绝 `ColorTargets` 为空、color target format 为 `None` 或 depth format、depth/stencil format 为 color format 的描述。
- `RHIResourceCreationSmoke` 覆盖上述行为。

## 执行结果

- `PipelineState.h` 已新增 `ColorTargetState`、`DepthStencilState`、`RasterState`。
- `PipelineStateDesc` 已新增 `ColorTargets`、`DepthStencil`、`Raster`，默认值保持旧 pipeline 创建路径可用。
- OpenGL `CreatePipelineState` 已增加 render state contract 校验。
- `RHIResourceCreationSmoke` 已覆盖默认 contract、显式 contract round-trip、非法 color/depth format 拒绝。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
