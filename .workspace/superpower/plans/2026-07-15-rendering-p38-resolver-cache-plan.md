# P38 Pipeline / BindGroup Cache 计划

状态：执行中
日期：2026-07-15

## 目标

减少 Forward 主路径中每个 render item 重复创建标准 bind group layout 和 pipeline state 的问题，让 resolver 开始具备可观测的 cache 行为。

## 本阶段范围

- `RenderStats` 增加 cache hit/miss 计数。
- `RenderResourceResolver` 缓存 frame/object 标准 bind group layout。
- `RenderResourceResolver` 缓存 pipeline state。
- pipeline cache key 使用：
  - shader program ref
  - vertex layout entries
  - material bind group layout entries/signature
- material bind group 仍逐 item 创建，保留 override 语义。
- `RenderingOperationsSmoke` 在多 render item 场景验证 cache hit。

## 非目标

- 不实现 material bind group cache。
- 不实现 descriptor pool/descriptor allocator。
- 不改变 draw sorting。
- 不引入 shader reflection。
- 不跨 renderer/global 缓存 pipeline state。

## 验收标准

- 多 item render 后 `RenderStats.BindGroupLayoutCacheHits > 0`。
- 多 item render 后 `RenderStats.PipelineStateCacheHits > 0`。
- fallback 和 material override 路径继续可用。
- 四个 smoke 通过，提交为单独 P38 commit。

## 执行结果

- `RenderStats` 已新增 bind group layout / pipeline state cache hit/miss 计数。
- `RenderResourceResolver` 已缓存 frame/object 标准 bind group layout。
- `RenderResourceResolver` 已缓存 pipeline state。
- pipeline cache key 使用 shader ref、vertex layout 和 material layout entries/signature。
- material bind group 仍逐 item 创建，保留 override 语义。
- `RenderingOperationsSmoke` 已验证多 item render 产生 layout cache hit 和 pipeline cache hit。

## 验证结果

- `RenderingOperationsSmoke passed`
- `RHICommandListBindingSmoke passed`
- `RHIResourceCreationSmoke passed`
- `RenderPassGraphSmoke passed`
- `git diff --check` exit 0
