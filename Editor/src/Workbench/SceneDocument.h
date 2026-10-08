#pragma once

#include <filesystem>
#include <string>

#include "HuaEngine/Scene/Scene.h"

namespace HE {
	struct SceneDocument {
		Ref<Scene> SceneRef;
		std::filesystem::path ScenePath;
		std::string DisplayName;
		bool Dirty = false;

		void Reset() {
			SceneRef.reset();
			ScenePath.clear();
			DisplayName.clear();
			Dirty = false;
		}

		[[nodiscard]] bool IsLoaded() const {
			return static_cast<bool>(SceneRef) && !ScenePath.empty();
		}

		void MarkSaved(const std::filesystem::path& path) {
			ScenePath = path;
			Dirty = false;
			if (DisplayName.empty() && !ScenePath.empty()) {
				DisplayName = ScenePath.stem().string();
			}
		}

		void RenamePath(const std::filesystem::path& path) {
			ScenePath = path;
			DisplayName = ScenePath.stem().string();
		}

		void MarkDirty() {
			Dirty = true;
		}

		void ApplyDirtyState(bool dirty) {
			Dirty = dirty;
		}
	};
}
