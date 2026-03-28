#include "enginepch.h"
#include "Interaction/EditorSceneCommands.h"

#include <array>

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

        ApplicationOperations* GetOperations(const EditorCommandContext& context) {
            return context.Operations;
        }

        ResultEnvelope MakeOperationsMissingResult(std::string operation) {
            auto result = ResultEnvelope::Failure(std::move(operation), "application.operations", "Application operations are not available");
            result.AddDetail({
                DiagnosticSeverity::Error,
                "editor.operations.unavailable",
                "Editor commands require ApplicationOperations to execute shared scene mutations",
                {}
            });
            return result;
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

        ResultEnvelope RestoreEntitySnapshot(const EditorCommandContext& context, const EntitySnapshot& snapshot, Entity& outEntity) {
            auto scene = GetScene(context);
            if (!scene) {
                return ResultEnvelope::Failure("scene.entity.restore", "scene", "An active scene is required before restoring an entity snapshot");
            }

            auto* operations = GetOperations(context);
            if (!operations) {
                return MakeOperationsMissingResult("scene.entity.restore");
            }

            uint32_t entityId = 0;
            auto createResult = operations->CreateSceneEntity(*scene, snapshot.Name, &entityId);
            if (!createResult.Succeeded()) {
                return createResult;
            }

            auto nameResult = operations->UpsertSceneEntityName(*scene, entityId, NameComponent(snapshot.Name));
            if (!nameResult.Succeeded()) {
                return nameResult;
            }

            auto transformResult = operations->UpsertSceneEntityTransform(*scene, entityId, snapshot.Transform);
            if (!transformResult.Succeeded()) {
                return transformResult;
            }

            if (snapshot.HasCamera) {
                auto cameraResult = operations->UpsertSceneCameraComponent(*scene, entityId, snapshot.Camera);
                if (!cameraResult.Succeeded()) {
                    return cameraResult;
                }
            }

            if (snapshot.HasMesh) {
                auto meshResult = operations->UpsertSceneMeshComponent(*scene, entityId, snapshot.Mesh);
                if (!meshResult.Succeeded()) {
                    return meshResult;
                }
            }

            if (snapshot.HasMaterial) {
                auto materialResult = operations->UpsertSceneMaterialComponent(*scene, entityId, snapshot.Material);
                if (!materialResult.Succeeded()) {
                    return materialResult;
                }
            }

            outEntity = Entity(static_cast<entt::entity>(entityId), &scene->GetEntityManager());
            return ResultEnvelope::Success("scene.entity.restore", snapshot.Name, "Scene entity restored from snapshot");
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

                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.entity.create");
                }

                uint32_t entityId = 0;
                auto result = operations->CreateSceneEntity(*scene, m_EntityName, &entityId);
                if (!result.Succeeded()) {
                    return result;
                }

                m_RuntimeEntity = Entity(static_cast<entt::entity>(entityId), &scene->GetEntityManager());
                Selection::SetSelection(m_RuntimeEntity);
                result.Operation = "editor.entity.create";
                result.Target = m_RuntimeEntity.GetName();
                result.Summary = "Created a new entity";
                return result;
            }

            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene) {
                    return ResultEnvelope::Failure("editor.entity.create.undo", "scene", "An active scene is required before undoing entity creation");
                }

                if (!m_RuntimeEntity.IsValid()) {
                    return ResultEnvelope::Failure("editor.entity.create.undo", m_EntityName, "The created entity is no longer available");
                }

                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.entity.create.undo");
                }

                const std::array<uint32_t, 1> entityIds = { m_RuntimeEntity.GetUid() };
                auto result = operations->DeleteSceneEntities(*scene, entityIds);
                if (!result.Succeeded()) {
                    return result;
                }

                Selection::RemoveFromSelection(m_RuntimeEntity);
                Selection::RemoveInvalidSelections();
                m_RuntimeEntity = {};
                result.Operation = "editor.entity.create.undo";
                result.Target = m_EntityName;
                result.Summary = "Removed the created entity";
                return result;
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

                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.entity.delete");
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

                std::vector<uint32_t> entityIds;
                entityIds.reserve(m_RuntimeEntities.size());
                for (const auto& entity : m_RuntimeEntities) {
                    entityIds.push_back(entity.GetUid());
                }

                auto result = operations->DeleteSceneEntities(*scene, entityIds);
                if (!result.Succeeded()) {
                    return result;
                }

                Selection::ClearSelection();
                result.Operation = "editor.entity.delete";
                result.Target = "selection";
                result.Summary = m_Snapshots.size() > 1 ? "Deleted selected entities" : "Deleted selected entity";
                return result;
            }

            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene) {
                    return ResultEnvelope::Failure("editor.entity.delete.undo", "scene", "An active scene is required before undoing entity deletion");
                }

                if (!GetOperations(context)) {
                    return MakeOperationsMissingResult("editor.entity.delete.undo");
                }

                m_RuntimeEntities.clear();
                std::vector<Entity> restored;
                restored.reserve(m_Snapshots.size());
                for (const auto& snapshot : m_Snapshots) {
                    Entity entity;
                    auto restoreResult = RestoreEntitySnapshot(context, snapshot, entity);
                    if (!restoreResult.Succeeded()) {
                        return restoreResult;
                    }
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
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.add_camera");
                }
                auto result = operations->AddSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Camera);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.add_camera";
                result.Target = m_Entity.GetName();
                result.Summary = "Added CameraComponent";
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::CameraComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_camera.undo", "entity", "CameraComponent is no longer available to remove");
                }
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.add_camera.undo");
                }
                auto result = operations->RemoveSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Camera);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.add_camera.undo";
                result.Target = m_Entity.GetName();
                result.Summary = "Removed CameraComponent";
                return result;
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
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.remove_camera");
                }
                m_Component = m_Entity.GetComponent<Rendering::CameraComponent>();
                auto result = operations->RemoveSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Camera);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.remove_camera";
                result.Target = m_Entity.GetName();
                result.Summary = "Removed CameraComponent";
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.remove_camera.undo", "entity", "The selected entity is no longer available");
                }
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.remove_camera.undo");
                }
                auto result = operations->UpsertSceneCameraComponent(*scene, m_Entity.GetUid(), m_Component);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.remove_camera.undo";
                result.Target = m_Entity.GetName();
                result.Summary = "Restored CameraComponent";
                return result;
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
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.add_mesh");
                }
                auto result = operations->AddSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Mesh);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.add_mesh";
                result.Target = m_Entity.GetName();
                result.Summary = "Added MeshComponent";
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::MeshComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_mesh.undo", "entity", "MeshComponent is no longer available to remove");
                }
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.add_mesh.undo");
                }
                auto result = operations->RemoveSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Mesh);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.add_mesh.undo";
                result.Target = m_Entity.GetName();
                result.Summary = "Removed MeshComponent";
                return result;
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
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.remove_mesh");
                }
                m_Component = m_Entity.GetComponent<Rendering::MeshComponent>();
                auto result = operations->RemoveSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Mesh);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.remove_mesh";
                result.Target = m_Entity.GetName();
                result.Summary = "Removed MeshComponent";
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.remove_mesh.undo", "entity", "The selected entity is no longer available");
                }
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.remove_mesh.undo");
                }
                auto result = operations->UpsertSceneMeshComponent(*scene, m_Entity.GetUid(), m_Component);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.remove_mesh.undo";
                result.Target = m_Entity.GetName();
                result.Summary = "Restored MeshComponent";
                return result;
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
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.add_material");
                }
                auto result = operations->AddSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Material);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.add_material";
                result.Target = m_Entity.GetName();
                result.Summary = "Added MaterialComponent";
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid() || !m_Entity.HasComponent<Rendering::MaterialComponent>()) {
                    return ResultEnvelope::Failure("editor.component.add_material.undo", "entity", "MaterialComponent is no longer available to remove");
                }
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.add_material.undo");
                }
                auto result = operations->RemoveSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Material);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.add_material.undo";
                result.Target = m_Entity.GetName();
                result.Summary = "Removed MaterialComponent";
                return result;
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
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.remove_material");
                }
                m_Component = m_Entity.GetComponent<Rendering::MaterialComponent>();
                auto result = operations->RemoveSceneComponent(*scene, m_Entity.GetUid(), SceneComponentKind::Material);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.remove_material";
                result.Target = m_Entity.GetName();
                result.Summary = "Removed MaterialComponent";
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !m_Entity.IsValid()) {
                    return ResultEnvelope::Failure("editor.component.remove_material.undo", "entity", "The selected entity is no longer available");
                }
                auto* operations = GetOperations(context);
                if (!operations) {
                    return MakeOperationsMissingResult("editor.component.remove_material.undo");
                }
                auto result = operations->UpsertSceneMaterialComponent(*scene, m_Entity.GetUid(), m_Component);
                if (!result.Succeeded()) {
                    return result;
                }
                result.Operation = "editor.component.remove_material.undo";
                result.Target = m_Entity.GetName();
                result.Summary = "Restored MaterialComponent";
                return result;
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
