#pragma once

#include <filesystem>
#include <string>

#include "HuaEngine/Project/ProjectContext.h"

namespace HE {
	struct ProjectSession {
		ProjectContext Context;
		ProjectStatusReport LastStatus;
		std::filesystem::path LastOpenedScenePath;
		bool Loaded = false;

		void Reset() {
			Context = {};
			LastStatus = {};
			LastOpenedScenePath.clear();
			Loaded = false;
		}

		[[nodiscard]] bool IsLoaded() const {
			return Loaded && Context.IsLoaded();
		}

		[[nodiscard]] std::string GetDisplayName() const {
			return Context.Descriptor.Name.empty() ? "UntitledProject" : Context.Descriptor.Name;
		}

		[[nodiscard]] std::string GetTargetId() const {
			return Context.GetTargetId();
		}
	};
}
