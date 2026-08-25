#include "enginepch.h"
#include "AssetPreviewContext.h"

#include "imgui.h"

namespace HE::Editor {
	void AssetPreviewContext::DrawMaterialPreview() const {
		if (!ImGui::CollapsingHeader("Working Copy Preview")) return;
		ImGui::Text("Shader: %s", m_Material.ShaderGuid.empty() ? "None" : m_Material.ShaderGuid.c_str());
		for (const auto& [name, parameter] : m_Material.Parameters) {
			if (parameter.Type == Rendering::MaterialParameterType::Vec4) {
				const auto color = std::get<glm::vec4>(parameter.Value);
				ImGui::ColorButton(name.c_str(), { color.r, color.g, color.b, color.a }, ImGuiColorEditFlags_NoTooltip, { 48.0f, 24.0f });
				ImGui::SameLine();
				ImGui::TextUnformatted(name.c_str());
			}
		}
		ImGui::TextDisabled("Preview uses only the asset working copy and does not mutate the active scene.");
	}
}
