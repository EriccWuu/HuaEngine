#pragma once

#include "HuaEngine/ECS/Runtime/PreparedExecution.h"

namespace HE::Ecs::Detail {
    // A queued task owns its borrows and observer reservation until Release.
    class QuerySubmission final {
    public:
        QuerySubmission();
        ~QuerySubmission();
        QuerySubmission(const QuerySubmission&) = delete;
        QuerySubmission& operator=(const QuerySubmission&) = delete;
        QuerySubmission(QuerySubmission&&) noexcept;
        QuerySubmission& operator=(QuerySubmission&&) noexcept;

        [[nodiscard]] static Result<QuerySubmission> Capture(Query& query, World& world, ChangedState* changed = nullptr);
        [[nodiscard]] EcsContext* Context() const noexcept;
        [[nodiscard]] WorldId MainId() const noexcept;
        [[nodiscard]] std::weak_ptr<const WorldLifetime> MainLifetime() const noexcept;
        [[nodiscard]] std::span<const LocalAccess> LocalAccesses() const noexcept;
        [[nodiscard]] std::span<const SubmittedRandomAccess> RandomAccesses() const noexcept;
        [[nodiscard]] std::span<const ResourceAccess> ResourceAccesses() const noexcept;
        [[nodiscard]] uint64_t ConsumerId() const noexcept;
        [[nodiscard]] Result<PreparedExecution> Prepare(PreparationOptions options = {});

        // Execution is single-use and releases borrows and reservations before returning.
        // Summary access remains valid until Release, which also cancels unexecuted work.
        [[nodiscard]] Result<void> Execute(const std::function<Result<void>(QueryBatch&)>& callback);
        void Release() noexcept;

    private:
        friend class PreparedExecution;
        struct Impl;
        explicit QuerySubmission(std::shared_ptr<Impl> implementation);
        std::shared_ptr<Impl> m_Impl;
    };
}
