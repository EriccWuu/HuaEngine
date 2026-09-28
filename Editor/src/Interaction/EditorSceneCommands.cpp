#include "enginepch.h"
#include "Interaction/EditorSceneCommands.h"
#include "HuaEngine/Application/ApplicationOperations.h"
#include "HuaEngine/Application/EcsResultEnvelope.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "Selection.h"
#include "Workbench/SceneDocument.h"
#include <array>
#include <optional>
#include <variant>

namespace HE {
    namespace {
        Ref<Scene> GetScene(const EditorCommandContext& context) {
            return context.SceneDocument ? context.SceneDocument->SceneRef : nullptr;
        }
        ResultEnvelope Unavailable(std::string operation) {
            return ResultEnvelope::Failure(std::move(operation), "entity", "The scene, entity, or application operations are unavailable");
        }
        EntityId Resolve(const Ref<Scene>& scene, Ecs::WorldId world, EntityUuid uuid) {
            return scene && scene->GetWorld().Id() == world ? scene->GetWorld().Find(uuid) : EntityId{};
        }
        SceneComponentKind Kind(EditorInspectableComponent kind) {
            switch (kind) {
            case EditorInspectableComponent::Camera: return SceneComponentKind::Camera;
            case EditorInspectableComponent::Mesh: return SceneComponentKind::Mesh;
            case EditorInspectableComponent::Material: return SceneComponentKind::Material;
            }
            return static_cast<SceneComponentKind>(-1);
        }
        using ComponentValue = std::variant<Rendering::CameraComponent, Rendering::MeshComponent, Rendering::MaterialComponent>;
        ResultEnvelope Upsert(ApplicationOperations& operations, Scene& scene, EntityId entity, const ComponentValue& value) {
            return std::visit([&](const auto& component) -> ResultEnvelope {
                using T = std::decay_t<decltype(component)>;
                if constexpr (std::is_same_v<T, Rendering::CameraComponent>) return operations.UpsertSceneCameraComponent(scene, entity.Index, component);
                else if constexpr (std::is_same_v<T, Rendering::MeshComponent>) return operations.UpsertSceneMeshComponent(scene, entity.Index, component);
                else return operations.UpsertSceneMaterialComponent(scene, entity.Index, component);
            }, value);
        }

        struct EntitySnapshot {
            EntityUuid Uuid;
            std::string Name;
            bool Enabled = true;
            std::vector<Ecs::OwnedValue> Components;
            std::vector<Ecs::TypeId> Tags;
            std::vector<Ecs::TypeId> Disabled;
            std::vector<Ecs::SharedBinding> Shared;
        };
        Ecs::Result<EntitySnapshot> Capture(const Ecs::World& world, EntityId entity) {
            EntitySnapshot result;
            result.Uuid = world.Uuid(entity);
            result.Name = world.Name(entity);
            result.Enabled = world.IsEnabled(entity);
            for (const auto id : world.ListTypes(entity)) {
                const auto* type = world.Types().Find(id);
                if (!type) return Ecs::Error{Ecs::ErrorCode::InvalidType, "CaptureEntity", "The component type is not registered"};
                if (type->Descriptor.Storage == Ecs::StorageKind::Tag) result.Tags.push_back(id);
                else {
                    auto value = Ecs::OwnedValue::Copy(*type, world.TryGet(entity, id));
                    if (!value) return value.GetError();
                    result.Components.push_back(std::move(value).Value());
                }
                if (!world.IsComponentEnabled(entity, id)) result.Disabled.push_back(id);
            }
            const auto location = world.Location(entity);
            auto chunk = world.InspectChunk({location.Chunk, location.ChunkGeneration});
            if (!chunk) return chunk.GetError();
            result.Shared.assign(chunk.Value().Shared.begin(), chunk.Value().Shared.end());
            return result;
        }
        Ecs::Result<EntityId> Restore(Ecs::World& world, const EntitySnapshot& snapshot) {
            if (world.IsAlive(world.Find(snapshot.Uuid))) return Ecs::Error{Ecs::ErrorCode::InvalidEntity, "RestoreEntity", "The original UUID is already in use"};
            std::vector<Ecs::OwnedValue> values;
            values.reserve(snapshot.Components.size());
            for (const auto& component : snapshot.Components) {
                auto value = component.Clone();
                if (!value) return value.GetError();
                values.push_back(std::move(value).Value());
            }
            auto created = world.CreateEmpty(snapshot.Name, snapshot.Uuid);
            if (!created) return created.GetError();
            const auto entity = created.Value();
            auto fail = [&](const Ecs::Error& error) -> Ecs::Result<EntityId> { (void)world.Destroy(entity); return error; };
            for (auto& component : values) {
                auto set = world.Set(entity, std::move(component));
                if (!set) return fail(set.GetError());
            }
            for (const auto type : snapshot.Tags) {
                auto set = world.SetTag(entity, type);
                if (!set) return fail(set.GetError());
            }
            for (const auto& binding : snapshot.Shared) {
                auto set = world.SetShared(entity, binding);
                if (!set) return fail(set.GetError());
            }
            for (const auto type : snapshot.Disabled) {
                auto set = world.SetComponentEnabled(entity, type, false);
                if (!set) return fail(set.GetError());
            }
            auto enabled = world.SetEnabled(entity, snapshot.Enabled);
            if (!enabled) return fail(enabled.GetError());
            return entity;
        }

        class CreateEntityCommand final : public IEditorCommand {
        public:
            explicit CreateEntityCommand(std::string name) : m_Name(std::move(name)) {}
            std::string GetLabel() const override { return "Create Entity"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !context.Operations || (m_World != 0 && m_World != scene->GetWorld().Id())) return Unavailable("editor.entity.create");
                if (m_Uuid != EntityUuid{} && scene->GetWorld().IsAlive(scene->GetWorld().Find(m_Uuid))) return ResultEnvelope::Failure("editor.entity.create", m_Name, "The original UUID is already in use");
                uint32_t index = 0;
                auto result = context.Operations->CreateSceneEntity(*scene, m_Name, &index, m_Uuid);
                if (!result.Succeeded()) return result;
                m_World = scene->GetWorld().Id();
                m_Uuid = scene->GetWorld().Uuid(scene->GetWorld().FindByIndex(index));
                Selection::SetSelectedEntity(m_Uuid);
                result.Operation = "editor.entity.create"; result.Target = m_Name; result.Summary = "Created a new entity";
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                const auto entity = Resolve(scene, m_World, m_Uuid);
                if (!scene || !context.Operations || !scene->GetWorld().IsAlive(entity)) return Unavailable("editor.entity.create.undo");
                const std::array<uint32_t, 1> indices{entity.Index};
                auto result = context.Operations->DeleteSceneEntities(*scene, indices);
                if (result.Succeeded()) {
                    Selection::ClearSelection();
                    result.Operation = "editor.entity.create.undo"; result.Target = m_Name; result.Summary = "Removed the created entity";
                }
                return result;
            }
        private:
            std::string m_Name;
            Ecs::WorldId m_World = 0;
            EntityUuid m_Uuid;
        };

        class DeleteEntitiesCommand final : public IEditorCommand {
        public:
            DeleteEntitiesCommand(const Ecs::World& world, std::span<const EntityId> entities) : m_World(world.Id()) {
                for (auto entity : entities) if (world.IsAlive(entity)) m_Uuids.push_back(world.Uuid(entity));
            }
            std::string GetLabel() const override { return m_Uuids.size() > 1 ? "Delete Entities" : "Delete Entity"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !context.Operations || scene->GetWorld().Id() != m_World) return Unavailable("editor.entity.delete");
                std::vector<uint32_t> indices;
                {
                    auto scope = Ecs::WorldReadScope::Acquire(scene->GetWorld());
                    if (!scope) return EcsFailureEnvelope("editor.entity.delete", "selection", scope.GetError());
                    const auto& world = scope.Value().Get();
                    std::vector<EntitySnapshot> snapshots;
                    for (const auto uuid : m_Uuids) {
                        const auto entity = world.Find(uuid);
                        if (!world.IsAlive(entity)) continue;
                        indices.push_back(entity.Index);
                        if (!m_Initialized) {
                            auto captured = Capture(world, entity);
                            if (!captured) return EcsFailureEnvelope("editor.entity.delete", std::string(world.Name(entity)), captured.GetError());
                            snapshots.push_back(std::move(captured).Value());
                        }
                    }
                    if (!m_Initialized) {
                        // Owned snapshots borrow registered descriptors; retain their owning scene.
                        m_SnapshotOwner = scene;
                        m_Snapshots = std::move(snapshots);
                        m_Initialized = true;
                    }
                }
                auto result = context.Operations->DeleteSceneEntities(*scene, indices);
                if (result.Succeeded()) { Selection::ClearSelection(); result.Operation = "editor.entity.delete"; result.Target = "selection"; }
                return result;
            }
            ResultEnvelope Undo(const EditorCommandContext& context) override {
                auto scene = GetScene(context);
                if (!scene || !context.Operations || scene->GetWorld().Id() != m_World || !m_Initialized) return Unavailable("editor.entity.delete.undo");
                auto scope = Ecs::WorldEditScope::Acquire(scene->GetWorld());
                if (!scope) return EcsFailureEnvelope("editor.entity.delete.undo", "selection", scope.GetError());
                auto& world = scope.Value().Get();
                std::vector<EntityId> restored;
                restored.reserve(m_Snapshots.size());
                for (const auto& snapshot : m_Snapshots) {
                    auto entity = Restore(world, snapshot);
                    if (!entity) {
                        for (auto previous : restored) (void)world.Destroy(previous);
                        return EcsFailureEnvelope("editor.entity.delete.undo", snapshot.Name, entity.GetError());
                    }
                    restored.push_back(entity.Value());
                }
                Selection::SetSelections(world, restored);
                return ResultEnvelope::Success("editor.entity.delete.undo", "selection", restored.size() > 1 ? "Restored deleted entities" : "Restored deleted entity");
            }
        private:
            Ecs::WorldId m_World;
            std::vector<EntityUuid> m_Uuids;
            Ref<Scene> m_SnapshotOwner;
            std::vector<EntitySnapshot> m_Snapshots;
            bool m_Initialized = false;
        };

        class ComponentCommand final : public IEditorCommand {
        public:
            ComponentCommand(EditorInspectableComponent kind, const Ecs::World& world, EntityId entity, bool remove)
                : m_Kind(kind), m_World(world.Id()), m_Uuid(world.Uuid(entity)), m_Remove(remove) {}
            std::string GetLabel() const override {
                for (const auto& descriptor : GetEditorInspectableComponents()) {
                    if (descriptor.Type == m_Kind) return std::string(m_Remove ? "Remove " : "Add ") + descriptor.DisplayName + " Component";
                }
                return "Edit Component";
            }
            ResultEnvelope Execute(const EditorCommandContext& context) override { return Apply(context, false); }
            ResultEnvelope Undo(const EditorCommandContext& context) override { return Apply(context, true); }
        private:
            ResultEnvelope Apply(const EditorCommandContext& context, bool undo) {
                const auto operation = std::string("editor.component.") + (m_Remove ? "remove_" : "add_") + std::string(ToString(Kind(m_Kind))) + (undo ? ".undo" : "");
                auto scene = GetScene(context);
                const auto entity = Resolve(scene, m_World, m_Uuid);
                if (!scene || !context.Operations || !scene->GetWorld().IsAlive(entity)) return Unavailable(operation);
                std::string name;
                if (m_Remove && !undo) {
                    auto scope = Ecs::WorldReadScope::Acquire(scene->GetWorld());
                    if (!scope) return EcsFailureEnvelope(operation, "entity", scope.GetError());
                    const auto& world = scope.Value().Get();
                    name = world.Name(entity);
                    m_Before.reset();
                    switch (m_Kind) {
                    case EditorInspectableComponent::Camera: if (auto* value = world.TryGet<Rendering::CameraComponent>(entity)) m_Before = *value; break;
                    case EditorInspectableComponent::Mesh: if (auto* value = world.TryGet<Rendering::MeshComponent>(entity)) m_Before = *value; break;
                    case EditorInspectableComponent::Material: if (auto* value = world.TryGet<Rendering::MaterialComponent>(entity)) m_Before = *value; break;
                    }
                    if (!m_Before) return Unavailable(operation);
                } else name = scene->GetWorld().Name(entity);
                if (m_Remove && undo && !m_Before) return Unavailable(operation);
                auto result = m_Remove && undo ? Upsert(*context.Operations, *scene, entity, *m_Before)
                    : (m_Remove != undo ? context.Operations->RemoveSceneComponent(*scene, entity.Index, Kind(m_Kind))
                        : context.Operations->AddSceneComponent(*scene, entity.Index, Kind(m_Kind)));
                if (result.Succeeded()) { result.Operation = operation; result.Target = name; }
                return result;
            }
            EditorInspectableComponent m_Kind;
            Ecs::WorldId m_World;
            EntityUuid m_Uuid;
            bool m_Remove;
            std::optional<ComponentValue> m_Before;
        };

        class SetComponentValueCommand final : public IEditorCommand {
        public:
            SetComponentValueCommand(Ref<Scene> scene, EntityId entity, Ecs::OwnedValue before, Ecs::OwnedValue after)
                : m_Owner(std::move(scene)), m_Uuid(m_Owner->GetWorld().Uuid(entity)),
                  m_Before(std::move(before)), m_After(std::move(after)) {}
            std::string GetLabel() const override { return "Edit Component"; }
            ResultEnvelope Execute(const EditorCommandContext& context) override { return Apply(context, m_After, "editor.component.edit"); }
            ResultEnvelope Undo(const EditorCommandContext& context) override { return Apply(context, m_Before, "editor.component.edit.undo"); }
        private:
            ResultEnvelope Apply(const EditorCommandContext& context, const Ecs::OwnedValue& value, const std::string& operation) const {
                auto scene = GetScene(context);
                if (!scene || scene != m_Owner || !context.Operations || !value) return Unavailable(operation);
                auto scope = Ecs::WorldEditScope::Acquire(scene->GetWorld());
                if (!scope) return EcsFailureEnvelope(operation, "entity", scope.GetError());
                auto& world = scope.Value().Get();
                const auto entity = world.Find(m_Uuid);
                if (!world.IsAlive(entity) || !world.Has(entity, value.Type()->Id)) return Unavailable(operation);
                auto copy = value.Clone();
                if (!copy) return EcsFailureEnvelope(operation, "entity", copy.GetError());
                auto edited = world.Set(entity, std::move(copy).Value());
                if (!edited) return EcsFailureEnvelope(operation, "entity", edited.GetError());
                return ResultEnvelope::Success(operation, std::string(world.Name(entity)), "Component fields updated");
            }
            Ref<Scene> m_Owner;
            EntityUuid m_Uuid;
            Ecs::OwnedValue m_Before, m_After;
        };

        template<typename T>
        class SetValueCommand final : public IEditorCommand {
        public:
            SetValueCommand(const Ecs::World& world, EntityId entity, T before, T after, std::string operation, std::string label)
                : m_World(world.Id()), m_Uuid(world.Uuid(entity)), m_Before(std::move(before)), m_After(std::move(after)), m_Operation(std::move(operation)), m_Label(std::move(label)) {}
            std::string GetLabel() const override { return m_Label; }
            ResultEnvelope Execute(const EditorCommandContext& context) override { return Apply(context, m_After, m_Operation); }
            ResultEnvelope Undo(const EditorCommandContext& context) override { return Apply(context, m_Before, m_Operation + ".undo"); }
        private:
            ResultEnvelope Apply(const EditorCommandContext& context, const T& value, const std::string& operation) const {
                auto scene = GetScene(context);
                const auto entity = Resolve(scene, m_World, m_Uuid);
                if (!scene || !context.Operations || !scene->GetWorld().IsAlive(entity)) return Unavailable(operation);
                auto result = [&]() -> ResultEnvelope {
                    if constexpr (std::is_same_v<T, TransformComponent>) return context.Operations->UpsertSceneEntityTransform(*scene, entity.Index, value);
                    else if constexpr (std::is_same_v<T, Rendering::MaterialComponent>) return context.Operations->UpsertSceneMaterialComponent(*scene, entity.Index, value);
                    else {
                        Rendering::MaterialComponent component;
                        {
                            auto scope = Ecs::WorldReadScope::Acquire(scene->GetWorld());
                            if (!scope) return EcsFailureEnvelope(operation, "entity", scope.GetError());
                            const auto* current = scope.Value().Get().TryGet<Rendering::MaterialComponent>(entity);
                            if (!current) return Unavailable(operation);
                            component = *current;
                        }
                        component.Overrides = value;
                        return context.Operations->UpsertSceneMaterialComponent(*scene, entity.Index, component);
                    }
                }();
                if (result.Succeeded()) { result.Operation = operation; result.Target = std::string(scene->GetWorld().Name(entity)); }
                return result;
            }
            Ecs::WorldId m_World;
            EntityUuid m_Uuid;
            T m_Before, m_After;
            std::string m_Operation, m_Label;
        };
    }

    const std::vector<EditorInspectableComponentDescriptor>& GetEditorInspectableComponents() {
        static const std::vector<EditorInspectableComponentDescriptor> components{
            {EditorInspectableComponent::Camera, "component.camera", "Camera"},
            {EditorInspectableComponent::Mesh, "component.mesh", "Mesh"},
            {EditorInspectableComponent::Material, "component.material", "Material"}};
        return components;
    }
    const EditorInspectableComponentDescriptor* FindEditorInspectableComponent(std::string_view id) {
        for (const auto& component : GetEditorInspectableComponents()) if (component.Id == id) return &component;
        return nullptr;
    }
    bool EntityHasInspectableComponent(EditorInspectableComponent type, const Ecs::World& world, EntityId entity) {
        if (!world.IsAlive(entity)) return false;
        switch (type) {
        case EditorInspectableComponent::Camera: return world.Has<Rendering::CameraComponent>(entity);
        case EditorInspectableComponent::Mesh: return world.Has<Rendering::MeshComponent>(entity);
        case EditorInspectableComponent::Material: return world.Has<Rendering::MaterialComponent>(entity);
        }
        return false;
    }
    bool CanRemoveInspectableComponent(EditorInspectableComponent type, const Ecs::World& world, EntityId entity) { return EntityHasInspectableComponent(type, world, entity); }
    EditorCommandPtr CreateCreateEntityCommand(std::string name) { return std::make_unique<CreateEntityCommand>(std::move(name)); }
    EditorCommandPtr CreateSetComponentValueCommand(const Ref<Scene>& scene, EntityId entity, Ecs::OwnedValue before, Ecs::OwnedValue after) {
        if (!scene || !before || !after || before.Type() != after.Type() ||
            !scene->GetWorld().Types().Owns(*before.Type()) || !scene->GetWorld().IsAlive(entity)) return nullptr;
        return std::make_unique<SetComponentValueCommand>(scene, entity, std::move(before), std::move(after));
    }
    EditorCommandPtr CreateDeleteEntitiesCommand(const Ecs::World& world, std::span<const EntityId> entities) { return std::make_unique<DeleteEntitiesCommand>(world, entities); }
    EditorCommandPtr CreateAddComponentCommand(EditorInspectableComponent type, const Ecs::World& world, EntityId entity) { return std::make_unique<ComponentCommand>(type, world, entity, false); }
    EditorCommandPtr CreateRemoveComponentCommand(EditorInspectableComponent type, const Ecs::World& world, EntityId entity) { return std::make_unique<ComponentCommand>(type, world, entity, true); }
    EditorCommandPtr CreateSetTransformCommand(const Ecs::World& world, EntityId entity, const TransformComponent& before, const TransformComponent& after) { return std::make_unique<SetValueCommand<TransformComponent>>(world, entity, before, after, "editor.transform", "Transform Entity"); }
    EditorCommandPtr CreateSetMaterialOverridesCommand(const Ecs::World& world, EntityId entity, const Rendering::MaterialOverrideSet& before, const Rendering::MaterialOverrideSet& after) { return std::make_unique<SetValueCommand<Rendering::MaterialOverrideSet>>(world, entity, before, after, "editor.material_override", "Edit Material Override"); }
    EditorCommandPtr CreateSetMaterialComponentCommand(const Ecs::World& world, EntityId entity, const Rendering::MaterialComponent& before, const Rendering::MaterialComponent& after) { return std::make_unique<SetValueCommand<Rendering::MaterialComponent>>(world, entity, before, after, "editor.material", "Change Material"); }
}
