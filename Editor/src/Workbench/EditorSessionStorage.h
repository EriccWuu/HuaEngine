#pragma once

#include <filesystem>
#include <string>

#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/SerializationCore.h"

namespace HE {
	struct PersistedEditorSession {
		std::string LastProjectRoot;
		std::string LastProjectName;
		std::string LastScenePath;

		[[nodiscard]] bool HasProject() const {
			return !LastProjectRoot.empty();
		}

		void Reset() {
			LastProjectRoot.clear();
			LastProjectName.clear();
			LastScenePath.clear();
		}
	};

	class EditorSessionStorage {
	public:
		[[nodiscard]] static std::filesystem::path GetSessionFilePath();
		[[nodiscard]] static bool Load(PersistedEditorSession& session);
		[[nodiscard]] static bool Save(const PersistedEditorSession& session);
		static void Clear();
	};
}

srefl_class(HE::PersistedEditorSession,
	fields(
		field(LastProjectRoot),
		field(LastProjectName),
		field(LastScenePath)
	)
)

namespace HE::Serialization {
	template<>
	struct Serializer<HE::PersistedEditorSession> {
		static void Serialize(SerializationBackend& backend, const std::string& name, const HE::PersistedEditorSession& session) {
			if (!name.empty()) {
				backend.BeginObject(name);
			}

			backend.Serialize("last_project_root", session.LastProjectRoot);
			backend.Serialize("last_project_name", session.LastProjectName);
			backend.Serialize("last_scene_path", session.LastScenePath);

			if (!name.empty()) {
				backend.EndObject();
			}
		}

		static bool Deserialize(SerializationBackend& backend, const std::string& name, HE::PersistedEditorSession& session) {
			if (!name.empty() && !backend.HasField(name)) {
				return false;
			}

			if (!name.empty()) {
				backend.BeginObject(name);
			}

			bool success = true;
			success &= DeserializeValue(backend, "last_project_root", session.LastProjectRoot);
			success &= DeserializeValue(backend, "last_project_name", session.LastProjectName);
			success &= DeserializeValue(backend, "last_scene_path", session.LastScenePath);

			if (!name.empty()) {
				backend.EndObject();
			}

			return success;
		}
	};
}
