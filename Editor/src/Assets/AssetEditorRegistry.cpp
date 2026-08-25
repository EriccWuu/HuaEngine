#include "enginepch.h"
#include "Assets/AssetEditorRegistry.h"

namespace HE::Editor {
	size_t AssetEditorKeyHash::operator()(const AssetEditorKey& key) const {
		const size_t kindHash = std::hash<uint32_t>{}(static_cast<uint32_t>(key.Kind));
		return kindHash ^ (std::hash<std::string>{}(key.ImporterId) << 1);
	}

	ResultEnvelope AssetEditorRegistry::Register(AssetEditorKey key, AssetEditorFactory factory) {
		if (key.Kind == AssetKind::Unknown || key.ImporterId.empty() || !factory) {
			return ResultEnvelope::Failure("asset.editor.register", key.ImporterId, "Asset editor registration is incomplete");
		}
		if (m_Factories.contains(key)) {
			auto result = ResultEnvelope::Failure("asset.editor.register", key.ImporterId, "Asset editor is already registered");
			result.AddDetail({ DiagnosticSeverity::Error, "asset.edit.editor_duplicate", "Asset editor key is already registered", key.ImporterId });
			return result;
		}
		m_Factories.emplace(std::move(key), std::move(factory));
		return ResultEnvelope::Success("asset.editor.register", {}, "Asset editor registered");
	}

	std::unique_ptr<IAssetEditor> AssetEditorRegistry::Create(AssetKind kind, std::string_view importerId) const {
		const auto found = m_Factories.find({ kind, std::string(importerId) });
		if (found != m_Factories.end()) return found->second();
		return m_FallbackFactory ? m_FallbackFactory() : nullptr;
	}
}
