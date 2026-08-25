#pragma once

#include "Assets/AssetEditor.h"
#include "HuaEngine/Asset/Import/ObjMeshImportSettings.h"
#include "HuaEngine/Asset/Import/PngTextureImportSettings.h"
#include "HuaEngine/Asset/Metadata/AssetMeta.h"

namespace HE::Editor {
	class ObjMeshImportEditor final : public IAssetEditor {
	public:
		ResultEnvelope Open(const AssetEditorOpenContext& context) override;
		void Draw(AssetEditorDrawContext&) override;
		ResultEnvelope Validate() const override;
		AssetEditCommit BuildCommit() const override;
		bool IsDirty() const override { return !(m_Baseline == m_Working); }
		void Revert() override { m_Working = m_Baseline; }
		ObjMeshImportSettings& GetWorkingCopy() { return m_Working; }
	private:
		AssetInspectionSnapshot m_Snapshot;
		AssetMeta m_Meta;
		ObjMeshImportSettings m_Baseline;
		ObjMeshImportSettings m_Working;
	};

	class PngTextureImportEditor final : public IAssetEditor {
	public:
		ResultEnvelope Open(const AssetEditorOpenContext& context) override;
		void Draw(AssetEditorDrawContext&) override;
		ResultEnvelope Validate() const override;
		AssetEditCommit BuildCommit() const override;
		bool IsDirty() const override { return !(m_Baseline == m_Working); }
		void Revert() override { m_Working = m_Baseline; }
		PngTextureImportSettings& GetWorkingCopy() { return m_Working; }
	private:
		AssetInspectionSnapshot m_Snapshot;
		AssetMeta m_Meta;
		PngTextureImportSettings m_Baseline;
		PngTextureImportSettings m_Working;
	};
}
