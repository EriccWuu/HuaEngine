#pragma once

#include "Assets/AssetEditor.h"
#include "HuaEngine/Rendering/Material/MaterialSourceData.h"

namespace HE::Editor {
	class MaterialAssetEditor final : public IAssetEditor {
	public:
		ResultEnvelope Open(const AssetEditorOpenContext& context) override;
		void Draw(AssetEditorDrawContext& context) override;
		[[nodiscard]] ResultEnvelope Validate() const override;
		[[nodiscard]] AssetEditCommit BuildCommit() const override;
		[[nodiscard]] bool IsDirty() const override;
		void Revert() override;

		[[nodiscard]] Rendering::MaterialSourceData& GetWorkingCopy() { return m_WorkingCopy; }
		[[nodiscard]] const Rendering::MaterialSourceData& GetWorkingCopy() const { return m_WorkingCopy; }

	private:
		AssetInspectionSnapshot m_Snapshot;
		Rendering::MaterialSourceData m_Baseline;
		Rendering::MaterialSourceData m_WorkingCopy;
	};
}
