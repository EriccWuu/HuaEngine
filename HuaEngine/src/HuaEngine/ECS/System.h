#pragma once

#include <string>
#include <vector>

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/CommandBuffer.h"
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
		CommandBuffer& Commands() { return m_ActiveCommandBuffer ? *m_ActiveCommandBuffer : m_OwnedCommandBuffer; }
		float DeltaTime() const { return m_DeltaTime; }

	private:
		void SetCommandBuffer(CommandBuffer* commandBuffer) {
			m_ActiveCommandBuffer = commandBuffer;
		}

	private:
		World& m_World;
		CommandBuffer m_OwnedCommandBuffer;
		CommandBuffer* m_ActiveCommandBuffer = nullptr;
		float m_DeltaTime = 0.0f;

		friend class Scheduler;
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
