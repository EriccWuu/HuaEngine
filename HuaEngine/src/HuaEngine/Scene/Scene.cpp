#include "enginepch.h"
#include "Scene.h"

#include <stdexcept>

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "HuaEngine/Generated/GeneratedReflection.h"
#include "Module/Rendering/RenderingComponent.h"
#include "Module/Rendering/SceneRenderer.h"

namespace HE {
    namespace {
        void RegisterSceneComponents(Ecs::EcsContext& context) {
            auto generated = Generated::RegisterGeneratedComponents(context.Types());
            if (!generated) {
                throw std::runtime_error(generated.GetError().Operation + ": " + generated.GetError().Message);
            }
            auto legacyRenderer = context.Types().Register<Rendering::RendererComponent>(
                Ecs::TypeGuid::FromName("HE::Rendering::RendererComponent"), "RendererComponent");
            if (!legacyRenderer) {
                throw std::runtime_error(legacyRenderer.GetError().Operation + ": " + legacyRenderer.GetError().Message);
            }
        }
    }

    Scene::Scene() : Scene(std::string{}) {}

    Scene::Scene(const std::string& name)
        : m_Name(name), m_OwnedContext(std::make_shared<Ecs::EcsContext>()), m_World(*m_OwnedContext) {
        RegisterSceneComponents(*m_OwnedContext);
    }

    Scene::Scene(Ecs::EcsContext& context, const std::string& name)
        : m_Name(name), m_World(context) {
        RegisterSceneComponents(context);
    }

    Scene::~Scene() = default;

    Ecs::Result<void> Scene::Update() { return OnUpdate(); }
    void Scene::OnRuntimeStart() {}
    void Scene::OnRuntimeStop() {}

    Ecs::Result<void> Scene::OnUpdate(float) {
        if (!m_World.Context().IsMainThread())
            return Ecs::Error{Ecs::ErrorCode::WrongThread, "SceneUpdate", "Scene updates require the Context main thread"};
        try {
            {
                Ecs::Timeline timeline(m_World.Context());
                auto rendered = m_Renderer ? m_Renderer->RenderActiveCamera(timeline, m_World)
                    : timeline.Finish();
                if (!rendered) return rendered.GetError();
            }
            auto compacted = m_World.Compact();
            if (!compacted) return compacted.GetError();
            return {};
        } catch (const std::logic_error& exception) {
            return Ecs::Error{Ecs::ErrorCode::Busy, "SceneUpdate", exception.what()};
        } catch (const std::exception& exception) {
            return Ecs::Error{Ecs::ErrorCode::ConstructionFailed, "SceneUpdate", exception.what()};
        }
    }

    Ecs::Result<EntityId> Scene::CreateEntity(std::string_view name, EntityUuid uuid) {
        auto scope = Ecs::WorldEditScope::Acquire(m_World);
        if (!scope) return scope.GetError();
        // Structural operations publish their own chunk versions. Keep the scope for synchronization.
        return CreateEntityInScope(m_World, name, uuid);
    }

    Ecs::Result<EntityId> Scene::CreateEntityInScope(Ecs::World& world, std::string_view name, EntityUuid uuid) {
        if (uuid != EntityUuid{}) {
            if (const auto existing = world.Find(uuid)) return existing;
        }
        const auto* transform = world.Types().Find<TransformComponent>();
        if (!transform) {
            return Ecs::Error{Ecs::ErrorCode::InvalidType, "CreateSceneEntity", "TransformComponent is not registered"};
        }
        auto created = world.CreateEmpty(name, uuid);
        if (!created) return created.GetError();
        const EntityId id = created.Value();
        auto added = world.AddDefault(id, transform->Id);
        if (!added) {
            const auto failure = added.GetError();
            auto removed = world.Destroy(id);
            if (!removed) {
                return Ecs::Error{Ecs::ErrorCode::InvalidState, "CreateSceneEntity",
                    failure.Message + "; cleanup failed: " + removed.GetError().Message};
            }
            return failure;
        }
        return id;
    }

    bool Scene::HasRenderer() const noexcept { return m_Renderer != nullptr; }

    Ecs::Result<bool> Scene::AttachRenderer(Ref<Rendering::RenderTarget> target, AssetResolver* resolver) {
        if (!target) return Ecs::Error{Ecs::ErrorCode::InvalidArgument, "AttachRenderer", "A render target is required"};
        try {
            const bool created = !m_Renderer;
            if (!m_Renderer) m_Renderer = std::make_unique<SceneRenderer>();
            m_Renderer->SetRenderTarget(std::move(target));
            m_Renderer->SetAssetResolver(resolver);
            return created;
        }
        catch (const std::exception& exception) {
            return Ecs::Error{Ecs::ErrorCode::ConstructionFailed, "AttachRenderer", exception.what()};
        }
    }

    Ecs::Result<void> Scene::RenderSingleCamera(const Rendering::RenderCamera& camera,
        Rendering::RenderGraphExtension* extension) {
        if (!m_Renderer) return Ecs::Error{Ecs::ErrorCode::InvalidState, "RenderSingleCamera", "The scene renderer is not attached"};
        if (!m_World.Context().IsMainThread())
            return Ecs::Error{Ecs::ErrorCode::WrongThread, "RenderSingleCamera", "Rendering requires the Context main thread"};
        try {
            Ecs::Timeline timeline(m_World.Context());
            return m_Renderer->RenderSingleCamera(timeline, m_World, camera, extension);
        } catch (const std::logic_error& exception) {
            return Ecs::Error{Ecs::ErrorCode::Busy, "RenderSingleCamera", exception.what()};
        } catch (const std::exception& exception) {
            return Ecs::Error{Ecs::ErrorCode::ConstructionFailed, "RenderSingleCamera", exception.what()};
        }
    }

    const Rendering::RenderResult* Scene::LastRenderResult() const noexcept {
        return m_Renderer ? &m_Renderer->LastResult() : nullptr;
    }

    EntityId Scene::LastActiveCameraEntity() const noexcept {
        return m_Renderer ? m_Renderer->LastActiveCameraEntity() : EntityId{};
    }
}
