#include "enginepch.h"
#include "ScriptRuntimeSystem.h"

#include "HuaEngine/Core/Assert.h"

namespace HE {
	SystemDescriptor ScriptRuntimeSystem::Describe() const {
		SystemDescriptor descriptor;
		descriptor.Name = "ScriptRuntimeSystem";
		descriptor.Stage = SystemStage::Update;
		descriptor.Writes = { ComponentTypeIdOf<NativeScriptComponent>() };
		return descriptor;
	}

	void ScriptRuntimeSystem::Update(SystemContext&) {
		Update();
	}

	void ScriptRuntimeSystem::Update() {
		HE_CORE_ASSERT(m_Scene, "ScriptRuntimeSystem requires a valid scene");
		HE_CORE_ASSERT(m_ScriptService, "ScriptRuntimeSystem requires a valid script service");
		(void)m_ScriptService->UpdateSceneScripts(*m_Scene);
	}
}
