#pragma once

#include <string>
#include <vector>

#include "HuaEngine.h"
#include "Interaction/EditorCommand.h"

namespace HE {
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
    [[nodiscard]] bool EntityHasInspectableComponent(EditorInspectableComponent type, const Entity& entity);
    [[nodiscard]] bool CanRemoveInspectableComponent(EditorInspectableComponent type, const Entity& entity);

    [[nodiscard]] EditorCommandPtr CreateCreateEntityCommand(std::string entityName);
    [[nodiscard]] EditorCommandPtr CreateDeleteEntitiesCommand(const std::vector<Entity>& entities);
    [[nodiscard]] EditorCommandPtr CreateAddComponentCommand(EditorInspectableComponent type, const Entity& entity);
    [[nodiscard]] EditorCommandPtr CreateRemoveComponentCommand(EditorInspectableComponent type, const Entity& entity);
}
