#pragma once 

#include <typeindex>
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/World.h"
#include "HuaEngine/ECS/Components.h"
#include "Module/Rendering/RenderingComponent.h"
#include "ComponentEditor.h"


namespace HE {
    class ComponentEditorRegistry {
    public:
        struct DrawOptions {
            std::function<void(std::type_index)> RequestRemove;
            std::function<bool(std::type_index)> CanRemove;
        };

        using DrawFunction = std::function<bool(World&, EntityId, const DrawOptions&)>;

        struct ComponentInfo {
            std::string displayName;
            DrawFunction drawFunc;
        };

        template<typename T>
        void Register(const std::string& displayName) {
            ComponentInfo info;
            info.displayName = displayName;
            info.drawFunc = [displayName](World& world, EntityId entityId, const DrawOptions& options) {
                if (auto* component = world.TryGetComponent<T>(entityId)) {
                    const std::type_index componentType(typeid(T));
                    const bool open = ImGui::CollapsingHeader(displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                    if (options.RequestRemove && ImGui::BeginPopupContextItem((displayName + "ContextMenu").c_str())) {
                        const bool canRemove = options.CanRemove ? options.CanRemove(componentType) : false;
                        if (ImGui::MenuItem("Remove Component", nullptr, false, canRemove)) {
                            options.RequestRemove(componentType);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }

                    if (open) {
                        ImGui::Indent();
                        const bool changed = DrawComponentEditor(*component);
                        ImGui::Unindent();
                        return changed;
                    }
                }

                return false;
            };

            std::type_index index(typeid(T));
            components[index] = info;
            registeredTypes.push_back(index);
        }

        bool DrawComponents(World& world, EntityId entityId, const DrawOptions& options = {}) const {
            bool changed = false;
            for (const auto& type : registeredTypes) {
                const auto& info = components.at(type);
                changed |= info.drawFunc(world, entityId, options);
            }

            return changed;
        }

        const std::vector<std::type_index>& GetRegisteredTypes() const {
            return registeredTypes;
        }

        const ComponentInfo* GetComponentInfo(std::type_index type) const {
            auto it = components.find(type);
            return it != components.end() ? &it->second : nullptr;
        }

        static ComponentEditorRegistry& Instance() {
            static ComponentEditorRegistry instance;
            return instance;
        }

    private:
        ComponentEditorRegistry() = default;

    private:
        std::unordered_map<std::type_index, ComponentInfo> components;
        std::vector<std::type_index> registeredTypes;
    };

    template<typename T>
    struct ComponentEditorAutoRegister {
        ComponentEditorAutoRegister(const std::string& displayName) {
            if (!IsRegistered) {
                ComponentEditorRegistry::Instance().Register<T>(displayName);
                IsRegistered = true;
            }
        }
        inline static bool IsRegistered = false;
    };

    // 使用别名来避免宏问题
    using CameraComponent = Rendering::CameraComponent;
    using RendererComponent = Rendering::RendererComponent;
    using MeshComponent = Rendering::MeshComponent;
    using MaterialComponent = Rendering::MaterialComponent;

#define REGISTER_COMPONENT_EDITOR(Type) \
    static ComponentEditorAutoRegister<Type> s_autoRegister_##Type(#Type);

    REGISTER_COMPONENT_EDITOR(TransformComponent)
    REGISTER_COMPONENT_EDITOR(CameraComponent)
    REGISTER_COMPONENT_EDITOR(RendererComponent)
    REGISTER_COMPONENT_EDITOR(MeshComponent)
    REGISTER_COMPONENT_EDITOR(MaterialComponent)
}
