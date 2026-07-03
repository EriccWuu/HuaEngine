# Refactored Engine Architecture

## 1. One-Line Model

The current HuaEngine architecture is:

`hosts stay thin, formal capabilities are exposed through ApplicationOperations, and Project / Scene / Asset / Script / Validation services sit underneath as the shared engine control layer.`

## 2. Maintained Hosts

- `ProjectHub.exe`: launcher-only no-project entry
- `Editor.exe`: project-bound GUI workbench
- `HuaEngineCLI.exe`: CLI interface

`Sandbox` has been removed from the active host topology.

## 3. Layering

- Host layer: ProjectHub, Editor, CLI
- Control layer: Application, ApplicationOperations, OperationRegistry, ResultEnvelope
- Domain layer: ProjectService, SceneService, AssetService, ScriptService, ValidationService
- Engine core layer: ECS, Scene, Serialization, Reflection, Rendering, OpenGL backend, Window/Event/ImGui

## 4. Static Architecture

```mermaid
graph TD
    A[ProjectHub Host] --> B[Application]
    C[Editor Host] --> B
    D[CLI Host] --> B

    B --> E[ApplicationOperations]
    B --> F[Window / LayerStack / Event / ImGui]

    E --> G[ProjectService]
    E --> H[SceneService]
    E --> I[AssetService]
    E --> J[ScriptService]
    E --> K[ValidationService]
    E --> L[Rendering Seam]

    H --> M[Scene]
    M --> N[EntityManager + EnTT]
    L --> O[RenderSystem]
    O --> P[Renderer]
    P --> Q[RenderCommand / RendererAPI]
    Q --> R[OpenGL Backend]
```

## 5. GUI Sequence

```mermaid
sequenceDiagram
    participant Hub as ProjectHub.exe
    participant Editor as Editor.exe
    participant App as Application
    participant Ops as ApplicationOperations
    participant Layer as EditorLayer

    Hub->>Editor: launch --project [--scene]
    Editor->>App: Start()
    App->>Ops: create formal operation surface
    App->>Layer: OnAttach()
    Layer->>Ops: open project / scene
    loop per frame
        Layer->>Ops: render scene viewport
        Layer->>Ops: validate / save / edit actions
    end
```

## 6. Reading Order

Start here when navigating changes:

1. identify the host
2. check whether the issue belongs to `ApplicationOperations`
3. then drill into scene/rendering/serialization internals
