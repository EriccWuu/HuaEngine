#include "enginepch.h"
#include "RenderQueries.h"

namespace HE::Rendering {
    void ExtractPrimaryCamera(EntityId entity, const TransformComponent& transform,
        const CameraComponent& camera, Ecs::BatchOutput<CameraCandidate> output) {
        if (!camera.Primary) return;
        output.Emit(CameraCandidate{entity, transform.GetTransformMat(), camera});
    }

    void ExtractRenderItem(EntityId entity, const TransformComponent& transform,
        const MeshComponent& mesh, const MaterialComponent& material,
        Ecs::BatchOutput<RenderSnapshot> output) {
        output.Emit(RenderSnapshot{entity, transform.GetTransformMat(), mesh.Mesh,
            material.Material, material.Overrides});
    }
}
