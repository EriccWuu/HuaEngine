#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Rendering/RHI/ShaderProgram.h"
#include "HuaEngine/Rendering/RHI/TextureResource.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/Mesh.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Reflection/ReflectionMarkers.h"
#include "HuaEngine/Serialization/SerializationCore.h"

#include <cctype>
#include <unordered_map>

namespace HE::Rendering {
	enum class [[sattr(guid="4f745e86ab69460db41dac991f79c005"; reflect=@full; attrs=["DisplayName=Material Blend Mode"])]] MaterialBlendMode {
		Opaque,
		Masked,
		Transparent
	};

	struct [[sattr(guid="4f745e86ab69460db41dac991f79c002"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Camera","Category=Rendering"])]] CameraComponent {
		[[sattr()]]
		bool Primary = true;
		[[sattr()]]
		bool FixedAspectRatio = false;
		[[sattr(attrs=["Editor.Min=1","Editor.Max=179","Editor.Unit=degrees"])]]
		float VerticalFovDegrees = 45.0f;
		[[sattr()]]
		float NearClip = 0.1f;
		[[sattr()]]
		float FarClip = 100.0f;
		[[sattr()]]
		float AspectRatio = 16.0f / 9.0f;
	};

	struct MaterialOverrideSet {
		std::unordered_map<std::string, HE::Rendering::MaterialParameterValue> Parameters;
		std::unordered_map<std::string, AssetGuid> TextureParameters;

		void SetFloat(const std::string& name, float value) {
			Parameters[name] = value;
		}

		void SetVec3(const std::string& name, const glm::vec3& value) {
			Parameters[name] = value;
		}

		void SetVec4(const std::string& name, const glm::vec4& value) {
			Parameters[name] = value;
		}

		[[nodiscard]] bool Empty() const {
			return Parameters.empty() && TextureParameters.empty();
		}
	};

	// Material component
	struct [[sattr(guid="4f745e86ab69460db41dac991f79c003"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Material","Category=Rendering"])]] MaterialComponent {
		MaterialComponent() = default;
		explicit MaterialComponent(const MaterialAssetRef& material)
			: Material(material) {}

		[[sattr(attrs=["Inspector.Drawer=MaterialReference","Editor.AssetKind=material","Editor.ReconcileField=Overrides"])]]
		MaterialAssetRef Material;
		[[sattr(attrs=["Inspector.Drawer=MaterialOverrides","Editor.SourceField=Material"])]]
		MaterialOverrideSet Overrides;
		[[sattr()]]
		MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
	};

	// Legacy RendererComponent for backward compatibility (deprecated)
	struct RendererComponent {
		RendererComponent() = default;
		RendererComponent(const Ref<HE::Rendering::ShaderProgram>& shaderProgram, const Ref<HE::Rendering::TextureResource>& texture)
			: ShaderProgram(shaderProgram), Texture(texture) {}

		Ref<HE::Rendering::ShaderProgram> ShaderProgram;
		Ref<HE::Rendering::TextureResource> Texture;
	};

	struct [[sattr(guid="4f745e86ab69460db41dac991f79c004"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Mesh","Category=Rendering"])]] MeshComponent {
		MeshComponent() = default;
		explicit MeshComponent(const MeshAssetRef& mesh)
			: Mesh(mesh) {}

		[[sattr(attrs=["Editor.AssetKind=mesh"])]]
		MeshAssetRef Mesh;
	};
}

namespace HE::Serialization {
	namespace MaterialOverrideSerialization {
		inline std::string NormalizeTypeName(std::string type) {
			for (char& character : type) {
				character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			}
			return type;
		}

		inline void SerializeFloatArray(SerializationBackend& backend, const std::string& name, const float* values, size_t count) {
			backend.BeginArray(name, count);
			for (size_t index = 0; index < count; ++index) {
				backend.BeginArrayElement(index);
				backend.Serialize("", values[index]);
				backend.EndArrayElement();
			}
			backend.EndArray();
		}

		inline bool DeserializeFloatArray(SerializationBackend& backend, const std::string& name, float* values, size_t count) {
			if (!backend.HasField(name) || backend.GetFieldType(name) != SerializationType::Array ||
				backend.GetArraySize(name) != count) {
				return false;
			}

			backend.BeginArray(name);
			bool success = true;
			for (size_t index = 0; index < count; ++index) {
				backend.BeginArrayElement(index);
				success &= backend.Deserialize("", values[index]);
				backend.EndArrayElement();
			}
			backend.EndArray();
			return success;
		}

		inline bool DeserializeVectorObject(SerializationBackend& backend, const std::string& name, glm::vec2& value) {
			if (!backend.HasField(name) || backend.GetFieldType(name) != SerializationType::Object) {
				return false;
			}
			backend.BeginObject(name);
			const bool success = backend.Deserialize("x", value.x) && backend.Deserialize("y", value.y);
			backend.EndObject();
			return success;
		}

		inline bool DeserializeVectorObject(SerializationBackend& backend, const std::string& name, glm::vec3& value) {
			if (!backend.HasField(name) || backend.GetFieldType(name) != SerializationType::Object) {
				return false;
			}
			backend.BeginObject(name);
			const bool success = backend.Deserialize("x", value.x) &&
				backend.Deserialize("y", value.y) &&
				backend.Deserialize("z", value.z);
			backend.EndObject();
			return success;
		}

		inline bool DeserializeVectorObject(SerializationBackend& backend, const std::string& name, glm::vec4& value) {
			if (!backend.HasField(name) || backend.GetFieldType(name) != SerializationType::Object) {
				return false;
			}
			backend.BeginObject(name);
			const bool success = backend.Deserialize("x", value.x) &&
				backend.Deserialize("y", value.y) &&
				backend.Deserialize("z", value.z) &&
				backend.Deserialize("w", value.w);
			backend.EndObject();
			return success;
		}

		inline bool SerializeMaterialParameterValue(
			SerializationBackend& backend,
			const HE::Rendering::MaterialParameterValue& value) {
			if (const auto* intValue = std::get_if<int>(&value)) {
				backend.Serialize("type", "int");
				backend.Serialize("value", static_cast<int32_t>(*intValue));
				return true;
			}
			if (const auto* floatValue = std::get_if<float>(&value)) {
				backend.Serialize("type", "float");
				backend.Serialize("value", *floatValue);
				return true;
			}
			if (const auto* vec2Value = std::get_if<glm::vec2>(&value)) {
				backend.Serialize("type", "vec2");
				SerializeFloatArray(backend, "value", &vec2Value->x, 2);
				return true;
			}
			if (const auto* vec3Value = std::get_if<glm::vec3>(&value)) {
				backend.Serialize("type", "vec3");
				SerializeFloatArray(backend, "value", &vec3Value->x, 3);
				return true;
			}
			if (const auto* vec4Value = std::get_if<glm::vec4>(&value)) {
				backend.Serialize("type", "vec4");
				SerializeFloatArray(backend, "value", &vec4Value->x, 4);
				return true;
			}
			if (const auto* mat3Value = std::get_if<glm::mat3>(&value)) {
				backend.Serialize("type", "mat3");
				SerializeFloatArray(backend, "value", &(*mat3Value)[0][0], 9);
				return true;
			}
			if (const auto* mat4Value = std::get_if<glm::mat4>(&value)) {
				backend.Serialize("type", "mat4");
				SerializeFloatArray(backend, "value", &(*mat4Value)[0][0], 16);
				return true;
			}

			backend.Serialize("type", "unsupported");
			backend.Serialize("diagnostic", "Unsupported material override parameter type");
			return false;
		}

		inline bool DeserializeMaterialParameterValue(
			SerializationBackend& backend,
			HE::Rendering::MaterialParameterValue& outValue) {
			std::string type;
			if (!backend.Deserialize("type", type)) {
				(void)backend.Deserialize("value_type", type);
			}
			type = NormalizeTypeName(type);

			if (type == "int") {
				int32_t value = 0;
				if (!backend.Deserialize("value", value)) {
					return false;
				}
				outValue = static_cast<int>(value);
				return true;
			}
			if (type == "float") {
				float value = 0.0f;
				if (!backend.Deserialize("value", value)) {
					return false;
				}
				outValue = value;
				return true;
			}
			if (type == "vec2") {
				glm::vec2 value(0.0f);
				if (!DeserializeFloatArray(backend, "value", &value.x, 2) &&
					!DeserializeVectorObject(backend, "value", value)) {
					return false;
				}
				outValue = value;
				return true;
			}
			if (type == "vec3") {
				glm::vec3 value(0.0f);
				if (!DeserializeFloatArray(backend, "value", &value.x, 3) &&
					!DeserializeVectorObject(backend, "value", value)) {
					return false;
				}
				outValue = value;
				return true;
			}
			if (type == "vec4") {
				glm::vec4 value(0.0f);
				if (!DeserializeFloatArray(backend, "value", &value.x, 4) &&
					!DeserializeVectorObject(backend, "value", value)) {
					return false;
				}
				outValue = value;
				return true;
			}
			if (type == "mat3") {
				glm::mat3 value(1.0f);
				if (!DeserializeFloatArray(backend, "value", &value[0][0], 9)) {
					return false;
				}
				outValue = value;
				return true;
			}
			if (type == "mat4") {
				glm::mat4 value(1.0f);
				if (!DeserializeFloatArray(backend, "value", &value[0][0], 16)) {
					return false;
				}
				outValue = value;
				return true;
			}

			return false;
		}
	}

	template<>
	struct Serializer<HE::Rendering::MaterialOverrideSet> {
		static void Serialize(
			SerializationBackend& backend,
			const std::string& name,
			const HE::Rendering::MaterialOverrideSet& overrides) {
			backend.BeginObject(name);
			backend.BeginObject("parameters");
			bool allParametersSupported = true;
			for (const auto& [parameterName, value] : overrides.Parameters) {
				backend.BeginObject(parameterName);
				allParametersSupported &= MaterialOverrideSerialization::SerializeMaterialParameterValue(backend, value);
				backend.EndObject();
			}
			backend.EndObject();
			backend.BeginObject("textures");
			for (const auto& [parameterName, guid] : overrides.TextureParameters) { backend.BeginObject(parameterName); backend.Serialize("guid", guid); backend.EndObject(); }
			backend.EndObject();
			if (!allParametersSupported) {
				backend.Serialize("diagnostic", "One or more material override parameters are unsupported");
			}
			backend.EndObject();
		}

		static bool Deserialize(
			SerializationBackend& backend,
			const std::string& name,
			HE::Rendering::MaterialOverrideSet& overrides) {
			if (!name.empty() && !backend.HasField(name)) {
				return false;
			}

			backend.BeginObject(name);
			bool success = true;
			overrides.Parameters.clear();
			if (backend.HasField("parameters")) {
				backend.BeginObject("parameters");
				backend.ForEachField([&](const std::string& parameterName) {
					HE::Rendering::MaterialParameterValue value;
					if (MaterialOverrideSerialization::DeserializeMaterialParameterValue(backend, value)) overrides.Parameters[parameterName] = std::move(value); else success = false;
				});
				backend.EndObject();
			}
			overrides.TextureParameters.clear();
			if (backend.HasField("textures")) {
				backend.BeginObject("textures");
				backend.ForEachField([&](const std::string& parameterName) { std::string guid; if (backend.Deserialize("guid", guid)) overrides.TextureParameters[parameterName] = std::move(guid); else success = false; });
				backend.EndObject();
			}
			backend.EndObject();
			return success;
		}
	};
}
