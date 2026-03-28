#include "enginepch.h"
#include "Interaction/EditorSceneCommands.h"

#include "Selection.h"
#include "Workbench/SceneDocument.h"

namespace HE {
    namespace {
        struct EntitySnapshot {
            std::string Name;
            TransformComponent Transform;
            bool HasCamera = false;
            Rendering::CameraComponent Camera;
            bool HasMesh = false;
            Rendering::MeshComponent Mesh;
            bool HasMaterial = false;
            Rendering::MaterialComponent Material;
            bool HasRenderer = false;
            Rendering::RendererComponent Renderer;
        };

        Ref<Scene> GetScene(const EditorCommandContext& context) {
            return context.SceneDocument ? context.SceneDocument->SceneRef : nullptr;
        }

        EntitySnapshot CaptureEntitySnapshot(const Entity& entity) {
            EntitySnapshot snapshot;
            snapshot.Name = entity.GetName();
            snapshot.Transform = entity.GetComponent<TransformComponent>();

            if (entity.HasComponent<Rendering::CameraComponent>()) {
                snapshot.HasCamera = true;
                snapshot.Camera = entity.GetComponent<Rendering::CameraComponent>();
            }

            if (entity.HasComponent<Rendering::MeshComponent>()) {
                snapshot.HasMesh = true;
                snapshot.Mesh = entity.GetComponent<Rendering::MeshComponent>();
            }

            if (entity.HasComponent<Rendering::MaterialComponent>()) {
                snapshot.HasMaterial = true;
                snapshot.Material = entity.GetComponent<Rendering::MaterialComponent>();
            }

            if (entity.HasComponent<Rendering::RendererComponent>()) {
                snapshot.HasRenderer = true;
                snapshot.Renderer = entity.GetComponent<Rendering::RendererComponent>();
            }

            return snapshot;
        }

        Entity RestoreEntitySnapshot(Scene& scene, const EntitySnapshot& snapshot) {
            Entity entity = scene.GetEntityManager().CreateEntity(snapshot.Name);
            entity.GetComponent<TransformComponent>() = snapshot.Transform;

            if (snapshot.HasCamera) {
                entity.AddComponent<Rendering::CameraComponent>(snapshot.Camera);
            }

            if (snapshot.HasMesh) {
                entity.AddComponent<Rendering::MeshComponent>(snapshot.Mesh);
            }

            if (snapshot.HasMaterial) {
                entity.AddComponent<Rendering::MaterialComponent>(snapshot.Material);
            }

            if (snapshot.HasRenderer) {
                entity.AddComponent<Rendering::RendererComponent>(snapshot.Renderer);
            }

            return entity;
        }

        Rendering::CameraComponent MakeDefaultCameraComponent() {
            Rendering::CameraComponent component;
            component.Camera = CreateRef<Rendering::Camera>();
            component.Primary = true;
            component.FixedAspectRatio = false;
            return component;
        }

        Rendering::MeshComponent MakeDefaultMeshComponent() {
            MeshManager::Instance().LoadDefaultMeshes();
            return Rendering::MeshComponent("Quad");
        }

        Rendering::MaterialComponent MakeDefaultMaterialComponent() {
            auto& library = Rendering::MaterialLibrary::Instance();
            Ref<Material> baseMaterial = nullptr;

            if (library.HasMaterial("SandboxMaterial")) {
                baseMaterial = library.GetMaterial("SandboxMaterial");
            }

            if (!baseMaterial) {
                if (!library.GetDefaultMaterial()) {
                    library.CreateDefaultMaterials();
                }
                baseMaterial = library.GetDefaultMaterial();
            }

            Rendering::MaterialComponent component;
            component.MaterialInstance = baseMaterial ? baseMaterial->CreateInstance() : nullptr;
            return component;
        }

        template<typename T>
        bool HasComponent(const Entity& entity) {
            return entity.IsValid() && entity.HasComponent<T>();
        }

        class CreateEntityCommand final : public IEditorCommand {
        public:
            explicit CreateEntityCommand(std::string entityName)
                : m_EntityName(std::move(entityName)) {}

            std::string GetLabel() const override {
                return "Create Entity";
            }

            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene) {
                    return ResultEnvelope::Failure("editor.entity.create", "scene", "An active scene is required before creating an entity");
                }

                Entity entity = scene->GetEntityManager().CreateEntity(m_EntityName);
                m_RuntimeEntity = entity;
                Selection::SetSelection(entity);
                return ResultEnvelope::Success("editor.entity.create", entity.GetName(), "Created a new entity");
            }

            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene) {
                    return ResultEnvelope::Failure("editor.entity.create.undo", "scene", "An active scene is required before undoing entity creation");
                }

                if (!m_RuntimeEntity.IsValid()) {
                    return ResultEnvelope::Failure("editor.entity.create.undo", m_EntityName, "The created entity is no longer available");
                }

                scene->GetEntityManager().DestroyEntity(m_RuntimeEntity);
                Selection::RemoveFromSelection(m_RuntimeEntity);
                Selection::RemoveInvalidSelections();
                m_RuntimeEntity = {};
                return ResultEnvelope::Success("editor.entity.create.undo", m_EntityName, "Removed the created entity");
            }

        private:
            std::string m_EntityName;
            Entity m_RuntimeEntity;
        };

        class DeleteEntitiesCommand final : public IEditorCommand {
        public:
            explicit DeleteEntitiesCommand(std::vector<Entity> entities)
                : m_RuntimeEntities(std::move(entities)) {}

            std::string GetLabel() const override {
                return m_RuntimeEntities.size() > 1 ? "Delete Entities" : "Delete Entity";
            }

            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene) {
                    return ResultEnvelope::Failure("editor.entity.delete", "scene", "An active scene is required before deleting entities");
                }

                if (!m_Initialized) {
                    m_Snapshots.clear();
                    for (const auto& entity : m_RuntimeEntities) {
                        if (!entity.IsValid()) {
                            continue;
                        }
                        m_Snapshots.push_back(CaptureEntitySnapshot(entity));
                    }
                    m_Initialized = true;
                }

                for (const auto& entity : m_RuntimeEntities) {
                    if (entity.IsValid()) {
                        scene->GetEntityManager().DestroyEntity(entity);
                    }
                }

                Selection::ClearSelection();
                return ResultEnvelope::Success("editor.entity.delete", "selection", m_Snapshots.size() > 1 ? "Deleted selected entities" : "Deleted selected entity");
            }

            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene) {
                    return ResultEnvelope::Failure("editor.entity.delete.undo", "scene", "An active scene is required before undoing entity deletion");
                }

                m_RuntimeEntities.clear();
                std::vector<Entity> restored;
                restored.reserve(m_Snapshots.size());
                for (const auto& snapshot : m_Snapshots) {
                    auto entity = RestoreEntitySnapshot(*scene, snapshot);
                    restored.push_back(entity);
                    m_RuntimeEntities.push_back(entity);
                }

                Selection::SetSelections(restored);
                return ResultEnvelope::Success("editor.entity.delete.undo", "selection", m_Snapshots.size() > 1 ? "Restored deleted entities" : "Restored deleted entity");
            }

        private:
            bool m_Initialized = false;
            std::vector<EntitySnapshot> m_Snapshots;
            std::vector<Entity> m_RuntimeEntities;
        };

        class AddCameraComponentCommand final : public IEditorCommand {
        public:
            explicit AddCameraComponentCommand(Entity entity) : m_Entity(entity) {}

            std::string GetLabel() const override { return "Add Camera Component"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.add_camera", "entity", "A valid selected entity is required before adding CameraComponent");
                }
                if (m_Entity.HasComponent<Rendering::CameraComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_camera", m_Entity.GetName(), "CameraComponent already exists on the selected entity");
                }
                m_Entity.AddComponent<Rendering::CameraComponent>(MakeDefaultCameraComponent());
                return ResultEnvelope::Success("editor.component.add_camera", m_Entity.GetName(), "Added CameraComponent");
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                (void)context;
                if (!m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::CameraComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_camera.undo", "entity", "CameraComponent is no longer available to remove");
                }
                m_Entity.RemoveComponent<Rendering::CameraComponent>();
                return ResultEnvelope::Success("editor.component.add_camera.undo", m_Entity.GetName(), "Removed CameraComponent");
            }
        private:
            Entity m_Entity;
        };

        class RemoveCameraComponentCommand final : public IEditorCommand {
        public:
            explicit RemoveCameraComponentCommand(Entity entity) : m_Entity(entity) {}

            std::string GetLabel() const override { return "Remove Camera Component"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::CameraComponent>()) {
                    return ResultEnvelope::Failure("editor.component.remove_camera", "entity", "CameraComponent is not available on the selected entity");
                }
                m_Component = m_Entity.GetComponent<Rendering::CameraComponent>();
                m_Entity.RemoveComponent<Rendering::CameraComponent>();
                return ResultEnvelope::Success("editor.component.remove_camera", m_Entity.GetName(), "Removed CameraComponent");
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                (void)context;
                if (!m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.remove_camera.undo", "entity", "The selected entity is no longer available");
                }
                m_Entity.AddComponent<Rendering::CameraComponent>(m_Component);
                return ResultEnvelope::Success("editor.component.remove_camera.undo", m_Entity.GetName(), "Restored CameraComponent");
            }
        private:
            Entity m_Entity;
            Rendering::CameraComponent m_Component;
        };

        class AddMeshComponentCommand final : public IEditorCommand {
        public:
            explicit AddMeshComponentCommand(Entity entity) : m_Entity(entity) {}
            std::string GetLabel() const override { return "Add Mesh Component"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.add_mesh", "entity", "A valid selected entity is required before adding MeshComponent");
                }
                if (m_Entity.HasComponent<Rendering::MeshComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_mesh", m_Entity.GetName(), "MeshComponent already exists on the selected entity");
                }
                m_Entity.AddComponent<Rendering::MeshComponent>(MakeDefaultMeshComponent());
                return ResultEnvelope::Success("editor.component.add_mesh", m_Entity.GetName(), "Added MeshComponent");
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                (void)context;
                if (!m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::MeshComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_mesh.undo", "entity", "MeshComponent is no longer available to remove");
                }
                m_Entity.RemoveComponent<Rendering::MeshComponent>();
                return ResultEnvelope::Success("editor.component.add_mesh.undo", m_Entity.GetName(), "Removed MeshComponent");
            }
        private:
            Entity m_Entity;
        };

        class RemoveMeshComponentCommand final : public IEditorCommand {
        public:
            explicit RemoveMeshComponentCommand(Entity entity) : m_Entity(entity) {}
            std::string GetLabel() const override { return "Remove Mesh Component"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::MeshComponent>()) {
                    return ResultEnvelope::Failure("editor.component.remove_mesh", "entity", "MeshComponent is not available on the selected entity");
                }
                m_Component = m_Entity.GetComponent<Rendering::MeshComponent>();
                m_Entity.RemoveComponent<Rendering::MeshComponent>();
                return ResultEnvelope::Success("editor.component.remove_mesh", m_Entity.GetName(), "Removed MeshComponent");
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                (void)context;
                if (!m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.remove_mesh.undo", "entity", "The selected entity is no longer available");
                }
                m_Entity.AddComponent<Rendering::MeshComponent>(m_Component);
                return ResultEnvelope::Success("editor.component.remove_mesh.undo", m_Entity.GetName(), "Restored MeshComponent");
            }
        private:
            Entity m_Entity;
            Rendering::MeshComponent m_Component;
        };

        class AddMaterialComponentCommand final : public IEditorCommand {
        public:
            explicit AddMaterialComponentCommand(Entity entity) : m_Entity(entity) {}
            std::string GetLabel() const override { return "Add Material Component"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.add_material", "entity", "A valid selected entity is required before adding MaterialComponent");
                }
                if (m_Entity.HasComponent<Rendering::MaterialComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_material", m_Entity.GetName(), "MaterialComponent already exists on the selected entity");
                }
                m_Entity.AddComponent<Rendering::MaterialComponent>(MakeDefaultMaterialComponent());
                return ResultEnvelope::Success("editor.component.add_material", m_Entity.GetName(), "Added MaterialComponent");
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                (void)context;
                if (!m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::MaterialComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_material.undo", "entity", "MaterialComponent is no longer available to remove");
                }
                m_Entity.RemoveComponent<Rendering::MaterialComponent>();
                return ResultEnvelope::Success("editor.component.add_material.undo", m_Entity.GetName(), "Removed MaterialComponent");
            }
        private:
            Entity m_Entity;
        };

        class RemoveMaterialComponentCommand final : public IEditorCommand {
        public:
            explicit RemoveMaterialComponentCommand(Entity entity) : m_Entity(entity) {}
            std::string GetLabel() const override { return "Remove Material Component"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::MaterialComponent>()) {
                    return ResultEnvelope::Failure("editor.component.remove_material", "entity", "MaterialComponent is not available on the selected entity");
                }
                m_Component = m_Entity.GetComponent<Rendering::MaterialComponent>();
                m_Entity.RemoveComponent<Rendering::MaterialComponent>();
                return ResultEnvelope::Success("editor.component.remove_material", m_Entity.GetName(), "Removed MaterialComponent");
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                (void)context;
                if (!m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.remove_material.undo", "entity", "The selected entity is no longer available");
                }
                m_Entity.AddComponent<Rendering::MaterialComponent>(m_Component);
                return ResultEnvelope::Success("editor.component.remove_material.undo", m_Entity.GetName(), "Restored MaterialComponent");
            }
        private:
            Entity m_Entity;
            Rendering::MaterialComponent m_Component;
        };
    }

    const std::vector<EditorInspectableComponentDescriptor>& GetEditorInspectableComponents() {
        static const std::vector<EditorInspectableComponentDescriptor> kComponents = {
            { EditorInspectableComponent::Camera, "component.camera", "Camera" },
            { EditorInspectableComponent::Mesh, "component.mesh", "Mesh" },
            { EditorInspectableComponent::Material, "component.material", "Material" }
        };
        return kComponents;
    }

    const EditorInspectableComponentDescriptor* FindEditorInspectableComponent(std::string_view id) {
        for (const auto& descriptor : GetEditorInspectableComponents()) {
            if (descriptor.Id == id) {
                return &descriptor;
            }
        }

        return nullptr;
    }

    bool EntityHasInspectableComponent(EditorInspectableComponent type, const Entity& entity) {
        switch (type) {
            case EditorInspectableComponent::Camera:
                return HasComponent<Rendering::CameraComponent>(entity);
            case EditorInspectableComponent::Mesh:
                return HasComponent<Rendering::MeshComponent>(entity);
            case EditorInspectableComponent::Material:
                return HasComponent<Rendering::MaterialComponent>(entity);
            default:
                return false;
        }
    }

    bool CanRemoveInspectableComponent(EditorInspectableComponent type, const Entity& entity) {
        return EntityHasInspectableComponent(type, entity);
    }

    EditorCommandPtr CreateCreateEntityCommand(std::string entityName) {
        return std::make_unique<CreateEntityCommand>(std::move(entityName));
    }

    EditorCommandPtr CreateDeleteEntitiesCommand(const std::vector<Entity>& entities) {
        return std::make_unique<DeleteEntitiesCommand>(entities);
    }

    EditorCommandPtr CreateAddComponentCommand(EditorInspectableComponent type, const Entity& entity) {
        switch (type) {
            case EditorInspectableComponent::Camera:
                return std::make_unique<AddCameraComponentCommand>(entity);
            case EditorInspectableComponent::Mesh:
                return std::make_unique<AddMeshComponentCommand>(entity);
            case EditorInspectableComponent::Material:
                return std::make_unique<AddMaterialComponentCommand>(entity);
            default:
                return nullptr;
        }
    }

    EditorCommandPtr CreateRemoveComponentCommand(EditorInspectableComponent type, const Entity& entity) {
        switch (type) {
            case EditorInspectableComponent::Camera:
                return std::make_unique<RemoveCameraComponentCommand>(entity);
            case EditorInspectableComponent::Mesh:
                return std::make_unique<RemoveMeshComponentCommand>(entity);
            case EditorInspectableComponent::Material:
                return std::make_unique<RemoveMaterialComponentCommand>(entity);
            default:
                return nullptr;
        }
    }
}
