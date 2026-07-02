#pragma once

#include <string>
#include <vector>

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/World.h"

namespace HE {
	enum class SystemStage {
		PreUpdate,
		Update,
		PostUpdate,
		Render
	};

	struct SystemDescriptor {
		std::string Name;
		SystemStage Stage = SystemStage::Update;
		std::vector<std::string> Before;
		std::vector<std::string> After;
		std::vector<ComponentTypeId> Reads;
		std::vector<ComponentTypeId> Writes;
		std::vector<std::string> ResourceReads;
		std::vector<std::string> ResourceWrites;
		bool Enabled = true;
	};

	class SystemContext {
	public:
		SystemContext(World& world, float deltaTime)
			: m_World(world), m_DeltaTime(deltaTime) {}

		World& WorldRef() { return m_World; }
		const World& WorldRef() const { return m_World; }
		float DeltaTime() const { return m_DeltaTime; }

	private:
		World& m_World;
		float m_DeltaTime = 0.0f;
	};

	class System {
	public:
		virtual ~System() = default;

		virtual SystemDescriptor Describe() const {
			SystemDescriptor descriptor;
			descriptor.Name = "System";
			return descriptor;
		}

		virtual void Update(SystemContext&) {
			Update();
		}

		virtual void Update() {}
	};
}
