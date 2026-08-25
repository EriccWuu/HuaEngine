#include "enginepch.h"
#include "MaterialAssetEditor.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "imgui.h"

namespace {
	template<size_t Size>
	bool DrawText(const char* label, std::string& value) {
		std::array<char, Size> buffer{};
		std::memcpy(buffer.data(), value.data(), std::min(value.size(), Size - 1));
		if (!ImGui::InputText(label, buffer.data(), buffer.size())) return false;
		value = buffer.data();
		return true;
	}

	bool DrawParameter(HE::Rendering::MaterialSourceParameter& parameter) {
		using namespace HE::Rendering;
		switch (parameter.Type) {
		case MaterialParameterType::Int:
			return ImGui::DragInt(parameter.Name.c_str(), &std::get<int>(parameter.Value));
		case MaterialParameterType::Float:
			return ImGui::DragFloat(parameter.Name.c_str(), &std::get<float>(parameter.Value), 0.01f);
		case MaterialParameterType::Vec2:
			return ImGui::DragFloat2(parameter.Name.c_str(), &std::get<glm::vec2>(parameter.Value).x, 0.01f);
		case MaterialParameterType::Vec3:
			return ImGui::DragFloat3(parameter.Name.c_str(), &std::get<glm::vec3>(parameter.Value).x, 0.01f);
		case MaterialParameterType::Vec4:
			return ImGui::ColorEdit4(parameter.Name.c_str(), &std::get<glm::vec4>(parameter.Value).x);
		case MaterialParameterType::Texture2D:
			return DrawText<256>(parameter.Name.c_str(), std::get<std::string>(parameter.Value));
		default:
			ImGui::TextDisabled("%s is not editable in the first material inspector", parameter.Name.c_str());
			return false;
		}
	}

	bool DrawAssetPicker(const char* label, std::string& guid, std::span<const HE::Editor::AssetPickerOption> options) {
		const auto selected = std::find_if(options.begin(), options.end(), [&](const auto& option) { return option.Guid == guid; });
		const char* preview = selected == options.end() ? (guid.empty() ? "None" : guid.c_str()) : selected->DisplayName.c_str();
		bool changed = false;
		if (ImGui::BeginCombo(label, preview)) {
			for (const auto& option : options) {
				const bool isSelected = option.Guid == guid;
				if (ImGui::Selectable(option.DisplayName.c_str(), isSelected)) {
					guid = option.Guid;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}
}

namespace HE::Editor {
	ResultEnvelope MaterialAssetEditor::Open(const AssetEditorOpenContext& context) {
		m_Snapshot = context.Snapshot;
		auto result = Rendering::LoadMaterialSourceData(m_Snapshot.Asset.AbsolutePath, m_Baseline);
		if (!result.Succeeded()) return result;
		m_WorkingCopy = m_Baseline;
		return ResultEnvelope::Success("asset.material_editor.open", m_Snapshot.Asset.Guid, "Material editor opened");
	}

	void MaterialAssetEditor::Draw(AssetEditorDrawContext& context) {
		ImGui::TextUnformatted("Material Source");
		ImGui::Separator();
		(void)DrawText<256>("Name", m_WorkingCopy.Name);
		(void)DrawAssetPicker("Shader", m_WorkingCopy.ShaderGuid, context.ShaderAssets);
		std::vector<std::string> names;
		for (const auto& [name, parameter] : m_WorkingCopy.Parameters) names.push_back(name);
		std::sort(names.begin(), names.end());
		for (const auto& name : names) {
			auto& parameter = m_WorkingCopy.Parameters.at(name);
			if (parameter.Type == Rendering::MaterialParameterType::Texture2D) (void)DrawAssetPicker(parameter.Name.c_str(), std::get<std::string>(parameter.Value), context.TextureAssets);
			else (void)DrawParameter(parameter);
		}
	}

	ResultEnvelope MaterialAssetEditor::Validate() const {
		std::string text;
		auto result = Rendering::EncodeMaterialSourceData(m_WorkingCopy, text);
		if (!result.Succeeded()) result.Operation = "asset.edit.validation_failed";
		return result;
	}

	AssetEditCommit MaterialAssetEditor::BuildCommit() const {
		std::string text;
		if (!Rendering::EncodeMaterialSourceData(m_WorkingCopy, text).Succeeded()) return { .Guid = m_Snapshot.Asset.Guid };
		return {
			.Guid = m_Snapshot.Asset.Guid,
			.Target = AssetEditTarget::Source,
			.ExpectedSourceHash = m_Snapshot.SourceContentHash,
			.ExpectedMetaHash = m_Snapshot.MetaContentHash,
			.SerializedContent = std::vector<uint8_t>(text.begin(), text.end())
		};
	}

	bool MaterialAssetEditor::IsDirty() const {
		std::string baseline;
		std::string working;
		return !Rendering::EncodeMaterialSourceData(m_Baseline, baseline).Succeeded() ||
			!Rendering::EncodeMaterialSourceData(m_WorkingCopy, working).Succeeded() || baseline != working;
	}

	void MaterialAssetEditor::Revert() {
		m_WorkingCopy = m_Baseline;
	}
}
