#pragma once

#include "Assets/AssetEditor.h"

namespace HE::Editor {
	class GenericAssetInspector final : public IAssetEditor {
	public:
		ResultEnvelope Open(const AssetEditorOpenContext& context) override;
		void Draw(AssetEditorDrawContext& context) override;
		[[nodiscard]] ResultEnvelope Validate() const override;
		[[nodiscard]] AssetEditCommit BuildCommit() const override;
		[[nodiscard]] bool IsDirty() const override { return false; }
		void Revert() override {}

		[[nodiscard]] const AssetInspectionSnapshot& GetSnapshot() const { return m_Snapshot; }

	private:
		AssetInspectionSnapshot m_Snapshot;
	};
}
