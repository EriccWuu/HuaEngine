#pragma once

#include <string>
#include <vector>

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/Runtime/World.h"
#include "Module/Rendering/RenderingComponent.h"
#include "Interaction/EditorCommand.h"

namespace HE {
    class Scene;
    enum class EditorInspectableComponent {
        Camera,
        Mesh,
        Material
    };

    struct EditorInspectableComponentDescriptor {
        EditorInspectableComponent Type;
        std::string Id;
        std::string DisplayName;
    };

    [[nodiscard]] const std::vector<EditorInspectableComponentDescriptor>& GetEditorInspectableComponents();
    [[nodiscard]] const EditorInspectableComponentDescriptor* FindEditorInspectableComponent(std::string_view id);
    [[nodiscard]] bool EntityHasInspectableComponent(EditorInspectableComponent type, const Ecs::World& world, EntityId entity);
    [[nodiscard]] bool CanRemoveInspectableComponent(EditorInspectableComponent type, const Ecs::World& world, EntityId entity);

    [[nodiscard]] EditorCommandPtr CreateCreateEntityCommand(std::string entityName);
    [[nodiscard]] EditorCommandPtr CreateSetComponentValueCommand(
        const Ref<Scene>& scene, EntityId entity, Ecs::OwnedValue before, Ecs::OwnedValue after);
    [[nodiscard]] EditorCommandPtr CreateDeleteEntitiesCommand(const Ecs::World& world, std::span<const EntityId> entities);
    [[nodiscard]] EditorCommandPtr CreateAddComponentCommand(EditorInspectableComponent type, const Ecs::World& world, EntityId entity);
    [[nodiscard]] EditorCommandPtr CreateRemoveComponentCommand(EditorInspectableComponent type, const Ecs::World& world, EntityId entity);
	[[nodiscard]] EditorCommandPtr CreateSetTransformCommand(
        const Ecs::World& world, EntityId entity,
        const TransformComponent& before,
		const TransformComponent& after);
	[[nodiscard]] EditorCommandPtr CreateSetMaterialOverridesCommand(
		const Ecs::World& world, EntityId entity,
		const Rendering::MaterialOverrideSet& before,
		const Rendering::MaterialOverrideSet& after);
	[[nodiscard]] EditorCommandPtr CreateSetMaterialComponentCommand(
		const Ecs::World& world, EntityId entity,
		const Rendering::MaterialComponent& before,
		const Rendering::MaterialComponent& after);
}
