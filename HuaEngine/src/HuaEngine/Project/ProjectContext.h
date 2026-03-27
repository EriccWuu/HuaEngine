#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/SerializationCore.h"

namespace HE {
	struct ProjectDescriptor {
		std::string Name = "UntitledProject";
		uint32_t SchemaVersion = 1;
		std::string AssetDirectory = "Assets";
		std::string SceneDirectory = "Scenes";
	};

	struct ProjectContext {
		std::filesystem::path RootPath;
		std::filesystem::path ProjectFilePath;
		ProjectDescriptor Descriptor;

		[[nodiscard]] bool IsLoaded() const;
		[[nodiscard]] std::filesystem::path GetMetadataDirectoryPath() const;
		[[nodiscard]] std::filesystem::path GetAssetRootPath() const;
		[[nodiscard]] std::filesystem::path GetSceneRootPath() const;
		[[nodiscard]] std::string GetTargetId() const;
	};

	struct ProjectStatusReport {
		bool RootExists = false;
		bool MetadataDirectoryExists = false;
		bool ProjectFileExists = false;
		bool AssetDirectoryExists = false;
		bool SceneDirectoryExists = false;

		[[nodiscard]] bool IsOperational() const;
		[[nodiscard]] bool HasIssues() const;
	};
}

srefl_class(HE::ProjectDescriptor,
	fields(
		field(Name),
		field(SchemaVersion),
		field(AssetDirectory),
		field(SceneDirectory)
	)
)

namespace HE::Serialization {
	template<>
	struct Serializer<HE::ProjectDescriptor> {
		static void Serialize(SerializationBackend& backend, const std::string& name, const HE::ProjectDescriptor& descriptor) {
			if (!name.empty()) {
				backend.BeginObject(name);
			}

			backend.Serialize("name", descriptor.Name);
			backend.Serialize("schema_version", descriptor.SchemaVersion);
			backend.Serialize("asset_directory", descriptor.AssetDirectory);
			backend.Serialize("scene_directory", descriptor.SceneDirectory);

			if (!name.empty()) {
				backend.EndObject();
			}
		}

		static bool Deserialize(SerializationBackend& backend, const std::string& name, HE::ProjectDescriptor& descriptor) {
			if (!name.empty() && !backend.HasField(name)) {
				return false;
			}

			if (!name.empty()) {
				backend.BeginObject(name);
			}

			bool success = true;
			success &= DeserializeValue(backend, "name", descriptor.Name);
			success &= DeserializeValue(backend, "schema_version", descriptor.SchemaVersion);
			success &= DeserializeValue(backend, "asset_directory", descriptor.AssetDirectory);
			success &= DeserializeValue(backend, "scene_directory", descriptor.SceneDirectory);

			if (!name.empty()) {
				backend.EndObject();
			}

			return success;
		}
	};
}
