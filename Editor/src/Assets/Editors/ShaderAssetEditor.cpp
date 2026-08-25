#include "enginepch.h"
#include "ShaderAssetEditor.h"

#include "HuaEngine/Asset/Import/ShaderDescriptor.h"
#include "HuaEngine/Core/HostLaunch.h"
#include "HuaEngine/Core/Sha256.h"
#include "imgui.h"
#include <sstream>

namespace {
	const char* ValueTypeLabel(HE::Rendering::ShaderValueType type) {
		using HE::Rendering::ShaderValueType;
		switch (type) { case ShaderValueType::Int: return "Int"; case ShaderValueType::Float: return "Float"; case ShaderValueType::Float2: return "Float2"; case ShaderValueType::Float3: return "Float3"; case ShaderValueType::Float4: return "Float4"; case ShaderValueType::Float4x4: return "Float4x4"; case ShaderValueType::Texture2D: return "Texture2D"; case ShaderValueType::SamplerState: return "SamplerState"; }
		return "Unknown";
	}
	std::string DefaultValueLabel(const HE::Rendering::ShaderParameterMetadata& parameter) {
		using namespace HE::Rendering;
		std::ostringstream stream;
		switch (parameter.Type) {
		case ShaderValueType::Int: stream << std::get<int32_t>(parameter.DefaultValue); break;
		case ShaderValueType::Float: stream << std::get<float>(parameter.DefaultValue); break;
		case ShaderValueType::Float2: { const auto value = std::get<glm::vec2>(parameter.DefaultValue); stream << value.x << ", " << value.y; break; }
		case ShaderValueType::Float3: { const auto value = std::get<glm::vec3>(parameter.DefaultValue); stream << value.x << ", " << value.y << ", " << value.z; break; }
		case ShaderValueType::Float4: { const auto value = std::get<glm::vec4>(parameter.DefaultValue); stream << value.x << ", " << value.y << ", " << value.z << ", " << value.w; break; }
		case ShaderValueType::Float4x4: stream << "matrix"; break;
		case ShaderValueType::Texture2D: stream << std::get<std::string>(parameter.DefaultValue); break;
		case ShaderValueType::SamplerState: stream << "sampler"; break;
		}
		return stream.str();
	}
}

namespace HE::Editor {
	ResultEnvelope ShaderAssetEditor::Open(const AssetEditorOpenContext& context) { m_Snapshot = context.Snapshot; return ResultEnvelope::Success("asset.shader_editor.open", m_Snapshot.Asset.Guid, "Shader inspector opened"); }
	ResultEnvelope ShaderAssetEditor::Validate() const { return ResultEnvelope::Success("asset.shader_editor.validate", m_Snapshot.Asset.Guid, "Shader inspector is read-only"); }
	void ShaderAssetEditor::Draw(AssetEditorDrawContext& context) {
		ShaderDescriptor descriptor; const bool hasDescriptor = LoadShaderDescriptor(m_Snapshot.Asset.AbsolutePath, descriptor).Succeeded();
		if (ImGui::Button("Open Source")) {
			const auto path = hasDescriptor ? m_Snapshot.Asset.AbsolutePath.parent_path() / descriptor.Source : m_Snapshot.Asset.AbsolutePath;
			(void)HostLaunch::Open(path);
		}
		ImGui::SameLine(); if (ImGui::Button("Reimport") && context.ReimportAsset) (void)context.ReimportAsset(m_Snapshot.Asset.AbsolutePath);
		ImGui::SameLine(); if (ImGui::Button("Copy Diagnostics")) { std::string text; for (const auto& diagnostic : m_Snapshot.Diagnostics) text += diagnostic.Code + ": " + diagnostic.Message + "\n"; ImGui::SetClipboardText(text.c_str()); }
		if (hasDescriptor) { ImGui::TextWrapped("HLSL: %s", descriptor.Source.generic_string().c_str()); ImGui::Text("Vertex: %s (%s)", descriptor.Vertex.Entry.c_str(), descriptor.Vertex.Profile.c_str()); ImGui::Text("Fragment: %s (%s)", descriptor.Fragment.Entry.c_str(), descriptor.Fragment.Profile.c_str()); }
		if (const auto& shader = m_Snapshot.ShaderData) {
			ImGui::Text("Compiler: %s", shader->CompilerIdentity.c_str());
			for (const auto& stage : shader->Stages) ImGui::BulletText("%s: SPIR-V %s, GLSL %s, DXIL unavailable", stage.EntryPoint.c_str(), stage.Spirv.empty() ? "missing" : "available", stage.GeneratedOpenGlGlsl.empty() ? "missing" : "available");
			ImGui::Text("Interface signature: %llu", static_cast<unsigned long long>(shader->Interface.Gpu.Signature));
			ImGui::TextWrapped("Interface digest: %s", Sha256ToHex(shader->Interface.Gpu.Digest).c_str());
			for (const auto& buffer : shader->Interface.Gpu.ConstantBuffers) { if (ImGui::TreeNode(buffer.Name.c_str())) { ImGui::Text("set %u binding %u size %u", buffer.Set, buffer.Binding, buffer.Size); for (const auto& member : buffer.Members) ImGui::BulletText("%s offset %u size %u", member.Name.c_str(), member.Offset, member.Size); ImGui::TreePop(); } }
			for (const auto& resource : shader->Interface.Gpu.Resources) ImGui::BulletText("%s set %u binding %u stages 0x%02x", resource.Name.c_str(), resource.Set, resource.Binding, resource.StageMask);
			if (ImGui::TreeNode("Material Parameters")) {
				for (const auto& parameter : shader->Interface.Authoring.Parameters) {
					ImGui::BulletText("%s: %s", parameter.DisplayName.empty() ? parameter.Name.c_str() : parameter.DisplayName.c_str(), ValueTypeLabel(parameter.Type));
					ImGui::TextDisabled("default: %s", DefaultValueLabel(parameter).c_str());
					if (!parameter.Range.empty()) ImGui::TextDisabled("range %.3f .. %.3f, step %.3f", parameter.Range.front(), parameter.Range.back(), parameter.Step);
					if (!parameter.Tooltip.empty()) ImGui::TextWrapped("%s", parameter.Tooltip.c_str());
				}
				ImGui::TreePop();
			}
		}
		for (const auto& diagnostic : m_Snapshot.Diagnostics) ImGui::TextWrapped("%s: %s", diagnostic.Code.c_str(), diagnostic.Message.c_str());
	}
}
