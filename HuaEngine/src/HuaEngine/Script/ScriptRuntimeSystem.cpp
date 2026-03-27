#include "enginepch.h"
#include "ScriptRuntimeSystem.h"

#include "HuaEngine/Core/Assert.h"

namespace HE {
	void ScriptRuntimeSystem::Update() {
		HE_CORE_ASSERT(m_Scene, "ScriptRuntimeSystem requires a valid scene");
		HE_CORE_ASSERT(m_ScriptService, "ScriptRuntimeSystem requires a valid script service");
		(void)m_ScriptService->UpdateSceneScripts(*m_Scene);
	}
}
