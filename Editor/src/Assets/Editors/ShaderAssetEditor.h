#pragma once

#include "Assets/AssetEditor.h"

namespace HE::Editor {
	class ShaderAssetEditor final : public IAssetEditor {
	public:
		ResultEnvelope Open(const AssetEditorOpenContext& context) override;
		void Draw(AssetEditorDrawContext& context) override;
		ResultEnvelope Validate() const override;
		AssetEditCommit BuildCommit() const override { return { .Guid = m_Snapshot.Asset.Guid }; }
		bool IsDirty() const override { return false; }
		void Revert() override {}
	private:
		AssetInspectionSnapshot m_Snapshot;
	};
}
