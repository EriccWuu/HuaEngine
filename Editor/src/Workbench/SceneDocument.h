#pragma once

#include <filesystem>
#include <string>

#include "HuaEngine/Scene/Scene.h"

namespace HE {
	enum class SceneDocumentSource {
		NewScene,
		LoadedFromDisk
	};

	struct SceneDocument {
		Ref<Scene> SceneRef;
		std::filesystem::path ScenePath;
		std::string DisplayName;
		bool Dirty = false;
		SceneDocumentSource Source = SceneDocumentSource::NewScene;

		void Reset() {
			SceneRef.reset();
			ScenePath.clear();
			DisplayName.clear();
			Dirty = false;
			Source = SceneDocumentSource::NewScene;
		}

		[[nodiscard]] bool IsLoaded() const {
			return static_cast<bool>(SceneRef);
		}

		void MarkSaved(const std::filesystem::path& path) {
			ScenePath = path;
			Dirty = false;
			if (DisplayName.empty() && !ScenePath.empty()) {
				DisplayName = ScenePath.stem().string();
			}
		}

		void MarkDirty() {
			Dirty = true;
		}

		void ApplyDirtyState(bool dirty) {
			Dirty = dirty;
		}
	};
}
