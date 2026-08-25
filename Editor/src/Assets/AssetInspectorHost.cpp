#include "enginepch.h"
#include "Assets/AssetInspectorHost.h"

#include "Assets/Editors/GenericAssetInspector.h"

namespace HE::Editor {
	AssetInspectorHost::AssetInspectorHost() {
		m_Registry.SetFallbackFactory([] { return std::make_unique<GenericAssetInspector>(); });
	}

	ResultEnvelope AssetInspectorHost::Open(const AssetGuid& guid, const InspectAssetCallback& inspectAsset) {
		if (guid.empty() || !inspectAsset) {
			return ResultEnvelope::Failure("asset.editor.open", guid, "Asset inspection request is incomplete");
		}

		AssetInspectionSnapshot snapshot;
		auto inspectResult = inspectAsset(guid, snapshot);
		if (!inspectResult.Succeeded()) return inspectResult;

		auto editor = m_Registry.Create(snapshot.Asset.Kind, snapshot.ImporterId);
		if (!editor) {
			auto result = ResultEnvelope::Failure("asset.editor.open", guid, "No asset editor is available");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.edit.editor_unavailable", "No specialized or fallback asset editor is registered", snapshot.ImporterId });
			return result;
		}

		auto openResult = editor->Open({ snapshot });
		if (!openResult.Succeeded()) return openResult;
		m_Session.Open(std::move(snapshot));
		m_Editor = std::move(editor);
		return openResult;
	}

	void AssetInspectorHost::Close() {
		m_Editor.reset();
		m_Session.Close();
	}
}
