#pragma once

#include <span>
#include <functional>

#include "Assets/AssetEditSession.h"
#include "Assets/AssetPickerModel.h"
#include "HuaEngine/Rendering/Shader/ShaderInterface.h"
#include "HuaEngine/Asset/Authoring/AssetEditCommit.h"

namespace HE::Editor {
	struct AssetEditorOpenContext {
		const AssetInspectionSnapshot& Snapshot;
	};

	struct AssetEditorDrawContext {
		std::span<const AssetPickerOption> ShaderAssets;
		std::span<const AssetPickerOption> TextureAssets;
		std::function<ResultEnvelope(const AssetGuid&, Rendering::ShaderAuthoringMetadata&)> GetShaderAuthoringMetadata;
		std::function<ResultEnvelope(const std::filesystem::path&)> ReimportAsset;
		std::function<void(const std::filesystem::path&)> OpenScene;
		std::filesystem::path ActiveScenePath;
		bool ActiveSceneDirty = false;
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
