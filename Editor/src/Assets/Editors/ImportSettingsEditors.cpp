#include "enginepch.h"
#include "ImportSettingsEditors.h"

#include "HuaEngine/Asset/Import/ObjMeshImporter.h"
#include "HuaEngine/Asset/Import/PngTextureImporter.h"
#include "imgui.h"

namespace {
	template<typename Importer, typename Settings>
	HE::ResultEnvelope OpenSettings(const HE::AssetInspectionSnapshot& snapshot, HE::AssetMeta& meta, Settings& baseline, Settings& working) {
		auto result = HE::LoadAssetMeta(snapshot.Asset.AbsolutePath, meta); if (!result.Succeeded()) return result;
		Importer importer; std::unique_ptr<HE::AssetImportSettings> decoded;
		result = importer.DecodeSettings(meta.Settings, decoded); if (!result.Succeeded()) return result;
		auto* typed = dynamic_cast<Settings*>(decoded.get()); if (!typed) return HE::ResultEnvelope::Failure("asset.editor.open", snapshot.Asset.Guid, "Import settings type is invalid");
		baseline = working = *typed; return HE::ResultEnvelope::Success("asset.editor.open", snapshot.Asset.Guid, "Import settings editor opened");
	}
	template<typename Importer, typename Settings>
	HE::AssetEditCommit BuildSettingsCommit(const HE::AssetInspectionSnapshot& snapshot, HE::AssetMeta meta, const Settings& settings) {
		Importer importer; if (!importer.EncodeSettings(settings, meta.Settings).Succeeded()) return { .Guid = snapshot.Asset.Guid };
		std::string text; if (!HE::EncodeAssetMeta(meta, text).Succeeded()) return { .Guid = snapshot.Asset.Guid };
		return { .Guid = snapshot.Asset.Guid, .Target = HE::AssetEditTarget::Metadata, .ExpectedSourceHash = snapshot.SourceContentHash, .ExpectedMetaHash = snapshot.MetaContentHash, .SerializedContent = { text.begin(), text.end() } };
	}
}

namespace HE::Editor {
	ResultEnvelope ObjMeshImportEditor::Open(const AssetEditorOpenContext& context) { m_Snapshot = context.Snapshot; return OpenSettings<ObjMeshImporter>(m_Snapshot, m_Meta, m_Baseline, m_Working); }
	ResultEnvelope ObjMeshImportEditor::Validate() const { return ObjMeshImporter().ValidateSettings(m_Working); }
	AssetEditCommit ObjMeshImportEditor::BuildCommit() const { return BuildSettingsCommit<ObjMeshImporter>(m_Snapshot, m_Meta, m_Working); }
	void ObjMeshImportEditor::Draw(AssetEditorDrawContext&) {
		ImGui::DragFloat("Import Scale", &m_Working.ImportScale, 0.01f, 0.001f, 1000.0f);
		const char* axes[] = { "X", "-X", "Y", "-Y", "Z", "-Z" }; int up = static_cast<int>(m_Working.UpAxis) / 2, forward = static_cast<int>(m_Working.ForwardAxis);
		const char* upAxes[] = { "X", "Y", "Z" };
		if (ImGui::Combo("Up Axis", &up, upAxes, 3)) m_Working.UpAxis = static_cast<MeshAxis>(up * 2);
		if (ImGui::Combo("Forward Axis", &forward, axes, 6)) m_Working.ForwardAxis = static_cast<MeshAxis>(forward);
		ImGui::Checkbox("Flip UV V", &m_Working.FlipUvV); ImGui::Checkbox("Generate Missing Normals", &m_Working.GenerateNormalsWhenMissing);
		ImGui::Checkbox("Recalculate Normals", &m_Working.RecalculateNormals); ImGui::Checkbox("Reverse Winding", &m_Working.ReverseWinding);
		if (const auto& stats = m_Snapshot.MeshStatistics) { ImGui::Separator(); ImGui::Text("Vertices: %u  Indices: %u", stats->VertexCount, stats->IndexCount); ImGui::Text("UV: %s  Normals: %s  Tangents: %s", stats->HasUv ? "yes" : "no", stats->HasNormals ? "yes" : "no", stats->HasTangents ? "yes" : "no"); ImGui::Text("AABB min %.3f %.3f %.3f", stats->BoundsMin[0], stats->BoundsMin[1], stats->BoundsMin[2]); ImGui::Text("AABB max %.3f %.3f %.3f", stats->BoundsMax[0], stats->BoundsMax[1], stats->BoundsMax[2]); }
	}

	ResultEnvelope PngTextureImportEditor::Open(const AssetEditorOpenContext& context) { m_Snapshot = context.Snapshot; return OpenSettings<PngTextureImporter>(m_Snapshot, m_Meta, m_Baseline, m_Working); }
	ResultEnvelope PngTextureImportEditor::Validate() const { return PngTextureImporter().ValidateSettings(m_Working); }
	AssetEditCommit PngTextureImportEditor::BuildCommit() const { return BuildSettingsCommit<PngTextureImporter>(m_Snapshot, m_Meta, m_Working); }
	void PngTextureImportEditor::Draw(AssetEditorDrawContext&) {
		ImGui::TextDisabled("Color Space: sRGB (artifact format does not encode linear color space)");
		ImGui::TextDisabled("Mipmaps: disabled (single-level texture artifacts)");
		int alpha = static_cast<int>(m_Working.AlphaMode); if (ImGui::Combo("Alpha Mode", &alpha, "Preserve\0Opaque\0")) m_Working.AlphaMode = static_cast<TextureAlphaMode>(alpha);
		int maxSize = static_cast<int>(m_Working.MaxSize); if (ImGui::InputInt("Max Size", &maxSize)) m_Working.MaxSize = static_cast<uint32_t>((std::max)(1, maxSize));
		ImGui::TextDisabled("Compression: none");
		if (const auto& stats = m_Snapshot.TextureStatistics) { ImGui::Separator(); ImGui::Text("Source: %ux%u, %u channel(s)", stats->SourceWidth, stats->SourceHeight, stats->SourceChannels); ImGui::Text("Artifact: %ux%u RGBA8, %u mip(s)", stats->Width, stats->Height, stats->MipLevels); ImGui::Text("Source alpha: %s", stats->HasAlpha ? "yes" : "no"); }
	}
}
