#pragma once

#include <cstdint>
#include <string_view>

#include "HuaEngine/Core/ResultEnvelope.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/ECS/ScriptableEntity.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE {
	struct ScriptStatusReport {
		uint32_t TotalScriptComponents = 0;
		uint32_t EnabledScriptComponents = 0;
		uint32_t BoundScriptComponents = 0;
		uint32_t ActiveScriptInstances = 0;
		uint32_t MissingBindingComponents = 0;

		[[nodiscard]] bool IsOperational() const {
			return MissingBindingComponents == 0;
		}

		[[nodiscard]] bool HasIssues() const {
			return !IsOperational();
		}
	};

	class ENGINE_API ScriptService {
	public:
		template<typename T>
		[[nodiscard]] ResultEnvelope BindNativeScript(Entity entity, std::string_view scriptName = {}) const {
			if (!entity.IsValid()) {
				auto result = ResultEnvelope::Failure("script.bind", {}, "Entity is invalid");
				result.AddDetail({ DiagnosticSeverity::Error, "script.entity.invalid", "Native script binding requires a valid entity", {} });
				return result;
			}

			auto& scriptComponent = entity.HasComponent<NativeScriptComponent>()
				? entity.GetComponent<NativeScriptComponent>()
				: entity.AddComponent<NativeScriptComponent>();

			DestroyScriptInstance(scriptComponent);
			scriptComponent.Bind<T>(scriptName);

			auto targetId = scriptComponent.ScriptName.empty() ? std::to_string(entity.GetUid()) : scriptComponent.ScriptName;
			auto result = ResultEnvelope::Success("script.bind", targetId, "Native script bound");
			result.SetPayloadValue("entity_id", std::to_string(entity.GetUid()));
			result.SetPayloadValue("script_name", scriptComponent.ScriptName);
			return result;
		}

		[[nodiscard]] ResultEnvelope UnbindNativeScript(Entity entity) const;
		[[nodiscard]] ResultEnvelope InitializeSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;
		[[nodiscard]] ResultEnvelope UpdateSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;
		[[nodiscard]] ResultEnvelope ShutdownSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;
		[[nodiscard]] ResultEnvelope CheckSceneScripts(Scene& scene, ScriptStatusReport* outReport = nullptr) const;

	private:
		void DestroyScriptInstance(NativeScriptComponent& scriptComponent) const;
	};
}
