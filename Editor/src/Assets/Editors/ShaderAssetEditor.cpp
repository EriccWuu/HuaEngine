#include "enginepch.h"
#include "ShaderAssetEditor.h"

#include "HuaEngine/Asset/Import/ShaderDescriptor.h"
#include "imgui.h"
#ifdef HE_PLATFORM_WINDOWS
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
#endif

namespace HE::Editor {
	ResultEnvelope ShaderAssetEditor::Open(const AssetEditorOpenContext& context) { m_Snapshot = context.Snapshot; return ResultEnvelope::Success("asset.shader_editor.open", m_Snapshot.Asset.Guid, "Shader inspector opened"); }
	ResultEnvelope ShaderAssetEditor::Validate() const { return ResultEnvelope::Success("asset.shader_editor.validate", m_Snapshot.Asset.Guid, "Shader inspector is read-only"); }
	void ShaderAssetEditor::Draw(AssetEditorDrawContext& context) {
		ShaderDescriptor descriptor; const bool hasDescriptor = LoadShaderDescriptor(m_Snapshot.Asset.AbsolutePath, descriptor).Succeeded();
		if (ImGui::Button("Open Source")) {
			auto path = hasDescriptor ? m_Snapshot.Asset.AbsolutePath.parent_path() / descriptor.Source : m_Snapshot.Asset.AbsolutePath;
#ifdef HE_PLATFORM_WINDOWS
			ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, path.parent_path().wstring().c_str(), SW_SHOWNORMAL);
#endif
		}
		ImGui::SameLine(); if (ImGui::Button("Reimport") && context.ReimportAsset) (void)context.ReimportAsset(m_Snapshot.Asset.AbsolutePath);
		ImGui::SameLine(); if (ImGui::Button("Copy Diagnostics")) { std::string text; for (const auto& diagnostic : m_Snapshot.Diagnostics) text += diagnostic.Code + ": " + diagnostic.Message + "\n"; ImGui::SetClipboardText(text.c_str()); }
		if (hasDescriptor) { ImGui::TextWrapped("HLSL: %s", descriptor.Source.generic_string().c_str()); ImGui::Text("Vertex: %s (%s)", descriptor.Vertex.Entry.c_str(), descriptor.Vertex.Profile.c_str()); ImGui::Text("Fragment: %s (%s)", descriptor.Fragment.Entry.c_str(), descriptor.Fragment.Profile.c_str()); }
		if (const auto& shader = m_Snapshot.ShaderData) {
			ImGui::Text("Compiler: %s", shader->CompilerIdentity.c_str());
			for (const auto& stage : shader->Stages) ImGui::BulletText("%s: SPIR-V %s, GLSL %s", stage.EntryPoint.c_str(), stage.Spirv.empty() ? "missing" : "available", stage.GeneratedOpenGlGlsl.empty() ? "missing" : "available");
			ImGui::Text("Interface signature: %llu", static_cast<unsigned long long>(shader->Interface.Gpu.Signature));
			for (const auto& buffer : shader->Interface.Gpu.ConstantBuffers) { if (ImGui::TreeNode(buffer.Name.c_str())) { ImGui::Text("set %u binding %u size %u", buffer.Set, buffer.Binding, buffer.Size); for (const auto& member : buffer.Members) ImGui::BulletText("%s offset %u size %u", member.Name.c_str(), member.Offset, member.Size); ImGui::TreePop(); } }
			for (const auto& resource : shader->Interface.Gpu.Resources) ImGui::BulletText("%s set %u binding %u", resource.Name.c_str(), resource.Set, resource.Binding);
			ImGui::Text("Material parameters: %zu", shader->Interface.Authoring.Parameters.size());
		}
		for (const auto& diagnostic : m_Snapshot.Diagnostics) ImGui::TextWrapped("%s: %s", diagnostic.Code.c_str(), diagnostic.Message.c_str());
	}
}
