#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Runtime/World.h"

namespace HE {
    class AssetResolver;
    class SceneRenderer;

    namespace Rendering {
        class RenderCamera;
        class RenderGraphExtension;
        class RenderTarget;
        struct RenderResult;
    }

    class Scene final {
    public:
        Scene();
        explicit Scene(const std::string& name);
        explicit Scene(Ecs::EcsContext& context, const std::string& name = {});
        ~Scene();
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

        [[nodiscard]] Ecs::Result<void> Update();
        void OnRuntimeStart();
        [[nodiscard]] Ecs::Result<void> OnUpdate(float deltaTime = 0.0f);
        void OnRuntimeStop();

        [[nodiscard]] const std::string& GetName() const noexcept { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }
        [[nodiscard]] Ecs::World& GetWorld() noexcept { return m_World; }
        [[nodiscard]] const Ecs::World& GetWorld() const noexcept { return m_World; }

        [[nodiscard]] Ecs::Result<EntityId> CreateEntity(std::string_view name = "Entity", EntityUuid uuid = {});
        [[nodiscard]] static Ecs::Result<EntityId> CreateEntityInScope(Ecs::World& world,
            std::string_view name = "Entity", EntityUuid uuid = {});

        [[nodiscard]] bool HasRenderer() const noexcept;
        [[nodiscard]] Ecs::Result<bool> AttachRenderer(Ref<Rendering::RenderTarget> target, AssetResolver* resolver);
        [[nodiscard]] Ecs::Result<void> RenderSingleCamera(const Rendering::RenderCamera& camera,
            Rendering::RenderGraphExtension* extension = nullptr);
        [[nodiscard]] const Rendering::RenderResult* LastRenderResult() const noexcept;
        [[nodiscard]] EntityId LastActiveCameraEntity() const noexcept;

    private:
        std::string m_Name;
        std::shared_ptr<Ecs::EcsContext> m_OwnedContext;
        Ecs::World m_World;
        std::unique_ptr<SceneRenderer> m_Renderer;
    };
}
