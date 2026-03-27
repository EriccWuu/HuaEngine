#include "enginepch.h"
#include "ScriptService.h"

namespace {
	std::string CountToString(uint32_t value) {
		return std::to_string(value);
	}

	std::string MakeSceneTargetId(HE::Scene& scene) {
		return scene.GetName().empty() ? std::string("<unnamed-scene>") : scene.GetName();
	}

	void ApplyReportPayload(HE::ResultEnvelope& result, const HE::ScriptStatusReport& report) {
		result.SetPayloadValue("total_script_components", CountToString(report.TotalScriptComponents));
		result.SetPayloadValue("enabled_script_components", CountToString(report.EnabledScriptComponents));
		result.SetPayloadValue("bound_script_components", CountToString(report.BoundScriptComponents));
		result.SetPayloadValue("active_script_instances", CountToString(report.ActiveScriptInstances));
		result.SetPayloadValue("missing_binding_components", CountToString(report.MissingBindingComponents));
	}

	template<typename Callback>
	void ForEachScriptComponent(HE::Scene& scene, Callback&& callback) {
		auto view = scene.View<HE::NativeScriptComponent>();
		for (auto entityHandle : view) {
			HE::Entity entity(entityHandle, &scene.GetEntityManager());
			auto& scriptComponent = view.template get<HE::NativeScriptComponent>(entityHandle);
			callback(entity, scriptComponent);
		}
	}

	HE::ScriptStatusReport CollectScriptStatus(HE::Scene& scene) {
		HE::ScriptStatusReport report;
		auto view = scene.View<HE::NativeScriptComponent>();
		for (auto entityHandle : view) {
			++report.TotalScriptComponents;

			auto& scriptComponent = view.template get<HE::NativeScriptComponent>(entityHandle);
			if (scriptComponent.Enabled) {
				++report.EnabledScriptComponents;
			}
			if (scriptComponent.IsBound()) {
				++report.BoundScriptComponents;
			}
			if (scriptComponent.Instance != nullptr) {
				++report.ActiveScriptInstances;
			}
			if (scriptComponent.Enabled && !scriptComponent.IsBound()) {
				++report.MissingBindingComponents;
			}
		}

		return report;
	}

	HE::ResultEnvelope MakeSceneScriptResult(
		std::string_view operation,
		HE::Scene& scene,
		const HE::ScriptStatusReport& report,
		std::string_view successSummary,
		std::string_view manualSummary) {
		auto result = report.IsOperational()
			? HE::ResultEnvelope::Success(std::string(operation), MakeSceneTargetId(scene), std::string(successSummary))
			: HE::ResultEnvelope::ManualIntervention(std::string(operation), MakeSceneTargetId(scene), std::string(manualSummary));
		ApplyReportPayload(result, report);
		if (report.MissingBindingComponents > 0) {
			result.AddDetail({ HE::DiagnosticSeverity::Error, "script.binding.missing", "One or more enabled NativeScriptComponent values are missing binding functions", CountToString(report.MissingBindingComponents) });
		}
		return result;
	}
}

namespace HE {
	void ScriptService::DestroyScriptInstance(NativeScriptComponent& scriptComponent) const {
		if (scriptComponent.Instance && scriptComponent.HasCreated) {
			scriptComponent.Instance->__OnDestroy();
		}

		scriptComponent.ReleaseInstance();
	}

	ResultEnvelope ScriptService::UnbindNativeScript(Entity entity) const {
		if (!entity.IsValid() || !entity.HasComponent<NativeScriptComponent>()) {
			auto result = ResultEnvelope::Failure("script.unbind", {}, "Entity has no native script binding");
			result.AddDetail({ DiagnosticSeverity::Error, "script.entity.missing_component", "Native script unbind requires an entity with NativeScriptComponent", {} });
			return result;
		}

		auto& scriptComponent = entity.GetComponent<NativeScriptComponent>();
		DestroyScriptInstance(scriptComponent);
		entity.RemoveComponent<NativeScriptComponent>();

		auto result = ResultEnvelope::Success("script.unbind", std::to_string(entity.GetUid()), "Native script unbound");
		result.SetPayloadValue("entity_id", std::to_string(entity.GetUid()));
		return result;
	}

	ResultEnvelope ScriptService::InitializeSceneScripts(Scene& scene, ScriptStatusReport* outReport) const {
		ForEachScriptComponent(scene, [](Entity entity, NativeScriptComponent& scriptComponent) {
			if (scriptComponent.Enabled && scriptComponent.IsBound() && scriptComponent.Instance == nullptr) {
				scriptComponent.Instance = scriptComponent.InstanceFunc();
				HE_CORE_ASSERT(scriptComponent.Instance, "Native script factory returned null");
				scriptComponent.Instance->__BindEntity(entity);
			}

			if (scriptComponent.Enabled && scriptComponent.Instance && !scriptComponent.HasCreated) {
				scriptComponent.Instance->__OnCreate();
				scriptComponent.HasCreated = true;
			}
		});

		auto report = CollectScriptStatus(scene);
		if (outReport) {
			*outReport = report;
		}

		return MakeSceneScriptResult(
			"script.initialize",
			scene,
			report,
			"Scene scripts initialized",
			"Scene scripts require binding fixes");
	}

	ResultEnvelope ScriptService::UpdateSceneScripts(Scene& scene, ScriptStatusReport* outReport) const {
		ForEachScriptComponent(scene, [](Entity entity, NativeScriptComponent& scriptComponent) {
			if (scriptComponent.Enabled && scriptComponent.IsBound() && scriptComponent.Instance == nullptr) {
				scriptComponent.Instance = scriptComponent.InstanceFunc();
				HE_CORE_ASSERT(scriptComponent.Instance, "Native script factory returned null");
				scriptComponent.Instance->__BindEntity(entity);
			}

			if (scriptComponent.Enabled && scriptComponent.Instance) {
				if (!scriptComponent.HasCreated) {
					scriptComponent.Instance->__OnCreate();
					scriptComponent.HasCreated = true;
				}

				scriptComponent.Instance->__OnUpdate();
			}
		});

		auto report = CollectScriptStatus(scene);
		if (outReport) {
			*outReport = report;
		}

		return MakeSceneScriptResult(
			"script.update",
			scene,
			report,
			"Scene scripts updated",
			"Scene scripts require binding fixes");
	}

	ResultEnvelope ScriptService::ShutdownSceneScripts(Scene& scene, ScriptStatusReport* outReport) const {
		ForEachScriptComponent(scene, [this](Entity, NativeScriptComponent& scriptComponent) {
			DestroyScriptInstance(scriptComponent);
		});

		auto report = CollectScriptStatus(scene);
		if (outReport) {
			*outReport = report;
		}

		return MakeSceneScriptResult(
			"script.shutdown",
			scene,
			report,
			"Scene scripts shut down",
			"Scene scripts shut down with binding issues still present");
	}

	ResultEnvelope ScriptService::CheckSceneScripts(Scene& scene, ScriptStatusReport* outReport) const {
		auto report = CollectScriptStatus(scene);

		if (outReport) {
			*outReport = report;
		}

		return MakeSceneScriptResult(
			"script.status",
			scene,
			report,
			"Scene script bindings are operational",
			"Scene script bindings require fixes");
	}
}
