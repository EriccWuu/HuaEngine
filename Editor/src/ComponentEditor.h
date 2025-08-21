#pragma once

#include "glm/glm.hpp"
#include "imgui.h"

namespace HE {
    template<typename T>
    inline void DrawFieldEditor(std::string_view name, T* objPtr) {
        ImGui::Text("%s", name.data());
    }

    template<>
    inline void DrawFieldEditor<glm::vec3>(std::string_view name, glm::vec3* objPtr) {
        ImGui::PushID(name.data());

        ImGui::Text("%s", name.data());

        ImGui::SameLine();
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::DragScalarN("##Slider", ImGuiDataType_Float, objPtr, 3, 0.1f, NULL, NULL);
        ImGui::PopItemWidth();

        ImGui::PopID();
    }

    template<typename T>
    inline void DrawComponentEditor(T& trans) {
        auto typeInfo = Refl::reflect<T>();
        typeInfo.visit_fields([&trans](auto&& field) {
            using fieldType = std::decay_t<decltype(field)>::type;
            auto ptr = reinterpret_cast<fieldType*>(reinterpret_cast<char*>(&trans) + field.offset());
            DrawFieldEditor<fieldType>(field.name(), ptr);
        });
    }
}