#include "ECSTestSupport.h"
#include "HuaEngine/Scene/Scene.h"
#include "Module/Rendering/RenderJobs.h"

#include <algorithm>
#include <map>
#include <set>

namespace {
using namespace ECSTestSupport;
using namespace HE::Ecs;
using namespace HE::Rendering;

struct Fragment { int Value = 0; };

struct Captured {
    std::vector<RenderSnapshot> Items;
    std::vector<CameraCandidate> Cameras;
};

Captured Extract(World& world, TimelineMode mode) {
    Timeline timeline(world.Context(), {.Mode = mode});
    auto render = Take(timeline.Dispatch(world, ExtractRenderItem{}));
    auto cameras = Take(timeline.Dispatch(world, ExtractPrimaryCamera{}));
    Check(timeline.Finish());
    Captured captured{Take(render.Collect(timeline)), Take(cameras.Collect(timeline))};
    Require(!render.Collect(timeline), "Expected task output to be consumed exactly once");
    return captured;
}
}

int main() {
    EcsContext context({.WorkerCount = 4});
    Captured serial;
    Captured parallel;
    std::map<uint32_t, HE::EntityId> expectedRender;
    std::set<uint32_t> expectedCamera;
    {
        HE::Scene scene(context, "Job extraction");
        auto& world = scene.GetWorld();
        Take(context.Types().Register<Fragment>("Extraction.Fragment"));
        const auto cameraType = context.Types().Find<CameraComponent>()->Id;
        const auto meshType = context.Types().Find<MeshComponent>()->Id;
        const auto materialType = context.Types().Find<MaterialComponent>()->Id;
        for (size_t index = 0; index < 730; ++index) {
            const auto id = Take(scene.CreateEntity("render row", {0xe77ac7ULL, index + 1}));
            HE::TransformComponent transform;
            transform.Position = {float(index), 2, 3};
            Check(world.Emplace<HE::TransformComponent>(id, transform));
            MeshComponent mesh;
            mesh.Mesh.Reference.Guid = "mesh-" + std::to_string(index);
            MaterialComponent material;
            material.Material.Reference.Guid = "material-" + std::to_string(index);
            material.Overrides.SetVec4("u_Color", {float(index), 0.5f, 0.25f, 1});
            material.Overrides.TextureParameters["u_Texture"] = "texture-" + std::to_string(index);
            Check(world.Emplace<MeshComponent>(id, mesh));
            Check(world.Emplace<MaterialComponent>(id, material));
            CameraComponent camera;
            camera.Primary = index % 7 != 0;
            camera.VerticalFovDegrees = 30 + float(index % 50);
            Check(world.Emplace<CameraComponent>(id, camera));
            if (index % 3 == 0) Check(world.Emplace<Fragment>(id, Fragment{int(index)}));
            const bool enabled = index % 13 != 0;
            const bool meshEnabled = index % 17 != 0;
            const bool cameraEnabled = index % 19 != 0;
            Check(world.SetEnabled(id, enabled));
            Check(world.SetComponentEnabled(id, meshType, meshEnabled));
            Check(world.SetComponentEnabled(id, cameraType, cameraEnabled));
            if (index % 23 == 0) { Check(world.Destroy(id)); continue; }
            if (enabled && meshEnabled) expectedRender.emplace(id.Index, id);
            if (enabled && cameraEnabled && camera.Primary) expectedCamera.insert(id.Index);
        }
        Require(world.Stats().Chunks > 4 && expectedRender.size() > 512,
            "Expected extraction to cross chunk, group and task batch boundaries");
        serial = Extract(world, TimelineMode::Serial);
        parallel = Extract(world, TimelineMode::Parallel);
        Require(serial.Items.size() == expectedRender.size() && serial.Cameras.size() == expectedCamera.size(),
            "Expected Jobs to honor entity and required component enable masks");
        Require(parallel.Items.size() == serial.Items.size() && parallel.Cameras.size() == serial.Cameras.size(),
            "Expected equal Serial and Parallel extraction cardinality");
        for (size_t row = 0; row < serial.Items.size(); ++row) {
            const auto& first = serial.Items[row];
            const auto& second = parallel.Items[row];
            Require(first.Entity == second.Entity && first.Transform == second.Transform &&
                first.Mesh.Reference.Guid == second.Mesh.Reference.Guid && first.Material.Reference.Guid == second.Material.Reference.Guid,
                "Expected identical stable Job output in both timeline modes");
            Require(expectedRender.erase(first.Entity.Index) == 1 && world.IsAlive(first.Entity),
                "Expected exactly one full-generation identity per renderable row");
        }
        for (size_t row = 0; row < serial.Cameras.size(); ++row) {
            Require(serial.Cameras[row].Entity == parallel.Cameras[row].Entity &&
                serial.Cameras[row].Camera.VerticalFovDegrees == parallel.Cameras[row].Camera.VerticalFovDegrees,
                "Expected stable camera identity and value output");
            Require(expectedCamera.erase(serial.Cameras[row].Entity.Index) == 1,
                "Expected only active primary cameras in Job output");
        }
        Require(expectedRender.empty() && expectedCamera.empty(), "Expected complete extraction without duplicate rows");
        const auto firstId = serial.Items.front().Entity;
        Check(world.Remove(firstId, materialType));
        Check(world.Clear());
        Require(!world.IsAlive(firstId), "Expected output to remain an identity snapshot after World mutation");
    }
    for (const auto& item : parallel.Items) {
        Require(item.Mesh.Reference.Guid.starts_with("mesh-") && item.Material.Reference.Guid.starts_with("material-") &&
            item.Overrides.Parameters.contains("u_Color") && item.Overrides.TextureParameters.at("u_Texture").starts_with("texture-"),
            "Expected owning resource references and overrides to survive source World destruction");
    }
    Require(!context.HasScheduledWork(), "Expected Job extraction to release every task and borrow");
    std::cout << "RenderExtractionSmoke passed\n";
}
