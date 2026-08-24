#include "enginepch.h"
#include "MaterialDefinition.h"

#include "Module/Rendering/RenderingComponent.h"

namespace HE::Rendering {
	bool ReconcileMaterialOverrides(MaterialOverrideSet& overrides, const MaterialDefinition& definition) {
		bool changed = false;
		for (auto iterator = overrides.Parameters.begin(); iterator != overrides.Parameters.end();) {
			const auto parameter = std::find_if(definition.GetParameters().begin(), definition.GetParameters().end(), [&](const auto& value) { return value.Name == iterator->first; });
			const bool compatible = parameter != definition.GetParameters().end() && (
				(parameter->Type == ShaderValueType::Int && std::holds_alternative<int>(iterator->second)) ||
				(parameter->Type == ShaderValueType::Float && std::holds_alternative<float>(iterator->second)) ||
				(parameter->Type == ShaderValueType::Float2 && std::holds_alternative<glm::vec2>(iterator->second)) ||
				(parameter->Type == ShaderValueType::Float3 && std::holds_alternative<glm::vec3>(iterator->second)) ||
				(parameter->Type == ShaderValueType::Float4 && std::holds_alternative<glm::vec4>(iterator->second)) ||
				(parameter->Type == ShaderValueType::Float4x4 && std::holds_alternative<glm::mat4>(iterator->second)));
			if (compatible) ++iterator; else { iterator = overrides.Parameters.erase(iterator); changed = true; }
		}
		for (auto iterator = overrides.TextureParameters.begin(); iterator != overrides.TextureParameters.end();) {
			const auto parameter = std::find_if(definition.GetParameters().begin(), definition.GetParameters().end(), [&](const auto& value) { return value.Name == iterator->first; });
			if (parameter != definition.GetParameters().end() && parameter->Type == ShaderValueType::Texture2D) ++iterator; else { iterator = overrides.TextureParameters.erase(iterator); changed = true; }
		}
		return changed;
	}
}
