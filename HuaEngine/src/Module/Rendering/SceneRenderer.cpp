#include "enginepch.h"
#include "SceneRenderer.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/Rendering/RenderCamera.h"
#include "HuaEngine/Rendering/RenderPipeline/ForwardRenderPipeline.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderResourceResolver.h"
#include "HuaEngine/Rendering/RenderPipeline/RenderTypes.h"
#include "HuaEngine/Rendering/RHI/RenderTarget.h"
#include "Module/Rendering/RenderQueries.h"

namespace HE {
    struct SceneRenderer::Impl {
        Ref<Rendering::RenderTarget> Target;
        Rendering::RenderResourceResolver Resolver;
        Rendering::ForwardRenderPipeline Pipeline;
        Rendering::RenderResult LastResult;
        EntityId ActiveCameraEntity;
    };

    SceneRenderer::SceneRenderer() : m_Impl(std::make_unique<Impl>()) {}
    SceneRenderer::~SceneRenderer() = default;

    void SceneRenderer::SetRenderTarget(Ref<Rendering::RenderTarget> target) {
        m_Impl->Target = std::move(target);
    }

    void SceneRenderer::SetAssetResolver(AssetResolver* resolver) {
        m_Impl->Resolver.SetAssetResolver(resolver);
    }

    Ecs::Result<void> SceneRenderer::RenderActiveCamera(Ecs::Timeline& timeline, Ecs::World& world) {
        m_Impl->LastResult = {};
        m_Impl->ActiveCameraEntity = {};
        if (!m_Impl->Target)
            return Ecs::Error{Ecs::ErrorCode::InvalidState, "RenderActiveCamera", "A render target is required"};
        auto submitted = timeline.Dispatch(world, Rendering::ExtractPrimaryCamera{});
        if (!submitted) return submitted.GetError();
        auto candidates = submitted.Value().Collect(timeline);
        if (!candidates) return candidates.GetError();
        if (candidates.Value().empty()) return timeline.Finish();

        const auto active = std::min_element(candidates.Value().begin(), candidates.Value().end(),
            [](const Rendering::CameraCandidate& left, const Rendering::CameraCandidate& right) {
                return left.Entity.Index < right.Entity.Index;
            });
        const auto& camera = active->Camera;
        const auto& specification = m_Impl->Target->GetSpecification();
        const float aspectRatio = camera.FixedAspectRatio || !specification.Width || !specification.Height
            ? camera.AspectRatio
            : static_cast<float>(specification.Width) / static_cast<float>(specification.Height);
        Rendering::RenderCamera renderCamera(
            glm::perspective(glm::radians(camera.VerticalFovDegrees), aspectRatio,
                camera.NearClip, camera.FarClip),
            glm::inverse(active->Transform));
        auto rendered = RenderSingleCamera(timeline, world, renderCamera);
        if (rendered) m_Impl->ActiveCameraEntity = active->Entity;
        return rendered;
    }

    Ecs::Result<void> SceneRenderer::RenderSingleCamera(Ecs::Timeline& timeline, Ecs::World& world,
        const Rendering::RenderCamera& camera, Rendering::RenderGraphExtension* extension) {
        m_Impl->LastResult = {};
        m_Impl->ActiveCameraEntity = {};
        if (!m_Impl->Target)
            return Ecs::Error{Ecs::ErrorCode::InvalidState, "RenderSingleCamera", "A render target is required"};

        auto submitted = timeline.Dispatch(world, Rendering::ExtractRenderItem{});
        if (!submitted) return submitted.GetError();
        auto snapshots = submitted.Value().Collect(timeline);
        if (!snapshots) return snapshots.GetError();
        auto finished = timeline.Finish();
        if (!finished) return finished.GetError();

        try {
            std::vector<Rendering::RenderItem> items;
            items.reserve(snapshots.Value().size());
            for (auto& snapshot : snapshots.Value()) {
                Rendering::RenderItem item;
                item.SourceEntity = snapshot.Entity;
                item.SourceWorldId = world.Id();
                item.Transform = snapshot.Transform;
                item.Mesh = std::move(snapshot.Mesh);
                item.Material = std::move(snapshot.Material);
                item.MaterialOverrides = std::move(snapshot.Overrides);
                items.push_back(std::move(item));
            }
            std::sort(items.begin(), items.end(), [](const auto& left, const auto& right) {
                return left.SourceEntity.Index < right.SourceEntity.Index;
            });

            Rendering::RenderView view;
            view.CameraRef = CreateRef<Rendering::RenderCamera>(camera);
            view.Target = m_Impl->Target;
            if (!view.Target->GetColorAttachmentTexture())
                return Ecs::Error{Ecs::ErrorCode::InvalidState, "RenderPipeline", "The render target has no color attachment"};
            m_Impl->LastResult = m_Impl->Pipeline.Render(view, items, m_Impl->Resolver, extension);
            if (!m_Impl->LastResult.Succeeded) {
                std::string message = "The render pipeline did not complete";
                if (!m_Impl->LastResult.GraphDiagnostics.empty())
                    message += ": " + m_Impl->LastResult.GraphDiagnostics.front().Message;
                return Ecs::Error{Ecs::ErrorCode::InvalidState, "RenderPipeline", std::move(message)};
            }
            return {};
        } catch (const std::exception& exception) {
            return Ecs::Error{Ecs::ErrorCode::ConstructionFailed, "RenderSingleCamera", exception.what()};
        } catch (...) {
            return Ecs::Error{Ecs::ErrorCode::ConstructionFailed, "RenderSingleCamera", "Render preparation failed"};
        }
    }

    const Rendering::RenderResult& SceneRenderer::LastResult() const noexcept { return m_Impl->LastResult; }
    EntityId SceneRenderer::LastActiveCameraEntity() const noexcept { return m_Impl->ActiveCameraEntity; }
}
