#pragma once

#include "HuaEngine/ECS/Runtime/World.h"

namespace HE::Ecs {
    // Scopes are created, used, moved, and destroyed on the Context main thread.
    class WorldAccessScope {
    protected:
        WorldAccessScope(World& world, bool edit) noexcept;
        ~WorldAccessScope();
        WorldAccessScope(const WorldAccessScope&) = delete;
        WorldAccessScope& operator=(const WorldAccessScope&) = delete;
        WorldAccessScope(WorldAccessScope&& other) noexcept;
        WorldAccessScope& operator=(WorldAccessScope&& other) noexcept;
        [[nodiscard]] static Result<void> Open(World& world, bool edit);
        [[nodiscard]] World& Access(bool write) const;
    private:
        void Release() noexcept;
        std::weak_ptr<const Detail::WorldLifetime> m_Lifetime;
        bool m_Edit = false;
    };

    class WorldReadScope final : private WorldAccessScope {
    public:
        ~WorldReadScope() = default;
        WorldReadScope(WorldReadScope&&) noexcept = default;
        WorldReadScope& operator=(WorldReadScope&&) noexcept = default;
        [[nodiscard]] static Result<WorldReadScope> Acquire(World& world);
        [[nodiscard]] const World& Get() const { return Access(false); }
    private:
        explicit WorldReadScope(World& world) noexcept : WorldAccessScope(world, false) {}
    };
    class WorldEditScope final : private WorldAccessScope {
    public:
        ~WorldEditScope() = default;
        WorldEditScope(WorldEditScope&&) noexcept = default;
        WorldEditScope& operator=(WorldEditScope&&) noexcept = default;
        [[nodiscard]] static Result<WorldEditScope> Acquire(World& world);
        [[nodiscard]] World& Get() const { return Access(true); }
    private:
        explicit WorldEditScope(World& world) noexcept : WorldAccessScope(world, true) {}
    };
}
