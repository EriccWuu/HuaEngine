# 相机数据与控制分离规格

## 目标

将渲染消费的相机快照、ECS 场景相机数据和编辑器相机控制状态分离。

## P1：ECS 相机数据化

- 新增纯 `RenderCamera`，仅携带 view/projection 矩阵。
- `CameraComponent` 保存可序列化的投影参数，不再持有 `RuntimeCamera`。
- `CameraSystem` 由 `TransformComponent + CameraComponent` 构建 `RenderCamera`，`RenderSystem` 只消费该快照。
- 视口渲染入口只消费 `RenderCamera`。

已完成。`CameraSystem` 查询 `TransformComponent + CameraComponent` 并为首个 Primary 相机生成渲染快照；`RenderSystem` 在同一 Render 阶段后置消费该快照。

`CameraSystem` 仅接收渲染视口宽高，不持有 RHI `RenderTarget`；尺寸由视口宿主在附加渲染器时同步。

## P2：编辑器控制器化

- 将 `EditorCamera` 改为不继承渲染相机数据的控制器。
- 控制器根据编辑器状态和视口尺寸输出 `RenderCamera`。
- Editor 与 smoke 改为使用控制器输出的相机快照。

已完成。`EditorCameraController` 不再继承渲染相机，`BuildRenderCamera()` 是它与渲染层的唯一数据交界。

已完成迁移：`EditorCameraController` 位于 `Editor/src/Viewport`，属于 `HE::Editor`；引擎公共头、Rendering 模块和引擎 smoke 均不再依赖它。

## 后续

`CameraComponent` 的投影字段已存在于 ECS 数据中；在扩展 Inspector 相机参数前，需要重新生成反射代码，使这些字段进入运行时编辑与场景序列化描述。
