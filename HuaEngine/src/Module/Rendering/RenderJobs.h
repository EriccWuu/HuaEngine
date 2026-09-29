#pragma once

#include "HuaEngine/ECS/Runtime/Job.h"
#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
    struct CameraCandidate {
        EntityId Entity;
        glm::mat4 Transform{1.0f};
        CameraComponent Camera;
    };

    struct RenderSnapshot {
        EntityId Entity;
        glm::mat4 Transform{1.0f};
        MeshAssetRef Mesh;
        MaterialAssetRef Material;
        MaterialOverrideSet Overrides;
    };

    struct ExtractPrimaryCamera final {
        using Output = CameraCandidate;
        Ecs::ComponentView<const TransformComponent> transforms;
        Ecs::ComponentView<const CameraComponent> cameras;
        void build(Ecs::AccessBuilder& access) const;
        [[nodiscard]] Ecs::Result<void> run(Ecs::TaskContext& context, Ecs::BatchOutput<Output>& output) const;
    };

    struct ExtractRenderItem final {
        using Output = RenderSnapshot;
        Ecs::ComponentView<const TransformComponent> transforms;
        Ecs::ComponentView<const MeshComponent> meshes;
        Ecs::ComponentView<const MaterialComponent> materials;
        void build(Ecs::AccessBuilder& access) const;
        [[nodiscard]] Ecs::Result<void> run(Ecs::TaskContext& context, Ecs::BatchOutput<Output>& output) const;
    };
}
