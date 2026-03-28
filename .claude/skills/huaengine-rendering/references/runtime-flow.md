# Runtime Flow

## 1. Primary Frame Path

The maintained rendering path is:

1. host activates a scene viewport through `ApplicationOperations`
2. each frame calls `rendering.render_scene_viewport`
3. `RenderSystem` gathers renderable entities from the scene
4. `Renderer` binds camera state and submits draws
5. `RenderCommand` delegates to `RendererAPI`
6. `RendererAPI` is currently backed by OpenGL

## 2. Host Boundary

Rendering is no longer described in terms of `Editor / Sandbox / Headless` as parallel first-class consumers.

Current host reality:

- `Editor` is the maintained GUI rendering host
- `ProjectHub` is only a launcher and does not own scene viewport rendering
- `Headless` uses formal non-GUI operations where relevant
- `Sandbox` has been removed from the maintained host graph

## 3. Debugging Order

When a render issue appears:

1. check scene/component inputs
2. check mesh/material readiness
3. check renderer/framebuffer/camera state
4. only then go into backend-level OpenGL investigation
