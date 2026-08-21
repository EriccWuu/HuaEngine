#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/SerializationCore.h"

namespace HE {
	struct PersistedSceneCameraPose {
		std::string ScenePath;
		float PositionX = 0.0f;
		float PositionY = 0.0f;
		float PositionZ = 0.0f;
		float Pitch = 0.0f;
		float Yaw = 0.0f;
	};

	struct PersistedEditorSession {
		std::string LastProjectRoot;
		std::string LastProjectName;
		std::string LastScenePath;
		std::vector<PersistedSceneCameraPose> SceneCameraPoses;

		[[nodiscard]] const PersistedSceneCameraPose* FindSceneCameraPose(std::string_view scenePath) const {
			for (const auto& pose : SceneCameraPoses) {
				if (pose.ScenePath == scenePath) return &pose;
			}
			return nullptr;
		}

		void UpsertSceneCameraPose(PersistedSceneCameraPose pose) {
			for (auto& existing : SceneCameraPoses) {
				if (existing.ScenePath == pose.ScenePath) {
					existing = std::move(pose);
					return;
				}
			}
			SceneCameraPoses.push_back(std::move(pose));
		}

		[[nodiscard]] bool HasProject() const {
			return !LastProjectRoot.empty();
		}

		void Reset() {
			LastProjectRoot.clear();
			LastProjectName.clear();
			LastScenePath.clear();
			SceneCameraPoses.clear();
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

srefl_class(HE::PersistedSceneCameraPose,
	fields(
		field(ScenePath), field(PositionX), field(PositionY), field(PositionZ), field(Pitch), field(Yaw)
	)
)

srefl_class(HE::PersistedEditorSession,
	fields(
		field(LastProjectRoot),
		field(LastProjectName),
		field(LastScenePath),
		field(SceneCameraPoses)
	)
)

namespace HE::Serialization {
	template<>
	struct Serializer<HE::PersistedSceneCameraPose> {
		static void Serialize(SerializationBackend& backend, const std::string& name, const HE::PersistedSceneCameraPose& pose) {
			backend.BeginObject(name);
			backend.Serialize("scene_path", pose.ScenePath);
			backend.Serialize("position_x", pose.PositionX);
			backend.Serialize("position_y", pose.PositionY);
			backend.Serialize("position_z", pose.PositionZ);
			backend.Serialize("pitch", pose.Pitch);
			backend.Serialize("yaw", pose.Yaw);
			backend.EndObject();
		}

		static bool Deserialize(SerializationBackend& backend, const std::string& name, HE::PersistedSceneCameraPose& pose) {
			backend.BeginObject(name);
			bool success = true;
			success &= DeserializeValue(backend, "scene_path", pose.ScenePath);
			success &= DeserializeValue(backend, "position_x", pose.PositionX);
			success &= DeserializeValue(backend, "position_y", pose.PositionY);
			success &= DeserializeValue(backend, "position_z", pose.PositionZ);
			success &= DeserializeValue(backend, "pitch", pose.Pitch);
			success &= DeserializeValue(backend, "yaw", pose.Yaw);
			backend.EndObject();
			return success;
		}
	};

	template<>
	struct Serializer<HE::PersistedEditorSession> {
		static void Serialize(SerializationBackend& backend, const std::string& name, const HE::PersistedEditorSession& session) {
			if (!name.empty()) {
				backend.BeginObject(name);
			}

			backend.Serialize("last_project_root", session.LastProjectRoot);
			backend.Serialize("last_project_name", session.LastProjectName);
			backend.Serialize("last_scene_path", session.LastScenePath);
			SerializeValue(backend, "scene_camera_poses", session.SceneCameraPoses);

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
			if (backend.HasField("scene_camera_poses")) {
				success &= DeserializeValue(backend, "scene_camera_poses", session.SceneCameraPoses);
			} else if (backend.HasField("has_scene_camera_pose")) {
				bool hasLegacyPose = false;
				success &= DeserializeValue(backend, "has_scene_camera_pose", hasLegacyPose);
				if (hasLegacyPose && !session.LastScenePath.empty()) {
					HE::PersistedSceneCameraPose legacyPose;
					legacyPose.ScenePath = session.LastScenePath;
					success &= DeserializeValue(backend, "scene_camera_position_x", legacyPose.PositionX);
					success &= DeserializeValue(backend, "scene_camera_position_y", legacyPose.PositionY);
					success &= DeserializeValue(backend, "scene_camera_position_z", legacyPose.PositionZ);
					success &= DeserializeValue(backend, "scene_camera_pitch", legacyPose.Pitch);
					success &= DeserializeValue(backend, "scene_camera_yaw", legacyPose.Yaw);
				session.UpsertSceneCameraPose(std::move(legacyPose));
				}
			}

			if (!name.empty()) {
				backend.EndObject();
			}

			return success;
		}
	};
}
