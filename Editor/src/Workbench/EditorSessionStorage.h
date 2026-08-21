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
		float SceneCameraPositionX = 0.0f;
		float SceneCameraPositionY = 0.0f;
		float SceneCameraPositionZ = 0.0f;
		float SceneCameraPitch = 0.0f;
		float SceneCameraYaw = 0.0f;
		bool HasSceneCameraPose = false;

		[[nodiscard]] bool HasProject() const {
			return !LastProjectRoot.empty();
		}

		void Reset() {
			LastProjectRoot.clear();
			LastProjectName.clear();
			LastScenePath.clear();
			SceneCameraPositionX = 0.0f;
			SceneCameraPositionY = 0.0f;
			SceneCameraPositionZ = 0.0f;
			SceneCameraPitch = 0.0f;
			SceneCameraYaw = 0.0f;
			HasSceneCameraPose = false;
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
		field(LastScenePath),
		field(SceneCameraPositionX),
		field(SceneCameraPositionY),
		field(SceneCameraPositionZ),
		field(SceneCameraPitch),
		field(SceneCameraYaw),
		field(HasSceneCameraPose)
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
			backend.Serialize("scene_camera_position_x", session.SceneCameraPositionX);
			backend.Serialize("scene_camera_position_y", session.SceneCameraPositionY);
			backend.Serialize("scene_camera_position_z", session.SceneCameraPositionZ);
			backend.Serialize("scene_camera_pitch", session.SceneCameraPitch);
			backend.Serialize("scene_camera_yaw", session.SceneCameraYaw);
			backend.Serialize("has_scene_camera_pose", session.HasSceneCameraPose);

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
			if (backend.HasField("has_scene_camera_pose")) {
				success &= DeserializeValue(backend, "scene_camera_position_x", session.SceneCameraPositionX);
				success &= DeserializeValue(backend, "scene_camera_position_y", session.SceneCameraPositionY);
				success &= DeserializeValue(backend, "scene_camera_position_z", session.SceneCameraPositionZ);
				success &= DeserializeValue(backend, "scene_camera_pitch", session.SceneCameraPitch);
				success &= DeserializeValue(backend, "scene_camera_yaw", session.SceneCameraYaw);
				success &= DeserializeValue(backend, "has_scene_camera_pose", session.HasSceneCameraPose);
			}

			if (!name.empty()) {
				backend.EndObject();
			}

			return success;
		}
	};
}
