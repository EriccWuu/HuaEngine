#pragma once

#include "glm/glm.hpp"
#include "imgui.h"

namespace HE {
    template<typename T>
    inline bool DrawFieldEditor(std::string_view name, T* objPtr) {
        (void)objPtr;
        ImGui::Text("%s", name.data());
        return false;
    }

    template<>
    inline bool DrawFieldEditor<glm::vec3>(std::string_view name, glm::vec3* objPtr) {
        ImGui::PushID(name.data());

        ImGui::Text("%s", name.data());

        ImGui::SameLine();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        const bool changed = ImGui::DragScalarN("##Slider", ImGuiDataType_Float, objPtr, 3, 0.1f, NULL, NULL);
        ImGui::PopItemWidth();

        ImGui::PopID();
        return changed;
    }

    template<typename T>
    inline bool DrawComponentEditor(T& trans) {
        auto typeInfo = Refl::reflect<T>();
        bool changed = false;
        typeInfo.visit_fields([&trans, &changed](auto&& field) {
            using fieldType = std::decay_t<decltype(field)>::type;
            auto ptr = reinterpret_cast<fieldType*>(reinterpret_cast<char*>(&trans) + field.offset());
            changed |= DrawFieldEditor<fieldType>(field.name(), ptr);
        });

        return changed;
    }
}
