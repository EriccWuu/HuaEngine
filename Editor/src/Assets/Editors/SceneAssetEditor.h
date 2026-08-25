#pragma once

#include <filesystem>
#include <functional>
#include <utility>

#include "Assets/AssetEditor.h"

namespace HE::Editor {
	struct SceneAssetDocumentState {
		std::filesystem::path ActiveScenePath;
		bool Dirty = false;
	};

	struct SceneAssetEditorServices {
		std::function<void(const std::filesystem::path&)> OpenScene;
		std::function<SceneAssetDocumentState()> GetActiveDocument;
	};

	class SceneAssetEditor final : public IAssetEditor {
	public:
		explicit SceneAssetEditor(SceneAssetEditorServices services = {})
			: m_Services(std::move(services)) {}

		ResultEnvelope Open(const AssetEditorOpenContext& context) override;
		void Draw(AssetEditorDrawContext& context) override;
		ResultEnvelope Validate() const override;
		AssetEditCommit BuildCommit() const override { return { .Guid = m_Snapshot.Asset.Guid }; }
		bool IsDirty() const override { return false; }
		void Revert() override {}
	private:
		SceneAssetEditorServices m_Services;
		AssetInspectionSnapshot m_Snapshot;
		uintmax_t m_SourceBytes = 0;
	};
}
