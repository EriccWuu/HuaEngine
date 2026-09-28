#pragma once

#include "HuaEngine/ECS/Runtime/Query.h"

namespace HE::Ecs::Detail {
    struct LocalAccess {
        ChunkHandle Chunk;
        TypeId Type = InvalidTypeId;
        AccessMode Access = AccessMode::Read;
    };
    struct SubmittedRandomAccess {
        std::weak_ptr<const WorldLifetime> Lifetime;
        WorldId World = 0;
        TypeId Type = InvalidTypeId;
        AccessMode Access = AccessMode::Read;
    };
    struct PreparationOptions {
        size_t RowsPerBatch = 256;
        // Zero disables small-task merging.
        size_t SmallTaskRows = 512;
    };
    struct PreparedChunk {
        ChunkHandle Chunk;
        size_t FirstBatch = 0;
        size_t BatchCount = 0;
        size_t Rows = 0;
        std::vector<LocalAccess> Accesses;
    };
    struct PreparedBatch {
        size_t ChunkIndex = 0;
        size_t Index = 0;
        size_t RowBegin = 0;
        size_t RowCount = 0;
    };

    // The scheduler invokes coordination methods outside its graph lock.
    class PreparedExecution final {
    public:
        PreparedExecution();
        ~PreparedExecution();
        PreparedExecution(const PreparedExecution&) = delete;
        PreparedExecution& operator=(const PreparedExecution&) = delete;
        PreparedExecution(PreparedExecution&&) noexcept;
        PreparedExecution& operator=(PreparedExecution&&) noexcept;

        [[nodiscard]] std::span<const PreparedChunk> Chunks() const noexcept;
        [[nodiscard]] std::span<const PreparedBatch> Batches() const noexcept;
        [[nodiscard]] size_t CandidateRows() const noexcept;
        [[nodiscard]] bool RequiresSerial() const noexcept;
        [[nodiscard]] bool MergeSmallTask() const noexcept;

        // Begin only after predecessors published. False means Changed skipped this Chunk.
        [[nodiscard]] Result<bool> BeginChunk(size_t chunk);
        // Different batches may invoke the same callback concurrently. Captured mutable
        // state must be private to a batch or synchronized by the caller.
        [[nodiscard]] Result<void> RunBatch(size_t batch, const std::function<Result<void>(QueryBatch&)>& callback);
        // The scheduler must wait for every dispatched child before completing a Chunk.
        // Cancellation allows undispatched children to remain unstarted.
        [[nodiscard]] Result<void> CompleteChunk(size_t chunk, bool cancelled = false);
        [[nodiscard]] Result<void> Finish(bool succeeded);

    private:
        friend class QuerySubmission;
        struct Impl;
        explicit PreparedExecution(std::shared_ptr<Impl> implementation);
        std::shared_ptr<Impl> m_Impl;
    };
}
