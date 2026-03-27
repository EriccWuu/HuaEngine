#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "HuaEngine.h"
#include "HuaEngine/Script/ScriptService.h"
#include "HuaEngine/Script/ScriptRuntimeSystem.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ScriptServiceSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void RequirePayloadValue(const HE::ResultEnvelope& result, const std::string& key, const std::string& expectedValue) {
		auto it = result.Payload.find(key);
		Require(it != result.Payload.end(), "Expected payload key '" + key + "' to exist");
		Require(it->second == expectedValue, "Expected payload key '" + key + "' to equal '" + expectedValue + "'");
	}

	struct CountingScript final : HE::ScriptableEntity {
		static inline int CreateCount = 0;
		static inline int UpdateCount = 0;
		static inline int DestroyCount = 0;

		static void Reset() {
			CreateCount = 0;
			UpdateCount = 0;
			DestroyCount = 0;
		}

	protected:
		void OnCreate() override {
			++CreateCount;
			GetComponent<HE::TransformComponent>().Position.x += 10.0f;
		}

		void OnUpdate() override {
			++UpdateCount;
			GetComponent<HE::TransformComponent>().Position.x += 1.0f;
		}

		void OnDestroy() override {
			++DestroyCount;
		}
	};

	struct ReplacementScript final : HE::ScriptableEntity {
		static inline int CreateCount = 0;
		static inline int UpdateCount = 0;
		static inline int DestroyCount = 0;

		static void Reset() {
			CreateCount = 0;
			UpdateCount = 0;
			DestroyCount = 0;
		}

	protected:
		void OnCreate() override {
			++CreateCount;
		}

		void OnUpdate() override {
			++UpdateCount;
		}

		void OnDestroy() override {
			++DestroyCount;
		}
	};
}

int main() {
	HE::Log::Init();
	CountingScript::Reset();
	ReplacementScript::Reset();

	HE::Scene scene("ScriptSmokeScene");
	HE::ScriptService scriptService;
	scene.AddSyetem(HE::CreateRef<HE::ScriptRuntimeSystem>(scene, scriptService));

	auto scriptedEntity = scene.GetEntityManager().CreateEntity();
	auto bindResult = scriptService.BindNativeScript<CountingScript>(scriptedEntity, "CountingScript");
	Require(bindResult.Succeeded(), "Expected script.bind to succeed");

	HE::ScriptStatusReport initialReport;
	auto initialStatus = scriptService.CheckSceneScripts(scene, &initialReport);
	Require(initialStatus.Succeeded(), "Expected script.status to succeed for a bound script component");
	Require(initialReport.TotalScriptComponents == 1, "Expected exactly one script component before runtime starts");
	Require(initialReport.ActiveScriptInstances == 0, "Expected no script instances before initialization");

	auto initializeResult = scriptService.InitializeSceneScripts(scene, &initialReport);
	Require(initializeResult.Succeeded(), "Expected script.initialize to succeed");
	Require(CountingScript::CreateCount == 1, "Expected initialization to create the script instance exactly once");
	Require(initialReport.ActiveScriptInstances == 1, "Expected initialization report to reflect one active script instance");
	RequirePayloadValue(initializeResult, "active_script_instances", "1");

	HE::ScriptStatusReport updateReport;
	auto updateResult = scriptService.UpdateSceneScripts(scene, &updateReport);
	Require(updateResult.Succeeded(), "Expected direct script.update to succeed");
	Require(CountingScript::UpdateCount == 1, "Expected direct script.update to advance the script once");
	Require(updateReport.ActiveScriptInstances == 1, "Expected update report to reflect one active script instance");
	RequirePayloadValue(updateResult, "active_script_instances", "1");

	scene.Update();
	scene.Update();
	Require(CountingScript::UpdateCount == 3, "Expected script runtime to advance once directly and twice through Scene::Update");
	Require(scriptedEntity.GetComponent<HE::TransformComponent>().Position.x == 13.0f, "Expected script lifecycle to mutate the entity transform during create/update");

	HE::ScriptStatusReport activeReport;
	auto activeStatus = scriptService.CheckSceneScripts(scene, &activeReport);
	Require(activeStatus.Succeeded(), "Expected active scene scripts to remain operational");
	Require(activeReport.ActiveScriptInstances == 1, "Expected exactly one active script instance");

	auto rebindResult = scriptService.BindNativeScript<ReplacementScript>(scriptedEntity, "ReplacementScript");
	Require(rebindResult.Succeeded(), "Expected active script rebind to succeed");
	Require(CountingScript::DestroyCount == 1, "Expected rebind to destroy the previously active script instance");
	Require(scriptedEntity.GetComponent<HE::NativeScriptComponent>().Instance == nullptr, "Expected rebind to clear the previous runtime instance until the new script initializes");

	auto reinitializeResult = scriptService.InitializeSceneScripts(scene, &activeReport);
	Require(reinitializeResult.Succeeded(), "Expected reinitialized replacement script to succeed");
	Require(ReplacementScript::CreateCount == 1, "Expected replacement script to initialize exactly once");

	auto unbindResult = scriptService.UnbindNativeScript(scriptedEntity);
	Require(unbindResult.Succeeded(), "Expected script.unbind to succeed for a bound entity");
	Require(ReplacementScript::DestroyCount == 1, "Expected direct unbind to destroy the active replacement script instance");
	Require(!scriptedEntity.HasComponent<HE::NativeScriptComponent>(), "Expected unbind to remove the native script component");

	auto invalidEntity = scene.GetEntityManager().CreateEntity();
	invalidEntity.AddComponent<HE::NativeScriptComponent>();

	HE::ScriptStatusReport degradedReport;
	auto degradedStatus = scriptService.CheckSceneScripts(scene, &degradedReport);
	Require(degradedStatus.RequiresManualIntervention(), "Expected unbound NativeScriptComponent to require manual intervention");
	Require(degradedReport.MissingBindingComponents == 1, "Expected degraded report to count the missing script binding");

	auto shutdownResult = scriptService.ShutdownSceneScripts(scene, &activeReport);
	Require(shutdownResult.RequiresManualIntervention(), "Expected shutdown to surface the remaining missing binding issue");
	Require(activeReport.ActiveScriptInstances == 0, "Expected shutdown report to reflect zero active script instances");
	RequirePayloadValue(shutdownResult, "active_script_instances", "0");

	std::cout << "ScriptServiceSmoke passed" << std::endl;
	return 0;
}
