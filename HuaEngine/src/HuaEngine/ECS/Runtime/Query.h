#pragma once

#include "HuaEngine/ECS/Runtime/ColumnView.h"

#include <functional>

namespace HE::Ecs {
    enum class AccessMode { Read, Write };
    enum class Presence { Required, Optional };

    struct QueryTerm {
        TypeId Type = InvalidTypeId;
        Presence Match = Presence::Required;
        AccessMode Access = AccessMode::Read;
    };
    struct RandomAccess {
        World* Target = nullptr;
        TypeId Type = InvalidTypeId;
        AccessMode Access = AccessMode::Read;
        // Filled by Query::Create so later planners need not dereference Target.
        WorldId TargetId = 0;
        std::weak_ptr<const Detail::WorldLifetime> Lifetime;
    };
    struct ResourceAccess {
        ResourceHandle Resource;
        AccessMode Access = AccessMode::Read;
    };
    struct QuerySpec {
        std::vector<QueryTerm> Columns;
        std::vector<TypeId> Required;
        std::vector<TypeId> Exclude;
        std::vector<SharedBinding> SharedBindings;
        std::vector<TypeId> ChangedTypes;
        std::vector<RandomAccess> RandomAccesses;
        std::vector<ResourceAccess> ResourceAccesses;
        bool IncludeDisabledEntities = false;
        bool IgnoreComponentEnabled = false;
    };
    struct QueryStats {
        uint64_t GroupMatchCount = 0;
        uint64_t LayoutBindCount = 0;
        uint64_t CacheHits = 0;
        uint64_t CandidateChunks = 0;
        uint64_t VisitedRows = 0;
    };

    namespace Detail {
        class QuerySubmission;
        class PreparedExecution;
        struct QueryExecution;
        struct QueryCacheState;
        struct RandomBinding {
            std::weak_ptr<QueryExecution> Execution;
            WorldId World = 0;
            TypeId Type = InvalidTypeId;
        };
        [[nodiscard]] Result<RandomBinding> BindRandom(const std::weak_ptr<QueryBatchData>& batch,
            World& world, const void* nativeKey, bool write);
        [[nodiscard]] Result<RandomBinding> BindRandom(const std::weak_ptr<QueryBatchData>& batch,
            WorldId world, const void* nativeKey, bool write);
        [[nodiscard]] Result<void*> RandomPointer(const RandomBinding& binding, EntityId entity,
            const void* nativeKey, bool write);
        [[nodiscard]] Result<void*> ResourcePointer(const std::weak_ptr<QueryBatchData>& batch,
            ResourceHandle resource, const void* nativeKey, bool write);
    }

    template<typename T>
    class RandomView {
    public:
        RandomView() = default;
        [[nodiscard]] Result<T*> TryGet(EntityId entity) const {
            auto pointer = Detail::RandomPointer(m_Binding, entity,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            if (!pointer) return pointer.GetError();
            return static_cast<T*>(pointer.Value());
        }
    private:
        friend class QueryBatch;
        explicit RandomView(Detail::RandomBinding binding) : m_Binding(std::move(binding)) {}
        Detail::RandomBinding m_Binding;
    };

    class QueryBatch {
    public:
        QueryBatch() = default;
        [[nodiscard]] bool Valid() const noexcept { return Detail::BatchValid(m_Data); }
        [[nodiscard]] size_t Size() const noexcept { return Detail::BatchSize(m_Data); }
        [[nodiscard]] EntityId Entity(size_t row) const noexcept;
        [[nodiscard]] ChunkHandle Chunk() const noexcept;
        template<typename T>
        [[nodiscard]] Result<ColumnView<T>> Column(size_t argument) const {
            auto access = Detail::ValidateColumn(m_Data, argument,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            if (!access) return access.GetError();
            return ColumnView<T>(m_Data, argument);
        }
        template<typename T>
        [[nodiscard]] Result<RandomView<T>> Random(World& world) const {
            auto binding = Detail::BindRandom(m_Data, world,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            if (!binding) return binding.GetError();
            return RandomView<T>(binding.Value());
        }
        template<typename T>
        [[nodiscard]] Result<RandomView<T>> Random(WorldId world) const {
            auto binding = Detail::BindRandom(m_Data, world,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            if (!binding) return binding.GetError();
            return RandomView<T>(binding.Value());
        }
        template<typename T>
        [[nodiscard]] Result<std::reference_wrapper<T>> Resource(ResourceHandle resource) const {
            auto pointer = Detail::ResourcePointer(m_Data, resource,
                NativeTypeKey<std::remove_const_t<T>>(), !std::is_const_v<T>);
            if (!pointer) return pointer.GetError();
            return std::ref(*static_cast<T*>(pointer.Value()));
        }
    private:
        friend class Query;
        friend class Detail::PreparedExecution;
        explicit QueryBatch(std::weak_ptr<Detail::QueryBatchData> data) : m_Data(std::move(data)) {}
        std::weak_ptr<Detail::QueryBatchData> m_Data;
    };

    class QueryRow {
    public:
        [[nodiscard]] EntityId Entity() const noexcept { return m_Batch.Entity(m_Row); }
        [[nodiscard]] bool Valid() const noexcept { return m_Batch.Valid(); }
        template<typename T>
        [[nodiscard]] Result<T*> Optional(size_t argument) const {
            auto column = m_Batch.Column<T>(argument);
            if (!column) return column.GetError();
            return column.Value().TryGet(m_Row);
        }
        template<typename T>
        [[nodiscard]] Result<std::reference_wrapper<T>> Get(size_t argument) const {
            auto pointer = Optional<T>(argument);
            if (!pointer) return pointer.GetError();
            if (!pointer.Value()) return Error{ErrorCode::InvalidState, "QueryRow", "The optional component is absent"};
            return std::ref(*pointer.Value());
        }
        template<typename T> [[nodiscard]] Result<RandomView<T>> Random(World& world) const { return m_Batch.Random<T>(world); }
        template<typename T> [[nodiscard]] Result<std::reference_wrapper<T>> Resource(ResourceHandle resource) const {
            return m_Batch.Resource<T>(resource);
        }
    private:
        friend class Query;
        QueryRow(QueryBatch batch, size_t row) : m_Batch(std::move(batch)), m_Row(row) {}
        QueryBatch m_Batch;
        size_t m_Row = 0;
    };

    class ChangedState final {
    public:
        ChangedState();
        ~ChangedState();
        ChangedState(const ChangedState&) = delete;
        ChangedState& operator=(const ChangedState&) = delete;
        ChangedState(ChangedState&&) = delete;
        ChangedState& operator=(ChangedState&&) = delete;
        [[nodiscard]] uint64_t ConsumerId() const noexcept;
        [[nodiscard]] size_t ObservationCount() const noexcept;
        // Queued tasks retain the observer and prevent reset until completion or cancellation.
        // One observer may queue in only one Context until all reservations are released.
        [[nodiscard]] Result<void> Reset();
    private:
        friend class Query;
        friend class Detail::QuerySubmission;
        friend class Detail::PreparedExecution;
        struct Impl;
        std::shared_ptr<Impl> m_Impl;
    };

    // The Context outlives this Query and every task captured from it.
    class Query final {
    public:
        ~Query();
        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;
        Query(Query&&) noexcept;
        Query& operator=(Query&&) noexcept;
        [[nodiscard]] static Result<Query> Create(EcsContext& context, QuerySpec spec);
        [[nodiscard]] const QuerySpec& Spec() const noexcept;
        [[nodiscard]] uint64_t FilterIdentity() const noexcept;
        // Read on the Context owner thread. Counters accumulate across executions,
        // including executions that fail; worker batches do not mutate them.
        [[nodiscard]] QueryStats Stats() const noexcept;

        template<typename Callback>
        [[nodiscard]] Result<void> Batches(World& world, Callback&& callback, ChangedState* changed = nullptr) {
            return Run(world, [&](QueryBatch& batch) -> Result<void> { return std::invoke(callback, batch); }, changed);
        }
        template<typename Callback>
        [[nodiscard]] Result<void> Each(World& world, Callback&& callback, ChangedState* changed = nullptr) {
            return Run(world, [&](QueryBatch& batch) -> Result<void> {
                for (size_t row = 0; row < batch.Size(); ++row) {
                    if (!batch.Valid()) return Error{ErrorCode::InvalidState, "Query", "A borrowed World was destroyed"};
                    QueryRow current(batch, row);
                    auto result = std::invoke(callback, current);
                    if (!batch.Valid()) return Error{ErrorCode::InvalidState, "Query", "A borrowed World was destroyed"};
                    if (!result) return result;
                }
                return {};
            }, changed);
        }

    private:
        friend class Detail::QuerySubmission;
        friend class Detail::PreparedExecution;
        struct Impl;
        explicit Query(std::shared_ptr<Impl> implementation);
        [[nodiscard]] Result<void> Run(World& world,
            const std::function<Result<void>(QueryBatch&)>& callback, ChangedState* changed);
        [[nodiscard]] static Result<void> RunShared(std::shared_ptr<Impl> implementation, World& world,
            const std::function<Result<void>(QueryBatch&)>& callback,
            std::shared_ptr<ChangedState::Impl> changed, bool scheduled, std::span<const WorldBorrow> capturedBorrows);
        std::shared_ptr<Impl> m_Impl;
    };
}
