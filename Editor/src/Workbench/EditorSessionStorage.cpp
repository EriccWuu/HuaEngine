#include "enginepch.h"
#include "EditorSessionStorage.h"

#include <cstdlib>
#include <system_error>

#include "HuaEngine/Serialization/Serialization.h"

namespace {
	std::filesystem::path ResolveEditorStateRoot() {
		const char* localAppData = std::getenv("LOCALAPPDATA");
		if (localAppData && *localAppData) {
			return std::filesystem::path(localAppData) / "HuaEngine" / "Editor";
		}

		return std::filesystem::temp_directory_path() / "huaengine_editor_state";
	}
}

namespace HE {
	std::filesystem::path EditorSessionStorage::GetSessionFilePath() {
		return ResolveEditorStateRoot() / "session.json";
	}

	bool EditorSessionStorage::Load(PersistedEditorSession& session) {
		const auto sessionFilePath = GetSessionFilePath();
		std::error_code errorCode;
		if (!std::filesystem::exists(sessionFilePath, errorCode) ||
			!std::filesystem::is_regular_file(sessionFilePath, errorCode)) {
			session.Reset();
			return false;
		}

		PersistedEditorSession loaded;
		if (!Serialization::LoadFromJson(sessionFilePath.string(), loaded)) {
			session.Reset();
			return false;
		}

		session = std::move(loaded);
		return session.HasProject();
	}

	bool EditorSessionStorage::Save(const PersistedEditorSession& session) {
		const auto sessionFilePath = GetSessionFilePath();
		std::error_code errorCode;
		std::filesystem::create_directories(sessionFilePath.parent_path(), errorCode);
		if (errorCode) {
			return false;
		}

		return Serialization::SaveAsJson(session, sessionFilePath.string());
	}

	void EditorSessionStorage::Clear() {
		const auto sessionFilePath = GetSessionFilePath();
		std::error_code errorCode;
		std::filesystem::remove(sessionFilePath, errorCode);
	}
}
