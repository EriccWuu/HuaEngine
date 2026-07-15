# P44 Material / BindGroup Layout Schema 稳定化计划

状态：已完成
日期：2026-07-16
基线：`fd71ecc feat(rendering): apply pipeline render state`

## 目标

把 material bind group layout 从“按当前 material instance entries 临时拼装”推进为“按 base material schema 稳定生成”，为 descriptor/layout cache 和 pipeline cache 提供稳定 key。

## 本阶段范围

- 新增 `MaterialBindingSchema` 与 `MaterialBindingSchemaEntry`。
- `Material` 提供 `GetBindingSchema()`，schema 由 base material 参数派生，按参数名稳定排序。
- schema entry 包含：
  - parameter name
  - parameter type
  - binding
  - texture slot
- `CreateMaterialBindGroup()` 基于 schema entries 生成 bind group entries。
- `RenderResourceResolver` 缓存 material bind group layout，cache key 使用 schema signature。
- pipeline cache entry 改用 material schema signature，而不是临时 layout entries。

## 非目标

- 不改 material 序列化格式。
- 不引入 shader reflection。
- 不迁移 material texture value 到 `TextureView/Sampler` schema。
- 不实现 stage visibility 的跨 backend 语义；本阶段只预留稳定 schema 位置。

## RED 测试

在 `RenderingOperationsSmoke` 中增强多 item render 验收：

- 同一 base material 的多个 material instance override 不改变 schema signature。
- 多 item render 能复用 material bind group layout cache。
- pipeline cache hit 基于 schema signature 保持稳定。

## 验收标准

- base material schema signature 稳定，不受 instance override value 影响。
- material bind group layout 能被 resolver 缓存复用。
- pipeline cache key 使用 schema signature。
- 既有四个 smoke 通过。
- P44 单独提交。

## 执行结果

- 新增 `MaterialBindingSchema` 与 `MaterialBindingSchemaEntry`。
- `Material::GetBindingSchema()` 已按参数名稳定排序，并生成不受 instance override value 影响的 signature。
- `RenderBindGroupBuilder` 已支持从 schema 创建 material bind group layout，并可复用外部传入的 cached layout 创建 bind group。
- `RenderResourceResolver` 已新增 material bind group layout cache，cache key 使用 schema signature。
- pipeline cache entry 已改为使用 material schema signature，而不是临时 layout entries。
- `RHIResourceCreationSmoke` 已覆盖 schema 排序、binding 编号、signature 存在，以及 instance override 不改变 base schema signature。

## 验证结果

- `RenderingOperationsSmoke` passed
- `RHICommandListBindingSmoke` passed
- `RHIResourceCreationSmoke` passed
- `RenderPassGraphSmoke` passed
- `git diff --check` passed
