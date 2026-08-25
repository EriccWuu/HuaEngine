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
			case AssetImportHealthState::NotApplicable: return "Not applicable";
			}
			return "Unknown";
		}
	}

	ResultEnvelope GenericAssetInspector::Open(const AssetEditorOpenContext& context) {
		m_Snapshot = context.Snapshot;
		return ResultEnvelope::Success("asset.editor.open", m_Snapshot.Asset.Guid, "Generic asset inspector opened");
	}

	void GenericAssetInspector::Draw(AssetEditorDrawContext&) {
		ImGui::Text("Read-only asset (%s)", ToLabel(m_Snapshot.Health.State));
		if (!m_Snapshot.ArtifactRelativePath.empty()) ImGui::TextWrapped("Artifact: %s", m_Snapshot.ArtifactRelativePath.generic_string().c_str());
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
