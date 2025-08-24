#pragma once 

#include <typeindex>
#include "HuaEngine/ECS/Entity.h"
#include "HuaEngine/ECS/Components.h"
#include "Module/Rendering/RenderingComponent.h"
#include "ComponentEditor.h"


namespace HE {
    class ComponentEditorRegistry {
    public:
        using DrawFunction = std::function<void(entt::registry&, entt::entity)>;

        struct ComponentInfo {
            std::string displayName;
            DrawFunction drawFunc;
        };

        template<typename T>
        void Register(const std::string& displayName) {
            ComponentInfo info;
            info.displayName = displayName;
            info.drawFunc = [displayName](entt::registry& reg, entt::entity ent) {
                if (reg.all_of<T>(ent)) {
                    T& component = reg.get<T>(ent);
                    if (ImGui::CollapsingHeader(displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Indent();
                        DrawComponentEditor(component);
                        ImGui::Unindent();
                    }
                }
            };

            std::type_index index(typeid(T));
            components[index] = info;
            registeredTypes.push_back(index);
        }

        void DrawComponents(entt::registry& registry, entt::entity entity) const {
            for (const auto& type : registeredTypes) {
                const auto& info = components.at(type);
                info.drawFunc(registry, entity);
            }
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

#define REGISTER_COMPONENT_EDITOR(Type) \
    static ComponentEditorAutoRegister<Type> s_autoRegister_##Type(#Type);

    REGISTER_COMPONENT_EDITOR(TransformComponent)
    REGISTER_COMPONENT_EDITOR(CameraComponent)
    REGISTER_COMPONENT_EDITOR(RendererComponent)
    REGISTER_COMPONENT_EDITOR(MeshComponent)
}