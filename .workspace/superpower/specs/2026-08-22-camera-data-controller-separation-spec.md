# 相机数据与控制分离规格

## 目标

将渲染消费的相机快照、ECS 场景相机数据和编辑器相机控制状态分离。

## P1：ECS 相机数据化

- 新增纯 `RenderCamera`，仅携带 view/projection 矩阵。
- `CameraComponent` 保存可序列化的投影参数，不再持有 `RuntimeCamera`。
- `RenderSystem` 由 `TransformComponent + CameraComponent` 构建 `RenderCamera`。
- 视口渲染入口只消费 `RenderCamera`。

## P2：编辑器控制器化

- 将 `EditorCamera` 改为不继承渲染相机数据的控制器。
- 控制器根据编辑器状态和视口尺寸输出 `RenderCamera`。
- Editor 与 smoke 改为使用控制器输出的相机快照。
