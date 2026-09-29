#pragma once

#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

#include "HuaEngine/ECS/Runtime/TypeRegistry.h"
#include "HuaEngine/ECS/Runtime/Resources.h"

namespace HE::Ecs {
	class Query;
	struct QuerySpec;
	class Timeline;
	class World;
	class WorldAccessScope;
	namespace Detail {
		struct QueryCacheState;
		struct QueryInstanceCache;
		class WorkerPool;
		// The Context leases this control state across World destruction and scopes.
		class TimelineControl {
		public:
			virtual ~TimelineControl() = default;
			[[nodiscard]] virtual Result<void> SynchronizeWorld(World& world) = 0;
			virtual void WorldDestroyed(World& world) noexcept = 0;
		};
	}
	struct EcsContextOptions { size_t WorkerCount = 0; };
	class EcsContext final {
	public:
		explicit EcsContext(EcsContextOptions options = {});
		~EcsContext();
		EcsContext(const EcsContext&) = delete;
		EcsContext& operator=(const EcsContext&) = delete;
		EcsContext(EcsContext&&) = delete;
		EcsContext& operator=(EcsContext&&) = delete;

		[[nodiscard]] TypeRegistry& Types() { return m_Types; }
		[[nodiscard]] const TypeRegistry& Types() const { return m_Types; }
		[[nodiscard]] Refl::Registry& Reflection() noexcept { return m_Types.Reflection(); }
		[[nodiscard]] const Refl::Registry& Reflection() const noexcept { return m_Types.Reflection(); }
		[[nodiscard]] ResourceRegistry& Resources() { return m_Resources; }
		[[nodiscard]] const ResourceRegistry& Resources() const { return m_Resources; }
		[[nodiscard]] bool IsMainThread() const noexcept { return std::this_thread::get_id() == m_MainThread; }
		[[nodiscard]] bool HasScheduledWork() const noexcept { return m_ScheduledWork.load(std::memory_order_acquire) != 0 || m_ClosingTimeline.load(std::memory_order_acquire); }
		[[nodiscard]] size_t WorkerCount() const noexcept { return m_WorkerCount; }
		// Jobs reuse one Query per complete binding in this Context.
		[[nodiscard]] Result<Query*> FindOrCreateQuery(std::string_view declaration, QuerySpec spec);

	private:
		friend class Query;
		friend class Timeline;
		friend class World;
		friend class WorldAccessScope;
		TypeRegistry m_Types;
		ResourceRegistry m_Resources;
		std::thread::id m_MainThread = std::this_thread::get_id();
		std::shared_ptr<Detail::QueryCacheState> m_QueryCache;
		std::unique_ptr<Detail::QueryInstanceCache> m_QueryInstanceCache;
		mutable std::mutex m_TimelineMutex;
		std::shared_ptr<Detail::TimelineControl> m_TimelineControl;
		std::atomic<bool> m_ClosingTimeline{false};
		std::atomic<size_t> m_ScheduledWork{0};
		const size_t m_WorkerCount;
		std::unique_ptr<Detail::WorkerPool> m_Workers;
		[[nodiscard]] Detail::WorkerPool& Workers();
		[[nodiscard]] std::shared_ptr<Detail::TimelineControl> TimelineSnapshot() const;
	};
}
