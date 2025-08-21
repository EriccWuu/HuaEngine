#include "enginepch.h"
#include "Scene.h"

namespace HE{
	void Scene::Update() {
		for (auto system : m_Systems) {
			system->Update();
		}
	}
}