#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Assets/AssetEditor.h"

namespace HE::Editor {
	struct AssetEditorKey {
		AssetKind Kind = AssetKind::Unknown;
		std::string ImporterId;

		bool operator==(const AssetEditorKey&) const = default;
	};

	struct AssetEditorKeyHash {
		size_t operator()(const AssetEditorKey& key) const;
	};

	using AssetEditorFactory = std::function<std::unique_ptr<IAssetEditor>()>;

	class AssetEditorRegistry {
	public:
		[[nodiscard]] ResultEnvelope Register(AssetEditorKey key, AssetEditorFactory factory);
		[[nodiscard]] std::unique_ptr<IAssetEditor> Create(AssetKind kind, std::string_view importerId) const;
		void SetFallbackFactory(AssetEditorFactory factory) { m_FallbackFactory = std::move(factory); }

	private:
		std::unordered_map<AssetEditorKey, AssetEditorFactory, AssetEditorKeyHash> m_Factories;
		AssetEditorFactory m_FallbackFactory;
	};
}
