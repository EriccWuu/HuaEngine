#include "enginepch.h"
#include "Assets/Editors/GenericAssetInspector.h"

#include "imgui.h"

namespace HE::Editor {
	namespace {
		const char* ToLabel(AssetImportHealthState state) {
			switch (state) {
			case AssetImportHealthState::Current: return "Current";
			case AssetImportHealthState::LastGoodWithFailure: return "Last good with failure";
			case AssetImportHealthState::Missing: return "Missing";
			case AssetImportHealthState::Stale: return "Stale";
			}
			return "Unknown";
		}
	}

	ResultEnvelope GenericAssetInspector::Open(const AssetEditorOpenContext& context) {
		m_Snapshot = context.Snapshot;
		return ResultEnvelope::Success("asset.editor.open", m_Snapshot.Asset.Guid, "Generic asset inspector opened");
	}

	void GenericAssetInspector::Draw(AssetEditorDrawContext&) {
		ImGui::TextUnformatted(m_Snapshot.Asset.AssetId.c_str());
		ImGui::Separator();
		ImGui::Text("Kind: %s", ToString(m_Snapshot.Asset.Kind).data());
		ImGui::TextWrapped("GUID: %s", m_Snapshot.Asset.Guid.c_str());
		ImGui::TextWrapped("Path: %s", m_Snapshot.Asset.RelativePath.generic_string().c_str());
		ImGui::Text("Importer: %s", m_Snapshot.ImporterId.empty() ? "Unavailable" : m_Snapshot.ImporterId.c_str());
		ImGui::Text("Import Health: %s", ToLabel(m_Snapshot.Health.State));
		ImGui::Text("Dependencies: %zu", m_Snapshot.Dependencies.size());
		ImGui::Text("Dependents: %zu", m_Snapshot.Dependents.size());
		if (!m_Snapshot.ArtifactRelativePath.empty()) ImGui::TextWrapped("Artifact: %s", m_Snapshot.ArtifactRelativePath.generic_string().c_str());
		for (const auto& diagnostic : m_Snapshot.Diagnostics) ImGui::TextWrapped("%s", diagnostic.Message.c_str());
	}

	ResultEnvelope GenericAssetInspector::Validate() const {
		return m_Snapshot.Asset.IsOperational()
			? ResultEnvelope::Success("asset.edit.validate", m_Snapshot.Asset.Guid, "Asset is valid for read-only inspection")
			: ResultEnvelope::Failure("asset.edit.validate", m_Snapshot.Asset.Guid, "Asset metadata is invalid");
	}

	AssetEditCommit GenericAssetInspector::BuildCommit() const {
		return { .Guid = m_Snapshot.Asset.Guid };
	}
}
