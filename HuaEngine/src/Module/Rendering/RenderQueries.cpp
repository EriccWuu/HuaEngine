#include "enginepch.h"
#include "RenderQueries.h"

namespace HE::Rendering {
    void ExtractPrimaryCamera::build(Ecs::AccessBuilder& access) const {
        access.read(&ExtractPrimaryCamera::transforms).read(&ExtractPrimaryCamera::cameras);
    }

    Ecs::Result<void> ExtractPrimaryCamera::run(Ecs::TaskContext& context, Ecs::BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row) {
            const auto& camera = cameras[row];
            if (camera.Primary)
                output.Emit(CameraCandidate{context.entity(row), transforms[row].GetTransformMat(), camera});
        }
        return {};
    }

    void ExtractRenderItem::build(Ecs::AccessBuilder& access) const {
        access.read(&ExtractRenderItem::transforms)
            .read(&ExtractRenderItem::meshes)
            .read(&ExtractRenderItem::materials);
    }

    Ecs::Result<void> ExtractRenderItem::run(Ecs::TaskContext& context, Ecs::BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row) {
            const auto& material = materials[row];
            output.Emit(RenderSnapshot{context.entity(row), transforms[row].GetTransformMat(),
                meshes[row].Mesh, material.Material, material.Overrides});
        }
        return {};
    }
}
