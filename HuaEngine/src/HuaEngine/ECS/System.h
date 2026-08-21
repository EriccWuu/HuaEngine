#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/CommandBuffer.h"
#include "HuaEngine/ECS/FrameContext.h"
#include "HuaEngine/ECS/World.h"

namespace HE {
	enum class SystemStage {
		PreUpdate,
		Update,
		PostUpdate,
		Render
	};

	enum class SystemAccessTarget {
		Component,
		FrameResource
	};

	enum class SystemAccessMode {
		Read,
		Write
	};

	struct SystemAccess {
		SystemAccessTarget Target = SystemAccessTarget::Component;
		SystemAccessMode Mode = SystemAccessMode::Read;
		ComponentTypeId ComponentType = InvalidComponentTypeId;
		std::string FrameResourceName;

		template<typename T>
		static SystemAccess ReadComponent() {
			return { SystemAccessTarget::Component, SystemAccessMode::Read, ComponentTypeIdOf<T>(), {} };
		}

		template<typename T>
		static SystemAccess WriteComponent() {
			return { SystemAccessTarget::Component, SystemAccessMode::Write, ComponentTypeIdOf<T>(), {} };
		}

		static SystemAccess ReadFrameResource(std::string_view resourceName) {
			return { SystemAccessTarget::FrameResource, SystemAccessMode::Read, InvalidComponentTypeId, std::string(resourceName) };
		}

		static SystemAccess WriteFrameResource(std::string_view resourceName) {
			return { SystemAccessTarget::FrameResource, SystemAccessMode::Write, InvalidComponentTypeId, std::string(resourceName) };
		}

		[[nodiscard]] bool TargetsSame(const SystemAccess& other) const {
			if (Target != other.Target) {
				return false;
			}

			return Target == SystemAccessTarget::Component
				? ComponentType == other.ComponentType
				: FrameResourceName == other.FrameResourceName;
		}
	};

	struct SystemDescriptor {
		std::string Name;
		SystemStage Stage = SystemStage::Update;
		std::vector<SystemAccess> Accesses;
		bool Enabled = true;
	};

	class SystemContext {
	public:
		SystemContext(World& world, FrameContext& frame, float deltaTime)
			: m_World(world), m_Frame(frame), m_DeltaTime(deltaTime) {}

		SystemContext(World& world, float deltaTime)
			: m_World(world), m_Frame(m_OwnedFrame), m_DeltaTime(deltaTime) {}

		World& WorldRef() { return m_World; }
		const World& WorldRef() const { return m_World; }
		FrameContext& Frame() { return m_Frame; }
		const FrameContext& Frame() const { return m_Frame; }
		CommandBuffer& Commands() { return m_ActiveCommandBuffer ? *m_ActiveCommandBuffer : m_OwnedCommandBuffer; }
		float DeltaTime() const { return m_DeltaTime; }

	private:
		void SetCommandBuffer(CommandBuffer* commandBuffer) {
			m_ActiveCommandBuffer = commandBuffer;
		}

	private:
		World& m_World;
		FrameContext m_OwnedFrame;
		FrameContext& m_Frame;
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
