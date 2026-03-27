#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Syetem.h"
#include "HuaEngine/Scene/Scene.h"
#include "ScriptService.h"

namespace HE {
	class ENGINE_API ScriptRuntimeSystem : public System {
	public:
		ScriptRuntimeSystem(Scene& scene, ScriptService& scriptService)
			: m_Scene(&scene), m_ScriptService(&scriptService) {}

		void Update() override;

	private:
		Scene* m_Scene = nullptr;
		ScriptService* m_ScriptService = nullptr;
	};
}
