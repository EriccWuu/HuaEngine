#include "enginepch.h"
#include "Scene.h"

	namespace HE{
	void Scene::Update() {
		OnUpdate();
	}

	void Scene::OnRuntimeStart() {
		(void)m_Scheduler.Build();
	}

	void Scene::OnUpdate(float deltaTime) {
		SystemContext context{m_World, m_FrameContext, deltaTime};
		m_Scheduler.Update(context);
	}

	void Scene::OnRuntimeStop() {
	}

	void Scene::AddSystem(Ref<System> system) {
		if (!system) {
			return;
		}

		m_Systems.emplace_back(system);
		m_Scheduler.AddSystem(system);
	}
}
