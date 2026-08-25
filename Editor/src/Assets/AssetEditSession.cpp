#include "enginepch.h"
#include "Assets/AssetEditSession.h"

namespace HE::Editor {
	void AssetEditSession::Open(AssetInspectionSnapshot snapshot) {
		m_Snapshot = std::move(snapshot);
		m_Open = true;
		m_Dirty = false;
	}

	void AssetEditSession::Close() {
		m_Snapshot = {};
		m_Open = false;
		m_Dirty = false;
	}
}
