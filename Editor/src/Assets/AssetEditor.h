#pragma once

#include "Assets/AssetEditSession.h"

namespace HE::Editor {
	struct AssetEditorOpenContext {
		const AssetInspectionSnapshot& Snapshot;
	};

	struct AssetEditorDrawContext {};

	struct AssetEditCommit {
		AssetGuid Guid;
	};

	class IAssetEditor {
	public:
		virtual ~IAssetEditor() = default;

		virtual ResultEnvelope Open(const AssetEditorOpenContext& context) = 0;
		virtual void Draw(AssetEditorDrawContext& context) = 0;
		[[nodiscard]] virtual ResultEnvelope Validate() const = 0;
		[[nodiscard]] virtual AssetEditCommit BuildCommit() const = 0;
		[[nodiscard]] virtual bool IsDirty() const = 0;
		virtual void Revert() = 0;
	};
}
