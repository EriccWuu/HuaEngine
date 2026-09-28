#pragma once

#include "HuaEngine/ECS/Runtime/GeneratedQuery.h"
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

    HE_ECS_QUERY()
    void ExtractPrimaryCamera(EntityId entity, const TransformComponent& transform,
        const CameraComponent& camera, Ecs::BatchOutput<CameraCandidate> output);

    HE_ECS_QUERY()
    void ExtractRenderItem(EntityId entity, const TransformComponent& transform,
        const MeshComponent& mesh, const MaterialComponent& material,
        Ecs::BatchOutput<RenderSnapshot> output);
}
