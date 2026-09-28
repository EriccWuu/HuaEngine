#pragma once

#include <memory>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/Runtime/Result.h"

namespace HE {
    class AssetResolver;
    namespace Ecs { class World; class Timeline; }
    namespace Rendering {
        class RenderCamera;
        class RenderGraphExtension;
        class RenderTarget;
        struct RenderResult;
    }

    class SceneRenderer final {
    public:
        SceneRenderer();
        ~SceneRenderer();
        SceneRenderer(const SceneRenderer&) = delete;
        SceneRenderer& operator=(const SceneRenderer&) = delete;
        SceneRenderer(SceneRenderer&&) = delete;
        SceneRenderer& operator=(SceneRenderer&&) = delete;

        void SetRenderTarget(Ref<Rendering::RenderTarget> target);
        void SetAssetResolver(AssetResolver* resolver);
        [[nodiscard]] Ecs::Result<void> RenderActiveCamera(Ecs::Timeline& timeline, Ecs::World& world);
        [[nodiscard]] Ecs::Result<void> RenderSingleCamera(Ecs::Timeline& timeline, Ecs::World& world,
            const Rendering::RenderCamera& camera, Rendering::RenderGraphExtension* extension = nullptr);
        [[nodiscard]] const Rendering::RenderResult& LastResult() const noexcept;
        [[nodiscard]] EntityId LastActiveCameraEntity() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
