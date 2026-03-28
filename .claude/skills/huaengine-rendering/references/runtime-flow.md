# 运行时渲染流程

## 1. 主帧路径

当前受维护的渲染主链是：

1. 宿主通过 `ApplicationOperations` 激活场景视口
2. 每帧调用正式的 scene viewport 渲染操作
3. `RenderSystem` 从场景里收集可渲染实体
4. `Renderer` 绑定相机状态并提交 draw
5. `RenderCommand` 继续委托给 `RendererAPI`
6. `RendererAPI` 当前由 OpenGL 实现

## 2. 宿主边界

当前渲染已经不再按 `Editor / Sandbox / Headless` 三个并列一等消费者来理解。

当前宿主事实：

- `Editor` 是正式 GUI 渲染宿主
- `ProjectHub` 只是启动器，不拥有场景视口渲染
- `Headless` 在适用场景下消费正式无 GUI 操作
- `Sandbox` 已从正式宿主图中移除

## 3. 排查顺序

出现渲染问题时，优先按这个顺序：

1. 先看场景 / 组件输入
2. 再看 mesh / material 是否准备就绪
3. 再看 renderer / framebuffer / camera 状态
4. 最后才下钻到 OpenGL backend
