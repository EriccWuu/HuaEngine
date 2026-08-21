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

## P3：ECS 帧上下文

- 新增场景持有的 `FrameContext`，作为一次 Scheduler 更新内的 typed resource 容器，并在每帧开始清空。
- `CameraSystem` 写入 `RenderFrameData::ActiveCamera`，`RenderSystem` 读取该资源；两者不再通过 `FindSystem` 或系统 getter 通信。
- `SystemDescriptor::Accesses` 统一描述组件与帧资源的 Read/Write；Scheduler 根据其中的写入与消费关系排序同阶段系统。

已完成。

## 后续

`CameraComponent` 的投影字段已存在于 ECS 数据中；在扩展 Inspector 相机参数前，需要重新生成反射代码，使这些字段进入运行时编辑与场景序列化描述。

## 视图边界目标

- Editor 需要同时支持独立的 Scene View 与 Game View，不以二选一方式切换。
- Scene View 使用 `EditorCameraController`，服务于选择、Gizmo 和场景编辑，不修改游戏相机数据。
- Game View 使用 ECS 的 Primary `CameraComponent` 经 `CameraSystem` 生成的渲染相机，展示运行时游戏画面。
- 两个视图未来使用各自的 `RenderTarget`；`RenderSystem` 应演进为消费多个明确的 `RenderView` 请求，而非持有唯一目标。
- 本阶段只实现 Scene View Gizmo，不实现 Game View 与多视图渲染调度。

## EditorGridPass 目标

- Scene View 网格必须由渲染管线绘制并参与深度测试，不能作为 ImGui 前景叠加层。
- `RenderView` 需要显式区分 Editor Scene View 与 Game View；仅 Scene View 请求 `EditorGridPass`。
- `EditorGridPass` 在场景几何之前写入或测试 `SceneDepthAttachment`，使被场景物体遮挡的网格不可见。
- 需要补齐最小 RHI 能力：线段或等价的三角形带绘制、可配置的深度测试/深度写入，以及专用网格 shader 与 GPU buffer 生命周期。
- ImGui 投影网格为过渡实现，`EditorGridPass` 落地后删除，ImGuizmo 保持为 Scene View 的最后一层工具叠加。

已完成：`RenderView::DrawEditorGrid` 仅由外部 Scene View 渲染入口启用；网格通过 `EditorGridPass` 在 `ForwardOpaquePass` 的同一 graphics pass 中先绘制，启用深度测试但不写深度，随后场景几何以相同 depth attachment 遮挡网格。Game/ECS 路径默认关闭。
