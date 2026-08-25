#include "enginepch.h"
#include "Assets/AssetInspectorHost.h"

#include "Assets/Editors/GenericAssetInspector.h"
#include "Assets/Editors/MaterialAssetEditor.h"
#include "Assets/Editors/ImportSettingsEditors.h"
#include "Assets/Editors/ShaderAssetEditor.h"
#include "Assets/Editors/SceneAssetEditor.h"

namespace HE::Editor {
	AssetInspectorHost::AssetInspectorHost() {
		m_Registry.SetFallbackFactory([] { return std::make_unique<GenericAssetInspector>(); });
		(void)m_Registry.Register({ AssetKind::Material, "hua.material-yaml" }, [] { return std::make_unique<MaterialAssetEditor>(); });
		(void)m_Registry.Register({ AssetKind::Mesh, "hua.mesh-obj" }, [] { return std::make_unique<ObjMeshImportEditor>(); });
		(void)m_Registry.Register({ AssetKind::Texture2D, "hua.texture-png" }, [] { return std::make_unique<PngTextureImportEditor>(); });
		(void)m_Registry.Register({ AssetKind::Shader, "hua.shader-hlsl" }, [] { return std::make_unique<ShaderAssetEditor>(); });
		(void)m_Registry.Register({ AssetKind::Scene, "scene.native" }, [] { return std::make_unique<SceneAssetEditor>(); });
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
