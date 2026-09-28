#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "HuaEngine/ECS/Runtime/Timeline.h"

#include <stdexcept>

namespace HE::Ecs {
    WorldAccessScope::WorldAccessScope(World& world, bool edit) noexcept : m_Lifetime(world.Lifetime()), m_Edit(edit) {}
    WorldAccessScope::~WorldAccessScope() { Release(); }
    WorldAccessScope::WorldAccessScope(WorldAccessScope&& other) noexcept
        : m_Lifetime(std::move(other.m_Lifetime)), m_Edit(other.m_Edit) {}
    WorldAccessScope& WorldAccessScope::operator=(WorldAccessScope&& other) noexcept {
        if (this != &other) { Release(); m_Lifetime = std::move(other.m_Lifetime); m_Edit = other.m_Edit; }
        return *this;
    }
    void WorldAccessScope::Release() noexcept {
        const auto lifetime = m_Lifetime.lock();
        auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
        if (world) world->EndScope(m_Edit);
        m_Lifetime.reset();
    }
    Result<void> WorldAccessScope::Open(World& world, bool edit) {
        if (!world.Context().IsMainThread()) return Error{ErrorCode::WrongThread, "WorldScope", "Synchronous scopes require the Context main thread"};
        const auto lifetime = world.Lifetime();
        if (world.Context().m_ClosingTimeline) return Error{ErrorCode::Busy, "WorldScope", "The Timeline is closing"};
        if (auto control = world.Context().TimelineSnapshot()) {
            auto synchronized = control->SynchronizeWorld(world);
            if (!synchronized) return synchronized;
        }
        const auto alive = lifetime.lock();
        if (!alive || alive->Owner.load(std::memory_order_acquire) != &world) return Error{ErrorCode::InvalidState, "WorldScope", "The World was destroyed while synchronizing"};
        return world.BeginScope(edit);
    }
    World& WorldAccessScope::Access(bool write) const {
        const auto lifetime = m_Lifetime.lock();
        auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
        if (!world || !world->Context().IsMainThread()) throw std::logic_error("The World scope is invalid on this thread");
        if (write) world->MarkEditAccess();
        return *world;
    }
    Result<WorldReadScope> WorldReadScope::Acquire(World& world) {
        auto opened = Open(world, false);
        if (!opened) return opened.GetError();
        return WorldReadScope(world);
    }
    Result<WorldEditScope> WorldEditScope::Acquire(World& world) {
        auto opened = Open(world, true);
        if (!opened) return opened.GetError();
        return WorldEditScope(world);
    }
}
