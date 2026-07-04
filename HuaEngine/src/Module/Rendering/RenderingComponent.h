#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Asset/AssetTypes.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/Shader/Shader.h"
#include "HuaEngine/Rendering/Texture.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/Mesh.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Reflection/ReflectionMarkers.h"
#include "HuaEngine/Serialization/SerializationCore.h"

#include <unordered_map>

namespace HE::Rendering {
	HE_REFLECT_ENUM(DisplayName="Material Blend Mode")
	enum class MaterialBlendMode {
		Opaque,
		Masked,
		Transparent
	};

	HE_REFLECT_COMPONENT(DisplayName="Camera", Category="Rendering")
	struct CameraComponent : Component {
		CameraComponent() = default;
		CameraComponent(const Ref<HE::Rendering::Camera>& camera) 
			: RuntimeCamera(camera) {}

		// Runtime camera is rebuilt or synchronized from component data and is not serialized.
		Ref<HE::Rendering::Camera> RuntimeCamera;
		HE_REFLECT_FIELD()
		bool Primary = true;
		HE_REFLECT_FIELD()
		bool FixedAspectRatio = false;
	};

	struct MaterialOverrideSet {
		std::unordered_map<std::string, HE::Rendering::MaterialParameterValue> Parameters;

		void SetVec4(const std::string& name, const glm::vec4& value) {
			Parameters[name] = value;
		}

		[[nodiscard]] bool Empty() const {
			return Parameters.empty();
		}
	};

	// Material component
	HE_REFLECT_COMPONENT(DisplayName="Material", Category="Rendering")
	struct MaterialComponent : Component {
		MaterialComponent() = default;
		explicit MaterialComponent(const MaterialAssetRef& material)
			: Material(material) {}

		HE_REFLECT_FIELD()
		MaterialAssetRef Material;
		HE_REFLECT_FIELD()
		MaterialOverrideSet Overrides;
		HE_REFLECT_FIELD()
		MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
	};

	// Legacy RendererComponent for backward compatibility (deprecated)
	struct RendererComponent : Component {
		RendererComponent() = default;
		RendererComponent(const Ref<HE::Rendering::Shader>& shader, const Ref<HE::Rendering::Texture>& texture)
			: Shader(shader), Texture(texture) {}

		Ref<HE::Rendering::Shader> Shader;
		Ref<HE::Rendering::Texture> Texture;
	};

	HE_REFLECT_COMPONENT(DisplayName="Mesh", Category="Rendering")
	struct MeshComponent : Component {
		MeshComponent() = default;
		explicit MeshComponent(const MeshAssetRef& mesh)
			: Mesh(mesh) {}

		HE_REFLECT_FIELD()
		MeshAssetRef Mesh;
	};
}

namespace HE::Serialization {
	template<>
	struct Serializer<HE::Rendering::MaterialOverrideSet> {
		static void Serialize(
			SerializationBackend& backend,
			const std::string& name,
			const HE::Rendering::MaterialOverrideSet& overrides) {
			backend.BeginObject(name);
			backend.BeginObject("parameters");
			for (const auto& [parameterName, value] : overrides.Parameters) {
				backend.BeginObject(parameterName);
				if (const auto* vec4Value = std::get_if<glm::vec4>(&value)) {
					backend.Serialize("type", "vec4");
					backend.BeginArray("value", 4);
					backend.BeginArrayElement(0);
					backend.Serialize("", vec4Value->x);
					backend.EndArrayElement();
					backend.BeginArrayElement(1);
					backend.Serialize("", vec4Value->y);
					backend.EndArrayElement();
					backend.BeginArrayElement(2);
					backend.Serialize("", vec4Value->z);
					backend.EndArrayElement();
					backend.BeginArrayElement(3);
					backend.Serialize("", vec4Value->w);
					backend.EndArrayElement();
					backend.EndArray();
				}
				backend.EndObject();
			}
			backend.EndObject();
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
			if (!backend.HasField("parameters")) {
				backend.EndObject();
				return true;
			}

			backend.BeginObject("parameters");
			bool success = true;
			overrides.Parameters.clear();
			backend.ForEachField([&](const std::string& parameterName) {
				std::string type;
				if (!backend.Deserialize("type", type)) {
					(void)backend.Deserialize("value_type", type);
				}

				if (type != "vec4" && type != "Vec4") {
					return;
				}

				glm::vec4 value(0.0f);
				bool valueSuccess = false;
				if (backend.HasField("value") && backend.GetFieldType("value") == SerializationType::Array &&
					backend.GetArraySize("value") == 4) {
					backend.BeginArray("value");
					backend.BeginArrayElement(0);
					valueSuccess = backend.Deserialize("", value.x);
					backend.EndArrayElement();
					backend.BeginArrayElement(1);
					valueSuccess &= backend.Deserialize("", value.y);
					backend.EndArrayElement();
					backend.BeginArrayElement(2);
					valueSuccess &= backend.Deserialize("", value.z);
					backend.EndArrayElement();
					backend.BeginArrayElement(3);
					valueSuccess &= backend.Deserialize("", value.w);
					backend.EndArrayElement();
					backend.EndArray();
				}
				else if (backend.HasField("value") && backend.GetFieldType("value") == SerializationType::Object) {
					backend.BeginObject("value");
					valueSuccess = backend.Deserialize("x", value.x);
					valueSuccess &= backend.Deserialize("y", value.y);
					valueSuccess &= backend.Deserialize("z", value.z);
					valueSuccess &= backend.Deserialize("w", value.w);
					backend.EndObject();
				}

				if (valueSuccess) {
					overrides.Parameters[parameterName] = value;
				}
				else {
					success = false;
				}
			});
			backend.EndObject();
			backend.EndObject();
			return success;
		}
	};
}
