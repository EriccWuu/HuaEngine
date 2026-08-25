#pragma once

#include "HuaEngine/Asset/AssetInspection.h"

namespace HE::Editor {
	class AssetEditSession {
	public:
		void Open(AssetInspectionSnapshot snapshot);
		void Close();

		[[nodiscard]] bool IsOpen() const { return m_Open; }
		[[nodiscard]] bool IsDirty() const { return m_Dirty; }
		[[nodiscard]] const AssetInspectionSnapshot* GetSnapshot() const { return m_Open ? &m_Snapshot : nullptr; }
		[[nodiscard]] const AssetGuid& GetGuid() const { return m_Snapshot.Asset.Guid; }

		void MarkDirty() { if (m_Open) m_Dirty = true; }
		void MarkClean() { m_Dirty = false; }

	private:
		AssetInspectionSnapshot m_Snapshot;
		bool m_Open = false;
		bool m_Dirty = false;
	};
}
