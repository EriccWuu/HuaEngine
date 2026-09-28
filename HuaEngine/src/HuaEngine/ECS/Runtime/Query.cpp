#include "HuaEngine/ECS/Runtime/QuerySubmission.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>

namespace HE::Ecs {
    namespace {
        constexpr size_t Missing = std::numeric_limits<size_t>::max();

        Error QueryError(ErrorCode code, std::string message) {
            return {code, "Query", std::move(message)};
        }
        bool Allows(AccessMode declared, bool write) noexcept { return !write || declared == AccessMode::Write; }
        bool ValidAccess(AccessMode access) noexcept { return access == AccessMode::Read || access == AccessMode::Write; }
        template<typename T> void Unique(std::vector<T>& values) {
            std::sort(values.begin(), values.end());
            values.erase(std::unique(values.begin(), values.end()), values.end());
        }
        bool Contains(std::span<const TypeId> values, TypeId value) {
            return std::binary_search(values.begin(), values.end(), value);
        }
        size_t FindType(std::span<const TypeId> values, TypeId value) {
            const auto found = std::lower_bound(values.begin(), values.end(), value);
            return found != values.end() && *found == value ? static_cast<size_t>(found - values.begin()) : Missing;
        }
        size_t FindMask(const ChunkInfo& chunk, TypeId type) {
            for (size_t index = 0; index < chunk.ComponentEnabled.size(); ++index) {
                if (chunk.ComponentEnabled[index].Type == type) return index;
            }
            return Missing;
        }
        uint64_t AllocateConsumerId() noexcept {
            static std::atomic<uint64_t> next{1};
            auto current = next.load(std::memory_order_relaxed);
            while (current != std::numeric_limits<uint64_t>::max()) {
                if (next.compare_exchange_weak(current, current + 1, std::memory_order_relaxed)) return current;
            }
            return 0;
        }

        struct FilterKey {
            std::vector<TypeId> Required;
            std::vector<TypeId> Exclude;
            std::vector<SharedBinding> Shared;
            std::vector<TypeId> Changed;
            bool IncludeDisabled = false;
            bool IgnoreComponents = false;
            bool operator<(const FilterKey& other) const {
                return std::tie(Required, Exclude, Shared, Changed, IncludeDisabled, IgnoreComponents) <
                    std::tie(other.Required, other.Exclude, other.Shared, other.Changed, other.IncludeDisabled, other.IgnoreComponents);
            }
        };
        struct RandomDeclaration {
            std::weak_ptr<const Detail::WorldLifetime> Lifetime;
            WorldId World = 0;
            TypeId Type = InvalidTypeId;
            const void* NativeKey = nullptr;
            AccessMode Access = AccessMode::Read;
        };
        struct ObservationKey {
            uint64_t Filter = 0;
            WorldId World = 0;
            ChunkHandle Chunk;
            TypeId Type = InvalidTypeId;
            auto operator<=>(const ObservationKey&) const = default;
        };
        struct Observation {
            std::weak_ptr<const Detail::WorldLifetime> Lifetime;
            uint64_t Version = 0;
            uint64_t MaskVersion = 0;
        };
        struct LayoutBinding {
            std::vector<size_t> Columns;
            std::vector<size_t> Changed;
            std::vector<size_t> Writes;
        };
        struct WorldCache {
            using LayoutKey = std::pair<uint64_t, LayoutId>;
            std::weak_ptr<const Detail::WorldLifetime> Lifetime;
            uint64_t Revision = 0;
            std::map<GroupHandle, LayoutKey> Groups;
            std::map<LayoutKey, LayoutBinding> Layouts;
        };
        struct RowMatcher {
            const ChunkInfo& Chunk;
            bool IncludeDisabled;
            bool IgnoreComponents;
            std::vector<size_t> RequiredMasks;
            std::vector<size_t> ExcludedMasks;

            RowMatcher(const ChunkInfo& chunk, const FilterKey& filter)
                : Chunk(chunk), IncludeDisabled(filter.IncludeDisabled), IgnoreComponents(filter.IgnoreComponents) {
                if (IgnoreComponents) return;
                RequiredMasks.reserve(filter.Required.size());
                ExcludedMasks.reserve(filter.Exclude.size());
                for (TypeId type : filter.Required) RequiredMasks.push_back(FindMask(Chunk, type));
                for (TypeId type : filter.Exclude) ExcludedMasks.push_back(FindMask(Chunk, type));
            }
            bool Matches(size_t row) const noexcept {
                if (!IncludeDisabled && !Chunk.Enabled(row)) return false;
                if (IgnoreComponents) return true;
                for (size_t mask : RequiredMasks) {
                    if (mask == Missing || !Chunk.ComponentEnabled[mask].Enabled(row)) return false;
                }
                for (size_t mask : ExcludedMasks) {
                    if (mask != Missing && Chunk.ComponentEnabled[mask].Enabled(row)) return false;
                }
                return true;
            }
            bool Any() const noexcept {
                for (size_t row = 0; row < Chunk.Entities.size(); ++row) if (Matches(row)) return true;
                return false;
            }
        };
    }

    namespace Detail {
        struct QueryCacheState {
            uint64_t NextFilter = 1;
            bool Executing = false;
            std::map<FilterKey, uint64_t> Filters;
        };

        struct QueryExecution {
            struct ResourceSnapshot {
                ResourceHandle Handle;
                const void* NativeKey = nullptr;
                std::shared_ptr<void> Object;
                AccessMode Access = AccessMode::Read;
            };
            struct WriteJournal {
                std::weak_ptr<const WorldLifetime> Lifetime;
                std::vector<ChunkColumnKey> Columns;
            };
            EcsContext* Context = nullptr;
            std::atomic<bool> Active{true};
            bool SnapshotMode = false;
            std::vector<std::weak_ptr<const WorldLifetime>> BorrowedWorlds;
            std::map<WorldId, const WorldBorrow*> Borrows;
            std::vector<RandomDeclaration> Random;
            std::vector<ResourceAccess> Resources;
            std::vector<ResourceSnapshot> ResourceSnapshots;
            const ResourceRegistry* ResourceOwner = nullptr;
            std::map<WorldId, WriteJournal> Writes;

            Result<void> AddWrite(World& world, ChunkColumnKey key) {
                return AddWrite(world.Id(), world.Lifetime(), key);
            }
            Result<void> AddWrite(WorldId id, std::weak_ptr<const WorldLifetime> lifetime, ChunkColumnKey key) {
                try {
                    auto& journal = Writes[id];
                    journal.Lifetime = std::move(lifetime);
                    if (std::find(journal.Columns.begin(), journal.Columns.end(), key) == journal.Columns.end()) {
                        journal.Columns.push_back(key);
                    }
                    return {};
                }
                catch (const std::exception& exception) { return QueryError(ErrorCode::ConstructionFailed, exception.what()); }
            }
            Result<void> Publish() {
                std::optional<Error> firstError;
                for (auto& [id, journal] : Writes) {
                    if (journal.Columns.empty()) continue;
                    if (SnapshotMode) {
                        const auto borrow = Borrows.find(id);
                        Result<void> result = borrow != Borrows.end() ? borrow->second->PublishWrites(journal.Columns) :
                            Result<void>(QueryError(ErrorCode::InvalidState, "The write lease is missing"));
                        if (!result && !firstError) firstError = result.GetError();
                        journal.Columns.clear();
                        continue;
                    }
                    const auto lifetime = journal.Lifetime.lock();
                    auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
                    if (!world || lifetime->Id != id) {
                        if (!firstError) firstError = QueryError(ErrorCode::InvalidState, "A written World no longer exists");
                        journal.Columns.clear();
                        continue;
                    }
                    auto result = world->PublishWrites(journal.Columns);
                    if (!result && !firstError) firstError = result.GetError();
                    journal.Columns.clear();
                }
                if (firstError) return *firstError;
                return {};
            }
        };

        struct QueryBatchData {
            struct ColumnBinding {
                const RegisteredType* Type = nullptr;
                size_t Column = Missing;
                size_t Mask = Missing;
                AccessMode Access = AccessMode::Read;
            };
            std::shared_ptr<QueryExecution> Execution;
            std::weak_ptr<const WorldLifetime> Lifetime;
            const WorldBorrow* MainBorrow = nullptr;
            uint64_t StructureVersion = 0;
            std::atomic<bool> Active{false};
            bool IgnoreEnabled = false;
            ChunkInfo Chunk;
            std::vector<size_t> Rows;
            std::vector<ColumnBinding> Bindings;
            std::vector<size_t> Changed;
            std::vector<size_t> Writes;
        };

        bool BatchValid(const QueryBatchData& batch) noexcept {
            if (!batch.Active || !batch.Execution->Active) return false;
            if (batch.Execution->SnapshotMode) {
                for (const auto& entry : batch.Execution->Borrows) {
                    if (!entry.second->Valid()) return false;
                }
                return batch.MainBorrow && batch.MainBorrow->StructureVersion() == batch.StructureVersion;
            }
            for (const auto& borrowed : batch.Execution->BorrowedWorlds) {
                const auto lifetime = borrowed.lock();
                if (!lifetime || !lifetime->Owner.load(std::memory_order_acquire)) return false;
            }
            const auto lifetime = batch.Lifetime.lock();
            const auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
            return world && world->StructureVersion() == batch.StructureVersion;
        }
        bool BatchValid(const std::weak_ptr<QueryBatchData>& weak) noexcept {
            const auto batch = weak.lock();
            return batch && BatchValid(*batch);
        }
        size_t BatchSize(const std::weak_ptr<QueryBatchData>& weak) noexcept {
            const auto batch = weak.lock();
            return batch && BatchValid(*batch) ? batch->Rows.size() : 0;
        }
        Result<void> ValidateBinding(const QueryBatchData& batch,
            size_t argument, const void* nativeKey, bool write) {
            if (!BatchValid(batch)) return QueryError(ErrorCode::InvalidState, "The query batch has expired");
            if (argument >= batch.Bindings.size()) return QueryError(ErrorCode::InvalidArgument, "The column argument is out of range");
            const auto& binding = batch.Bindings[argument];
            if (binding.Type->Descriptor.NativeKey != nativeKey) return QueryError(ErrorCode::InvalidType, "The column argument has a different native type");
            if (!Allows(binding.Access, write)) return QueryError(ErrorCode::UnsupportedOperation, "Writing this column was not declared");
            return {};
        }
        Result<void> ValidateColumn(const std::weak_ptr<QueryBatchData>& weak,
            size_t argument, const void* nativeKey, bool write) {
            const auto batch = weak.lock();
            if (!batch) return QueryError(ErrorCode::InvalidState, "The query batch has expired");
            return ValidateBinding(*batch, argument, nativeKey, write);
        }
        Result<void*> ColumnPointer(const std::weak_ptr<QueryBatchData>& weak,
            size_t argument, size_t row, const void* nativeKey, bool write) {
            const auto batch = weak.lock();
            if (!batch) return QueryError(ErrorCode::InvalidState, "The query batch has expired");
            auto access = ValidateBinding(*batch, argument, nativeKey, write);
            if (!access) return access.GetError();
            if (row >= batch->Rows.size()) return QueryError(ErrorCode::InvalidArgument, "The logical row is out of range");
            const auto& binding = batch->Bindings[argument];
            if (binding.Column == Missing) return static_cast<void*>(nullptr);
            const size_t physical = batch->Rows[row];
            if (!batch->IgnoreEnabled && binding.Mask != Missing && !batch->Chunk.ComponentEnabled[binding.Mask].Enabled(physical)) {
                return static_cast<void*>(nullptr);
            }
            return const_cast<void*>(batch->Chunk.Columns[binding.Column].At(physical));
        }
        std::optional<ColumnSpan> ContiguousColumn(const std::weak_ptr<QueryBatchData>& weak,
            size_t argument, const void* nativeKey, bool write) {
            const auto batch = weak.lock();
            if (!batch || !ValidateBinding(*batch, argument, nativeKey, write)) return std::nullopt;
            const auto& binding = batch->Bindings[argument];
            if (binding.Column == Missing || binding.Type->Descriptor.Storage != StorageKind::Direct) return std::nullopt;
            if (batch->Rows.empty()) return ColumnSpan{};
            const size_t first = batch->Rows.front();
            for (size_t row = 0; row < batch->Rows.size(); ++row) {
                if (batch->Rows[row] != first + row) return std::nullopt;
                if (!batch->IgnoreEnabled && binding.Mask != Missing && !batch->Chunk.ComponentEnabled[binding.Mask].Enabled(first + row)) {
                    return std::nullopt;
                }
            }
            return ColumnSpan{const_cast<void*>(batch->Chunk.Columns[binding.Column].At(first)), batch->Rows.size()};
        }

        Result<RandomBinding> BindRandom(const std::weak_ptr<QueryBatchData>& weak,
            World& world, const void* nativeKey, bool write) {
            if (!BatchValid(weak)) return QueryError(ErrorCode::InvalidState, "The query batch has expired");
            const auto batch = weak.lock();
            for (const auto& declaration : batch->Execution->Random) {
                if (declaration.NativeKey != nativeKey) continue;
                const auto lifetime = declaration.Lifetime.lock();
                if (!lifetime || lifetime->Owner.load(std::memory_order_acquire) != &world) continue;
                if (!Allows(declaration.Access, write)) return QueryError(ErrorCode::UnsupportedOperation, "Random write access was not declared");
                return RandomBinding{batch->Execution, declaration.World, declaration.Type};
            }
            return QueryError(ErrorCode::UnsupportedOperation, "Random access to this World and native type was not declared");
        }
        Result<RandomBinding> BindRandom(const std::weak_ptr<QueryBatchData>& weak,
            WorldId world, const void* nativeKey, bool write) {
            if (!BatchValid(weak)) return QueryError(ErrorCode::InvalidState, "The query batch has expired");
            const auto batch = weak.lock();
            for (const auto& declaration : batch->Execution->Random) {
                if (declaration.World != world || declaration.NativeKey != nativeKey) continue;
                const auto lifetime = declaration.Lifetime.lock();
                if (!lifetime || !lifetime->Owner.load(std::memory_order_acquire)) {
                    return QueryError(ErrorCode::InvalidState, "The random-access World has expired");
                }
                if (!Allows(declaration.Access, write)) {
                    return QueryError(ErrorCode::UnsupportedOperation, "Random write access was not declared");
                }
                return RandomBinding{batch->Execution, declaration.World, declaration.Type};
            }
            return QueryError(ErrorCode::UnsupportedOperation, "Random access to this World and native type was not declared");
        }
        Result<void*> RandomPointer(const RandomBinding& binding, EntityId entity,
            const void* nativeKey, bool write) {
            const auto execution = binding.Execution.lock();
            if (!execution || !execution->Active) return QueryError(ErrorCode::InvalidState, "The random-access view has expired");
            for (const auto& borrowed : execution->BorrowedWorlds) {
                const auto lifetime = borrowed.lock();
                if (!lifetime || !lifetime->Owner.load(std::memory_order_acquire)) {
                    return QueryError(ErrorCode::InvalidState, "A borrowed World was destroyed");
                }
            }
            for (const auto& declaration : execution->Random) {
                if (declaration.World != binding.World || declaration.Type != binding.Type || declaration.NativeKey != nativeKey) continue;
                if (!Allows(declaration.Access, write)) return QueryError(ErrorCode::UnsupportedOperation, "Random write access was not declared");
                const auto lifetime = declaration.Lifetime.lock();
                if (execution->SnapshotMode) {
                    const auto borrow = execution->Borrows.find(declaration.World);
                    if (borrow == execution->Borrows.end()) return QueryError(ErrorCode::InvalidState, "The random-access lease is missing");
                    auto component = borrow->second->Lookup(entity, declaration.Type);
                    if (!component) {
                        // TryGet preserves the synchronous API's null result for absence.
                        if (component.GetError().Code == ErrorCode::InvalidEntity || component.GetError().Code == ErrorCode::InvalidType) {
                            return static_cast<void*>(nullptr);
                        }
                        return component.GetError();
                    }
                    if (write && component.Value().Data) {
                        auto marked = execution->AddWrite(declaration.World, declaration.Lifetime, component.Value().Column);
                        if (!marked) return marked.GetError();
                    }
                    return const_cast<void*>(component.Value().Data);
                }
                auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
                if (!world) return QueryError(ErrorCode::InvalidState, "The random-access World has expired");
                const void* pointer = std::as_const(*world).TryGet(entity, declaration.Type);
                if (write && pointer) {
                    const auto location = world->Location(entity);
                    auto marked = execution->AddWrite(*world, {{location.Chunk, location.ChunkGeneration}, declaration.Type});
                    if (!marked) return marked.GetError();
                }
                return const_cast<void*>(pointer);
            }
            return QueryError(ErrorCode::UnsupportedOperation, "The random-access declaration is missing");
        }
        Result<void*> ResourcePointer(const std::weak_ptr<QueryBatchData>& weak,
            ResourceHandle resource, const void* nativeKey, bool write) {
            if (!BatchValid(weak)) return QueryError(ErrorCode::InvalidState, "The query batch has expired");
            const auto execution = weak.lock()->Execution;
            if (execution->SnapshotMode) {
                if (resource.Owner != execution->ResourceOwner) return QueryError(ErrorCode::InvalidType, "The resource belongs to another Context");
                for (const auto& snapshot : execution->ResourceSnapshots) {
                    if (snapshot.Handle != resource) continue;
                    if (snapshot.NativeKey != nativeKey) return QueryError(ErrorCode::InvalidType, "The resource has a different native type");
                    if (!Allows(snapshot.Access, write)) return QueryError(ErrorCode::UnsupportedOperation, "Resource write access was not declared");
                    return snapshot.Object.get();
                }
                return QueryError(ErrorCode::UnsupportedOperation, "Access to this resource was not declared");
            }
            const auto* registered = execution->Context->Resources().Find(resource);
            if (!registered || registered->NativeKey != nativeKey) return QueryError(ErrorCode::InvalidType, "The resource belongs to another Context or native type");
            for (const auto& declaration : execution->Resources) {
                if (declaration.Resource != resource) continue;
                if (!Allows(declaration.Access, write)) return QueryError(ErrorCode::UnsupportedOperation, "Resource write access was not declared");
                return registered->Object.get();
            }
            return QueryError(ErrorCode::UnsupportedOperation, "Access to this resource was not declared");
        }
    }

    struct ChangedState::Impl {
        uint64_t Identity = AllocateConsumerId();
        mutable std::mutex Mutex;
        bool Running = false;
        size_t PendingUsers = 0;
        const EcsContext* PendingContext = nullptr;
        std::map<ObservationKey, Observation> Observations;
    };
    ChangedState::ChangedState() : m_Impl(std::make_shared<Impl>()) {}
    ChangedState::~ChangedState() = default;
    uint64_t ChangedState::ConsumerId() const noexcept { return m_Impl->Identity; }
    size_t ChangedState::ObservationCount() const noexcept { std::lock_guard lock(m_Impl->Mutex); return m_Impl->Observations.size(); }
    Result<void> ChangedState::Reset() {
        std::lock_guard lock(m_Impl->Mutex);
        if (m_Impl->Running || m_Impl->PendingUsers != 0) return QueryError(ErrorCode::Busy, "The Changed observer has queued or running work");
        m_Impl->Observations.clear();
        return {};
    }

    struct Query::Impl {
        EcsContext* Context = nullptr;
        QuerySpec Description;
        FilterKey Filter;
        uint64_t FilterId = 0;
        QueryStats Counters;
        bool Running = false;
        // Capture runs on the Context owner thread; completion can release on a worker.
        std::atomic<size_t> PendingUsers{0};
        std::shared_ptr<Detail::QueryCacheState> CacheState;
        std::vector<RandomDeclaration> Random;
        std::map<WorldId, WorldCache> Caches;

        bool Matches(const GroupInfo& group) const {
            for (TypeId type : Filter.Required) if (!Contains(group.Types, type) && !Contains(group.Tags, type)) return false;
            if (Filter.IgnoreComponents) {
                for (TypeId type : Filter.Exclude) if (Contains(group.Types, type) || Contains(group.Tags, type)) return false;
            }
            for (const auto& binding : Filter.Shared) {
                if (std::find(group.Shared.begin(), group.Shared.end(), binding) == group.Shared.end()) return false;
            }
            return true;
        }
        void BindLayout(WorldCache& cache, const GroupInfo& group) {
            const WorldCache::LayoutKey key{group.Handle.Generation, group.Layout};
            if (cache.Layouts.contains(key)) { ++Counters.CacheHits; return; }
            LayoutBinding binding;
            for (const auto& term : Description.Columns) {
                const size_t column = FindType(group.Types, term.Type);
                binding.Columns.push_back(column);
                if (term.Access == AccessMode::Write && column != Missing) binding.Writes.push_back(column);
            }
            for (TypeId type : Filter.Changed) binding.Changed.push_back(FindType(group.Types, type));
            Unique(binding.Writes);
            cache.Layouts.emplace(key, std::move(binding));
            ++Counters.LayoutBindCount;
        }
        WorldCache& Refresh(World& world) {
            std::erase_if(Caches, [](const auto& entry) {
                const auto lifetime = entry.second.Lifetime.lock();
                return !lifetime || !lifetime->Owner.load(std::memory_order_acquire);
            });
            auto& cache = Caches[world.Id()];
            cache.Lifetime = world.Lifetime();
            if (cache.Revision == world.GroupRevision()) { ++Counters.CacheHits; return cache; }
            auto changes = world.ChangesSince(cache.Revision);
            if (changes.Reset) {
                cache.Groups.clear();
                cache.Layouts.clear();
            }
            for (auto removed : changes.Removed) cache.Groups.erase(removed);
            for (const auto& group : changes.Added) {
                ++Counters.GroupMatchCount;
                if (Matches(group)) {
                    BindLayout(cache, group);
                    cache.Groups[group.Handle] = {group.Handle.Generation, group.Layout};
                }
                else cache.Groups.erase(group.Handle);
            }
            std::set<WorldCache::LayoutKey> liveLayouts;
            for (const auto& [handle, layout] : cache.Groups) liveLayouts.insert(layout);
            std::erase_if(cache.Layouts, [&](const auto& layout) { return !liveLayouts.contains(layout.first); });
            cache.Revision = changes.Revision;
            return cache;
        }
    };

    Query::Query(std::shared_ptr<Impl> implementation) : m_Impl(std::move(implementation)) {}
    Query::~Query() = default;
    Query::Query(Query&&) noexcept = default;
    Query& Query::operator=(Query&&) noexcept = default;
    const QuerySpec& Query::Spec() const noexcept {
        static const QuerySpec empty;
        return m_Impl ? m_Impl->Description : empty;
    }
    uint64_t Query::FilterIdentity() const noexcept { return m_Impl ? m_Impl->FilterId : 0; }
    QueryStats Query::Stats() const noexcept { return m_Impl ? m_Impl->Counters : QueryStats{}; }

    Result<Query> Query::Create(EcsContext& context, QuerySpec spec) {
        if (!context.IsMainThread()) return QueryError(ErrorCode::WrongThread, "Query creation requires the Context owner thread");
        try {
            auto implementation = std::make_shared<Impl>();
            implementation->Context = &context;
            for (size_t argument = 0; argument < spec.Columns.size(); ++argument) {
                const auto& term = spec.Columns[argument];
                const auto* type = context.Types().Find(term.Type);
                if (!type || type->Descriptor.Storage == StorageKind::Tag) {
                    return QueryError(ErrorCode::InvalidType, "Column " + std::to_string(argument) + " must name a registered stored component");
                }
                if (!ValidAccess(term.Access) || (term.Match != Presence::Required && term.Match != Presence::Optional)) {
                    return QueryError(ErrorCode::InvalidArgument, "Column " + std::to_string(argument) + " has an invalid presence or access mode");
                }
                if (term.Match == Presence::Required) spec.Required.push_back(term.Type);
            }
            for (TypeId type : spec.ChangedTypes) {
                const auto* registered = context.Types().Find(type);
                if (!registered || registered->Descriptor.Storage == StorageKind::Tag) {
                    return QueryError(ErrorCode::InvalidType, "Changed requires a registered stored component");
                }
                spec.Required.push_back(type);
            }
            Unique(spec.Required); Unique(spec.Exclude); Unique(spec.ChangedTypes);
            for (TypeId type : spec.Required) {
                if (!context.Types().Find(type)) return QueryError(ErrorCode::InvalidType, "Required names an unregistered type");
                if (Contains(spec.Exclude, type)) return QueryError(ErrorCode::InvalidArgument, "A type cannot be both Required and Excluded");
            }
            for (TypeId type : spec.Exclude) {
                if (!context.Types().Find(type)) return QueryError(ErrorCode::InvalidType, "Exclude names an unregistered type");
            }
            Unique(spec.SharedBindings);
            TypeId lastShared = InvalidTypeId;
            for (const auto& binding : spec.SharedBindings) {
                const auto* type = context.Types().Find(binding.Type);
                const auto* resource = context.Resources().Find(binding.Object);
                if (!type || !resource || type->Descriptor.Storage == StorageKind::Tag || type->Descriptor.NativeKey != resource->NativeKey) {
                    return QueryError(ErrorCode::InvalidType, "A shared binding must match the Context and native component type");
                }
                if (binding.Type == lastShared) return QueryError(ErrorCode::InvalidArgument, "A shared type cannot require two object identities");
                lastShared = binding.Type;
            }
            for (auto& random : spec.RandomAccesses) {
                if (!random.Target || &random.Target->Context() != &context || !ValidAccess(random.Access)) {
                    return QueryError(ErrorCode::InvalidArgument, "Random access requires a target World in this Context and a valid access mode");
                }
                const auto* type = random.Target->Types().Find(random.Type);
                if (!type || type->Descriptor.Storage == StorageKind::Tag) return QueryError(ErrorCode::InvalidType, "Random access requires a stored component type");
                random.TargetId = random.Target->Id();
                random.Lifetime = random.Target->Lifetime();
                const auto existing = std::find_if(implementation->Random.begin(), implementation->Random.end(), [&](const auto& entry) {
                    return entry.World == random.Target->Id() && entry.Type == random.Type;
                });
                if (existing != implementation->Random.end()) {
                    if (random.Access == AccessMode::Write) existing->Access = AccessMode::Write;
                }
                else implementation->Random.push_back({random.Target->Lifetime(), random.Target->Id(), random.Type, type->Descriptor.NativeKey, random.Access});
            }
            std::vector<ResourceAccess> resources;
            for (const auto& access : spec.ResourceAccesses) {
                if (!context.Resources().Find(access.Resource) || !ValidAccess(access.Access)) {
                    return QueryError(ErrorCode::InvalidType, "Resource access requires a registered resource in this Context and a valid access mode");
                }
                const auto existing = std::find_if(resources.begin(), resources.end(), [&](const auto& entry) { return entry.Resource == access.Resource; });
                if (existing == resources.end()) resources.push_back(access);
                else if (access.Access == AccessMode::Write) existing->Access = AccessMode::Write;
            }
            spec.ResourceAccesses = std::move(resources);
            implementation->Filter = {spec.Required, spec.Exclude, spec.SharedBindings, spec.ChangedTypes,
                spec.IncludeDisabledEntities, spec.IgnoreComponentEnabled};
            if (!context.m_QueryCache) context.m_QueryCache = std::make_shared<Detail::QueryCacheState>();
            implementation->CacheState = context.m_QueryCache;
            auto& pool = *context.m_QueryCache;
            auto identity = pool.Filters.find(implementation->Filter);
            if (identity == pool.Filters.end()) {
                if (pool.NextFilter == std::numeric_limits<uint64_t>::max()) return QueryError(ErrorCode::InvalidState, "Query filter identity space is exhausted");
                identity = pool.Filters.emplace(implementation->Filter, pool.NextFilter).first;
                ++pool.NextFilter;
            }
            implementation->FilterId = identity->second;
            implementation->Description = std::move(spec);
            return Query(std::move(implementation));
        }
        catch (const std::exception& exception) { return QueryError(ErrorCode::ConstructionFailed, exception.what()); }
    }

    namespace Detail {
        struct QuerySubmission::Impl {
            std::shared_ptr<Query::Impl> QueryState;
            std::shared_ptr<ChangedState::Impl> Changed;
            EcsContext* OwnerContext = nullptr;
            WorldId MainWorld = 0;
            std::weak_ptr<const WorldLifetime> Main;
            std::vector<WorldBorrow> Borrows;
            std::map<WorldId, std::weak_ptr<const WorldLifetime>> Lifetimes;
            std::vector<LocalAccess> Local;
            std::vector<SubmittedRandomAccess> Random;
            std::vector<ResourceAccess> Resources;
            uint64_t Consumer = 0;
            bool Reserved = false;
            bool Executed = false;

            ~Impl() { ReleaseReservations(); }
            void ReleaseReservations() noexcept {
                if (Reserved) {
                    QueryState->PendingUsers.fetch_sub(1, std::memory_order_acq_rel);
                    if (Changed) {
                        std::lock_guard lock(Changed->Mutex);
                        if (--Changed->PendingUsers == 0) Changed->PendingContext = nullptr;
                    }
                    Reserved = false;
                }
                Borrows.clear();
            }
        };

        QuerySubmission::QuerySubmission() = default;
        QuerySubmission::QuerySubmission(std::shared_ptr<Impl> implementation) : m_Impl(std::move(implementation)) {}
        QuerySubmission::~QuerySubmission() = default;
        QuerySubmission::QuerySubmission(QuerySubmission&&) noexcept = default;
        QuerySubmission& QuerySubmission::operator=(QuerySubmission&&) noexcept = default;

        Result<QuerySubmission> QuerySubmission::Capture(Query& query, World& world, ChangedState* changed) {
            const auto queryState = query.m_Impl;
            const auto observer = changed ? changed->m_Impl : nullptr;
            if (!queryState) return QueryError(ErrorCode::InvalidState, "The Query was moved from");
            if (!queryState->Context->IsMainThread()) return QueryError(ErrorCode::WrongThread, "Query capture requires the Context owner thread");
            if (&world.Context() != queryState->Context) return QueryError(ErrorCode::InvalidArgument, "The World belongs to another Context");
            if (queryState->Running || queryState->CacheState->Executing) {
                return QueryError(ErrorCode::Busy, "Query capture cannot reenter an executing query or observer");
            }
            if (observer) {
                std::lock_guard lock(observer->Mutex);
                if (observer->Running && observer->PendingUsers == 0) return QueryError(ErrorCode::Busy, "The Changed observer is executing synchronously");
                if (observer->PendingUsers != 0 && observer->PendingContext != queryState->Context) {
                    return QueryError(ErrorCode::Busy, "The Changed observer has queued work in another Context");
                }
            }
            if (!queryState->Filter.Changed.empty() && (!observer || observer->Identity == 0)) {
                return QueryError(ErrorCode::InvalidArgument, "Changed queries require a valid consumer state");
            }
            if (queryState->PendingUsers.load(std::memory_order_acquire) == std::numeric_limits<size_t>::max()) {
                return QueryError(ErrorCode::InvalidState, "The query reservation count is exhausted");
            }
            try {
                auto captured = std::make_shared<Impl>();
                captured->QueryState = queryState;
                captured->Changed = observer;
                captured->OwnerContext = queryState->Context;
                captured->MainWorld = world.Id();
                captured->Main = world.Lifetime();
                captured->Consumer = observer ? observer->Identity : 0;
                captured->Resources = queryState->Description.ResourceAccesses;

                std::map<WorldId, World*> targets{{world.Id(), &world}};
                for (const auto& declaration : queryState->Random) {
                    const auto lifetime = declaration.Lifetime.lock();
                    auto* target = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
                    if (!target || lifetime->Id != declaration.World) {
                        return QueryError(ErrorCode::InvalidState, "A declared random-access World was destroyed");
                    }
                    targets.emplace(declaration.World, target);
                    captured->Random.push_back({declaration.Lifetime, declaration.World, declaration.Type, declaration.Access});
                }
                std::sort(captured->Random.begin(), captured->Random.end(), [](const auto& left, const auto& right) {
                    return std::tie(left.World, left.Type) < std::tie(right.World, right.Type);
                });
                captured->Borrows.reserve(targets.size());
                for (const auto& [id, target] : targets) {
                    auto borrow = target->AcquireBorrow();
                    if (!borrow) return borrow.GetError();
                    captured->Borrows.push_back(std::move(borrow).Value());
                    captured->Lifetimes.emplace(id, target->Lifetime());
                }

                auto& cache = queryState->Refresh(world);
                std::map<std::pair<ChunkHandle, TypeId>, AccessMode> local;
                const auto addLocal = [&](ChunkHandle chunk, TypeId type, AccessMode access) {
                    auto [entry, inserted] = local.emplace(std::make_pair(chunk, type), access);
                    if (!inserted && access == AccessMode::Write) entry->second = AccessMode::Write;
                };
                for (const auto& [group, layout] : cache.Groups) {
                    auto chunks = world.GetGroupChunks(group);
                    if (!chunks) return chunks.GetError();
                    const auto& binding = cache.Layouts.at(layout);
                    for (const auto& chunk : chunks.Value()) {
                        const RowMatcher rows(chunk, queryState->Filter);
                        if (!rows.Any()) continue;
                        for (size_t argument = 0; argument < queryState->Description.Columns.size(); ++argument) {
                            if (binding.Columns[argument] == Missing) continue;
                            const auto& term = queryState->Description.Columns[argument];
                            addLocal(chunk.Handle(), term.Type, term.Access);
                        }
                        for (TypeId type : queryState->Filter.Changed) addLocal(chunk.Handle(), type, AccessMode::Read);
                    }
                }
                captured->Local.reserve(local.size());
                for (const auto& [key, access] : local) captured->Local.push_back({key.first, key.second, access});

                if (observer) {
                    std::lock_guard lock(observer->Mutex);
                    // Another Context may reserve or start this observer while capture prepares its cache.
                    if (observer->Running && observer->PendingUsers == 0) return QueryError(ErrorCode::Busy, "The Changed observer is executing synchronously");
                    if (observer->PendingUsers != 0 && observer->PendingContext != queryState->Context) {
                        return QueryError(ErrorCode::Busy, "The Changed observer has queued work in another Context");
                    }
                    if (observer->PendingUsers == std::numeric_limits<size_t>::max()) return QueryError(ErrorCode::InvalidState, "The observer reservation count is exhausted");
                    observer->PendingContext = queryState->Context;
                    ++observer->PendingUsers;
                }
                queryState->PendingUsers.fetch_add(1, std::memory_order_acq_rel);
                captured->Reserved = true;
                return QuerySubmission(std::move(captured));
            }
            catch (const std::exception& exception) { return QueryError(ErrorCode::ConstructionFailed, exception.what()); }
        }

        EcsContext* QuerySubmission::Context() const noexcept { return m_Impl ? m_Impl->OwnerContext : nullptr; }
        WorldId QuerySubmission::MainId() const noexcept { return m_Impl ? m_Impl->MainWorld : 0; }
        std::weak_ptr<const WorldLifetime> QuerySubmission::MainLifetime() const noexcept {
            return m_Impl ? m_Impl->Main : std::weak_ptr<const WorldLifetime>{};
        }
        std::span<const LocalAccess> QuerySubmission::LocalAccesses() const noexcept {
            return m_Impl ? std::span<const LocalAccess>(m_Impl->Local) : std::span<const LocalAccess>{};
        }
        std::span<const SubmittedRandomAccess> QuerySubmission::RandomAccesses() const noexcept {
            return m_Impl ? std::span<const SubmittedRandomAccess>(m_Impl->Random) : std::span<const SubmittedRandomAccess>{};
        }
        std::span<const ResourceAccess> QuerySubmission::ResourceAccesses() const noexcept {
            return m_Impl ? std::span<const ResourceAccess>(m_Impl->Resources) : std::span<const ResourceAccess>{};
        }
        uint64_t QuerySubmission::ConsumerId() const noexcept { return m_Impl ? m_Impl->Consumer : 0; }

        Result<void> QuerySubmission::Execute(const std::function<Result<void>(QueryBatch&)>& callback) {
            const auto state = m_Impl;
            if (!state || state->Executed || !state->Reserved) {
                return QueryError(ErrorCode::InvalidState, "The query submission was released or already executed");
            }
            if (!state->OwnerContext->IsMainThread()) return QueryError(ErrorCode::WrongThread, "Serial submission execution requires the Context owner thread");
            state->Executed = true;
            struct Finish {
                Impl& State;
                ~Finish() { State.ReleaseReservations(); }
            } finish{*state};
            if (std::any_of(state->Borrows.begin(), state->Borrows.end(), [](const auto& borrow) { return !borrow.Valid(); })) {
                return QueryError(ErrorCode::InvalidState, "A captured World was destroyed");
            }
            const auto lifetime = state->Main.lock();
            auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
            if (!world || lifetime->Id != state->MainWorld) return QueryError(ErrorCode::InvalidState, "The captured main World was destroyed");
            if (!callback) return QueryError(ErrorCode::InvalidArgument, "The query callback is empty");
            return Query::RunShared(state->QueryState, *world, callback, state->Changed, true, state->Borrows);
        }

        void QuerySubmission::Release() noexcept { m_Impl.reset(); }

        struct PreparedExecution::Impl {
            enum class BatchStage { Ready, Running, Done, Skipped, Cancelled };
            struct BatchSlot {
                std::shared_ptr<QueryBatchData> Data;
                std::atomic<BatchStage> Stage{BatchStage::Ready};
                bool Failed = false;
            };
            struct ChunkState {
                ChunkInfo Snapshot;
                std::vector<size_t> ChangedColumns;
                bool Begun = false;
                bool ShouldRun = false;
                bool Complete = false;
            };
            std::shared_ptr<QuerySubmission::Impl> Capture;
            std::mutex Mutex;
            std::vector<PreparedChunk> ChunkPlans;
            std::vector<PreparedBatch> BatchPlans;
            std::vector<ChunkState> ChunkStates;
            std::vector<std::unique_ptr<BatchSlot>> Slots;
            std::map<WorldId, const WorldBorrow*> Borrows;
            std::set<ChunkHandle> MatchingChunks;
            std::set<ChunkHandle> InvalidObservedChunks;
            std::map<ObservationKey, Observation> Baseline;
            std::map<ObservationKey, Observation> Pending;
            size_t Rows = 0;
            bool Serial = false;
            bool Small = false;
            bool ObserverStarted = false;
            bool Failed = false;
            bool Finished = false;

            ~Impl() {
                if (!Finished && Capture) {
                    // Every in-flight method owns this Impl. The last owner therefore
                    // reaches this fallback only after all running callbacks have left.
                    for (size_t index = 0; index < ChunkStates.size(); ++index) {
                        try { (void)PublishChunk(index); }
                        catch (...) {
                            const auto& plan = ChunkPlans[index];
                            for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                                try { (void)Slots[batch]->Data->Execution->Publish(); } catch (...) {}
                            }
                        }
                    }
                    if (ObserverStarted && Capture->Changed) {
                        std::lock_guard lock(Capture->Changed->Mutex);
                        Capture->Changed->Running = false;
                    }
                    Capture->ReleaseReservations();
                }
            }
            bool Valid() const noexcept {
                return std::all_of(Capture->Borrows.begin(), Capture->Borrows.end(), [](const auto& borrow) { return borrow.Valid(); });
            }
            Result<void> PublishChunk(size_t index) {
                const auto& plan = ChunkPlans[index];
                std::map<WorldId, std::vector<ChunkColumnKey>> writes;
                for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                    if (Slots[batch]->Stage.load(std::memory_order_acquire) == BatchStage::Running) {
                        return QueryError(ErrorCode::Busy, "A Chunk child is still running");
                    }
                    for (const auto& [id, journal] : Slots[batch]->Data->Execution->Writes) {
                        if (journal.Columns.empty()) continue;
                        auto& keys = writes[id];
                        keys.insert(keys.end(), journal.Columns.begin(), journal.Columns.end());
                    }
                }
                std::optional<Error> firstError;
                for (auto& [id, keys] : writes) {
                    Unique(keys);
                    auto published = Borrows.at(id)->PublishWrites(keys);
                    if (!published && !firstError) firstError = published.GetError();
                }
                for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                    for (auto& [id, journal] : Slots[batch]->Data->Execution->Writes) journal.Columns.clear();
                }
                if (firstError) return *firstError;
                return {};
            }
            Result<void> StartObserver() {
                if (ObserverStarted || !Capture->Changed) return {};
                auto& observer = *Capture->Changed;
                std::lock_guard lock(observer.Mutex);
                if (observer.Running) return QueryError(ErrorCode::Busy, "The Changed observer predecessor has not completed");
                Baseline = observer.Observations;
                Pending = Baseline;
                std::erase_if(Pending, [&](const auto& entry) {
                    const auto lifetime = entry.second.Lifetime.lock();
                    if (!lifetime || !lifetime->Owner.load(std::memory_order_acquire)) return true;
                    if (entry.first.World != Capture->MainWorld) return false;
                    return InvalidObservedChunks.contains(entry.first.Chunk) ||
                        (entry.first.Filter == Capture->QueryState->FilterId && !MatchingChunks.contains(entry.first.Chunk));
                });
                for (const auto& chunk : ChunkStates) for (size_t column : chunk.ChangedColumns) {
                    Pending.try_emplace({Capture->QueryState->FilterId, Capture->MainWorld, chunk.Snapshot.Handle(),
                        chunk.Snapshot.Columns[column].Type->Id}, Observation{Capture->Main, 0, 0});
                }
                observer.Running = true;
                ObserverStarted = true;
                return {};
            }
        };

        PreparedExecution::PreparedExecution() = default;
        PreparedExecution::PreparedExecution(std::shared_ptr<Impl> implementation) : m_Impl(std::move(implementation)) {}
        PreparedExecution::~PreparedExecution() = default;
        PreparedExecution::PreparedExecution(PreparedExecution&&) noexcept = default;
        PreparedExecution& PreparedExecution::operator=(PreparedExecution&&) noexcept = default;

        Result<PreparedExecution> QuerySubmission::Prepare(PreparationOptions options) {
            const auto captured = m_Impl;
            if (!captured || captured->Executed || !captured->Reserved) return QueryError(ErrorCode::InvalidState, "The submission was released or consumed");
            if (!captured->OwnerContext->IsMainThread()) return QueryError(ErrorCode::WrongThread, "Query preparation requires the Context owner thread");
            if (!options.RowsPerBatch) return QueryError(ErrorCode::InvalidArgument, "RowsPerBatch must be positive");
            // A zero small-task threshold explicitly disables merging.
            if (std::any_of(captured->Borrows.begin(), captured->Borrows.end(), [](const auto& borrow) { return !borrow.Valid(); })) {
                return QueryError(ErrorCode::InvalidState, "A captured World was destroyed");
            }
            try {
                auto prepared = std::make_shared<PreparedExecution::Impl>();
                prepared->Capture = captured;
                size_t borrowIndex = 0;
                for (const auto& [id, lifetime] : captured->Lifetimes) prepared->Borrows.emplace(id, &captured->Borrows[borrowIndex++]);
                std::vector<QueryExecution::ResourceSnapshot> resources;
                for (const auto& access : captured->Resources) {
                    const auto* entry = captured->OwnerContext->Resources().Find(access.Resource);
                    if (!entry) return QueryError(ErrorCode::InvalidType, "The declared resource is unavailable");
                    resources.push_back({access.Resource, entry->NativeKey, entry->Object, access.Access});
                    if (access.Access == AccessMode::Write) prepared->Serial = true;
                }
                for (const auto& random : captured->Random) {
                    if (random.Access == AccessMode::Write) prepared->Serial = true;
                    if (random.World == captured->MainWorld) for (const auto& local : captured->Local) {
                        if (local.Type == random.Type && local.Access == AccessMode::Write) prepared->Serial = true;
                    }
                }
                const auto lifetime = captured->Main.lock();
                auto* world = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
                if (!world) return QueryError(ErrorCode::InvalidState, "The captured main World was destroyed");
                if (captured->Changed) {
                    std::set<ChunkHandle> observedChunks;
                    {
                        std::lock_guard lock(captured->Changed->Mutex);
                        for (const auto& [key, observation] : captured->Changed->Observations) {
                            if (key.World == captured->MainWorld) observedChunks.insert(key.Chunk);
                        }
                    }
                    // Borrows prevent deletion after preparation. Capture stale handles now
                    // so workers can prune every old filter without reading the World shell.
                    for (ChunkHandle chunk : observedChunks) {
                        if (!world->InspectChunk(chunk)) prepared->InvalidObservedChunks.insert(chunk);
                    }
                }
                auto& query = *captured->QueryState;
                auto& cache = query.Refresh(*world);
                for (const auto& [group, layout] : cache.Groups) {
                    auto chunks = world->GetGroupChunks(group);
                    if (!chunks) return chunks.GetError();
                    const auto& binding = cache.Layouts.at(layout);
                    for (const auto& chunk : chunks.Value()) {
                        prepared->MatchingChunks.insert(chunk.Handle());
                        ++query.Counters.CandidateChunks;
                        const RowMatcher matcher(chunk, query.Filter);
                        std::vector<size_t> rows;
                         rows.reserve(chunk.Entities.size());
                        for (size_t row = 0; row < chunk.Entities.size(); ++row) {
                            ++query.Counters.VisitedRows;
                            if (matcher.Matches(row)) rows.push_back(row);
                        }
                        if (rows.empty()) continue;
                        const size_t chunkIndex = prepared->ChunkPlans.size();
                        PreparedChunk plan{chunk.Handle(), prepared->BatchPlans.size(), 0, rows.size(), {}};
                        for (const auto& local : captured->Local) if (local.Chunk == chunk.Handle()) plan.Accesses.push_back(local);
                        for (size_t begin = 0; begin < rows.size();) {
                            const size_t count = std::min(options.RowsPerBatch, rows.size() - begin);
                            const size_t index = prepared->BatchPlans.size();
                            auto data = std::make_shared<QueryBatchData>();
                            data->Execution = std::make_shared<QueryExecution>();
                            auto& execution = *data->Execution;
                            execution.Context = captured->OwnerContext;
                            execution.Active = false;
                            execution.SnapshotMode = true;
                            execution.Borrows = prepared->Borrows;
                            execution.Random = query.Random;
                            execution.ResourceSnapshots = resources;
                            execution.ResourceOwner = &captured->OwnerContext->Resources();
                            for (const auto& [id, weak] : captured->Lifetimes) {
                                execution.BorrowedWorlds.push_back(weak);
                                execution.Writes.emplace(id, QueryExecution::WriteJournal{weak, {}});
                            }
                            data->Lifetime = captured->Main;
                             data->MainBorrow = prepared->Borrows.at(captured->MainWorld);
                             data->StructureVersion = data->MainBorrow->StructureVersion();
                            data->IgnoreEnabled = query.Description.IgnoreComponentEnabled;
                            data->Chunk = chunk;
                            data->Rows.assign(rows.begin() + begin, rows.begin() + begin + count);
                            data->Writes = binding.Writes;
                            for (size_t argument = 0; argument < query.Description.Columns.size(); ++argument) {
                                const auto& term = query.Description.Columns[argument];
                                data->Bindings.push_back({captured->OwnerContext->Types().Find(term.Type), binding.Columns[argument],
                                    FindMask(chunk, term.Type), term.Access});
                            }
                            auto slot = std::make_unique<PreparedExecution::Impl::BatchSlot>();
                            slot->Data = std::move(data);
                            prepared->Slots.push_back(std::move(slot));
                            prepared->BatchPlans.push_back({chunkIndex, index, begin, count});
                            ++plan.BatchCount;
                            begin += count;
                        }
                        prepared->Rows += rows.size();
                        prepared->ChunkPlans.push_back(std::move(plan));
                        prepared->ChunkStates.push_back({chunk, binding.Changed});
                    }
                }
                prepared->Small = prepared->Rows < options.SmallTaskRows;
                captured->Executed = true;
                return PreparedExecution(std::move(prepared));
            }
            catch (const std::exception& exception) { return QueryError(ErrorCode::ConstructionFailed, exception.what()); }
        }

        std::span<const PreparedChunk> PreparedExecution::Chunks() const noexcept {
            return m_Impl ? std::span<const PreparedChunk>(m_Impl->ChunkPlans) : std::span<const PreparedChunk>{};
        }
        std::span<const PreparedBatch> PreparedExecution::Batches() const noexcept {
            return m_Impl ? std::span<const PreparedBatch>(m_Impl->BatchPlans) : std::span<const PreparedBatch>{};
        }
        size_t PreparedExecution::CandidateRows() const noexcept { return m_Impl ? m_Impl->Rows : 0; }
        bool PreparedExecution::RequiresSerial() const noexcept { return m_Impl && m_Impl->Serial; }
        bool PreparedExecution::MergeSmallTask() const noexcept { return m_Impl && m_Impl->Small; }

        Result<bool> PreparedExecution::BeginChunk(size_t index) {
            const auto state = m_Impl;
            if (!state) return QueryError(ErrorCode::InvalidState, "The prepared execution was moved from");
            std::lock_guard lock(state->Mutex);
            if (state->Finished || index >= state->ChunkStates.size()) return QueryError(ErrorCode::InvalidState, "The prepared Chunk is unavailable");
            auto& chunk = state->ChunkStates[index];
            if (chunk.Begun || chunk.Complete) return QueryError(ErrorCode::InvalidState, "The prepared Chunk was already started");
            if (!state->Valid()) return QueryError(ErrorCode::InvalidState, "A borrowed World was destroyed");
            try {
                auto observer = state->StartObserver();
                if (!observer) return observer.GetError();
                bool shouldRun = chunk.ChangedColumns.empty();
                for (size_t column : chunk.ChangedColumns) {
                    const auto& info = chunk.Snapshot.Columns[column];
                    const ObservationKey key{state->Capture->QueryState->FilterId, state->Capture->MainWorld, chunk.Snapshot.Handle(), info.Type->Id};
                    const auto observed = state->Baseline.find(key);
                    const uint64_t version = info.CurrentVersion();
                    shouldRun |= observed == state->Baseline.end() || observed->second.Version != version ||
                        observed->second.MaskVersion != chunk.Snapshot.MaskVersion;
                    auto& next = state->Pending.at(key);
                    next.Version = version;
                    next.MaskVersion = chunk.Snapshot.MaskVersion;
                }
                chunk.Begun = true;
                chunk.ShouldRun = shouldRun;
                if (!shouldRun) {
                    const auto& plan = state->ChunkPlans[index];
                    for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                        state->Slots[batch]->Stage.store(Impl::BatchStage::Skipped, std::memory_order_release);
                    }
                }
                return shouldRun;
            }
            catch (const std::exception& exception) { state->Failed = true; return QueryError(ErrorCode::ConstructionFailed, exception.what()); }
        }

        Result<void> PreparedExecution::RunBatch(size_t index, const std::function<Result<void>(QueryBatch&)>& callback) {
            const auto state = m_Impl;
            if (!state) return QueryError(ErrorCode::InvalidState, "The prepared execution was moved from");
            Impl::BatchSlot* slot = nullptr;
            {
                std::lock_guard lock(state->Mutex);
                if (state->Finished || index >= state->Slots.size()) return QueryError(ErrorCode::InvalidState, "The prepared batch is unavailable");
                auto& chunk = state->ChunkStates[state->BatchPlans[index].ChunkIndex];
                if (!chunk.Begun || !chunk.ShouldRun || chunk.Complete) return QueryError(ErrorCode::InvalidState, "The prepared Chunk is not ready");
                slot = state->Slots[index].get();
                auto expected = Impl::BatchStage::Ready;
                if (!slot->Stage.compare_exchange_strong(expected, Impl::BatchStage::Running)) return QueryError(ErrorCode::InvalidState, "The prepared batch was already consumed");
            }
            auto& data = *slot->Data;
            struct BatchFinish {
                Impl::BatchSlot& Slot;
                bool Failed = true;
                ~BatchFinish() {
                    Slot.Data->Active.store(false, std::memory_order_release);
                    Slot.Data->Execution->Active.store(false, std::memory_order_release);
                    Slot.Failed = Failed;
                    Slot.Stage.store(Impl::BatchStage::Done, std::memory_order_release);
                }
            } finish{*slot};
            if (!state->Valid()) return QueryError(ErrorCode::InvalidState, "A borrowed World was destroyed");
            if (!callback) return QueryError(ErrorCode::InvalidArgument, "The prepared callback is empty");
            try {
                for (size_t column : data.Writes) {
                    const TypeId type = data.Chunk.Columns[column].Type->Id;
                    const size_t mask = FindMask(data.Chunk, type);
                    const bool present = data.IgnoreEnabled || mask == Missing || std::any_of(data.Rows.begin(), data.Rows.end(), [&](size_t row) {
                        return data.Chunk.ComponentEnabled[mask].Enabled(row);
                    });
                    if (present) {
                        auto marked = data.Execution->AddWrite(state->Capture->MainWorld, state->Capture->Main, {data.Chunk.Handle(), type});
                        if (!marked) {
                            for (auto& [id, journal] : data.Execution->Writes) journal.Columns.clear();
                            return marked;
                        }
                    }
                }
                data.Execution->Active.store(true, std::memory_order_release);
                data.Active.store(true, std::memory_order_release);
                QueryBatch batch(slot->Data);
                Result<void> result;
                try { result = callback(batch); }
                catch (const std::exception& exception) { result = QueryError(ErrorCode::TaskFailed, exception.what()); }
                catch (...) { result = QueryError(ErrorCode::TaskFailed, "The query callback threw a non-standard exception"); }
                if (!state->Valid()) result = QueryError(ErrorCode::InvalidState, "A borrowed World was destroyed");
                finish.Failed = !result;
                return result;
            }
            catch (const std::exception& exception) { return QueryError(ErrorCode::ConstructionFailed, exception.what()); }
            catch (...) { return QueryError(ErrorCode::TaskFailed, "The prepared batch failed with a non-standard exception"); }
        }

        Result<void> PreparedExecution::CompleteChunk(size_t index, bool cancelled) {
            const auto state = m_Impl;
            if (!state) return QueryError(ErrorCode::InvalidState, "The prepared execution was moved from");
            std::lock_guard lock(state->Mutex);
            if (state->Finished || index >= state->ChunkStates.size()) return QueryError(ErrorCode::InvalidState, "The prepared Chunk is unavailable");
            auto& chunk = state->ChunkStates[index];
            if (chunk.Complete || (!chunk.Begun && !cancelled)) return QueryError(ErrorCode::InvalidState, "The prepared Chunk cannot complete in its current state");
            const auto& plan = state->ChunkPlans[index];
            for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                const auto stage = state->Slots[batch]->Stage.load(std::memory_order_acquire);
                if (stage == Impl::BatchStage::Running) return QueryError(ErrorCode::Busy, "A Chunk child is still running");
                if (stage == Impl::BatchStage::Ready && !cancelled) return QueryError(ErrorCode::InvalidState, "A Chunk child has not run");
            }
            try {
                bool failed = cancelled;
                for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                    auto& slot = *state->Slots[batch];
                    if (slot.Stage.load() == Impl::BatchStage::Ready) slot.Stage.store(Impl::BatchStage::Cancelled);
                    failed |= slot.Failed;
                }
                auto published = state->PublishChunk(index);
                chunk.Complete = true;
                state->Failed |= failed || !published;
                if (!published) return published;
                if (failed) return QueryError(cancelled ? ErrorCode::Cancelled : ErrorCode::TaskFailed, "The prepared Chunk did not succeed");
                return {};
            }
            catch (const std::exception& exception) {
                // Allocation failure must still publish started writes and retire the Chunk.
                chunk.Complete = true;
                state->Failed = true;
                for (size_t batch = plan.FirstBatch; batch < plan.FirstBatch + plan.BatchCount; ++batch) {
                    (void)state->Slots[batch]->Data->Execution->Publish();
                }
                return QueryError(ErrorCode::ConstructionFailed, exception.what());
            }
        }

        Result<void> PreparedExecution::Finish(bool succeeded) {
            const auto state = m_Impl;
            if (!state) return QueryError(ErrorCode::InvalidState, "The prepared execution was moved from");
            std::unique_lock lock(state->Mutex);
            if (state->Finished) return QueryError(ErrorCode::InvalidState, "The prepared execution already finished");
            if (std::any_of(state->ChunkStates.begin(), state->ChunkStates.end(), [](const auto& chunk) { return !chunk.Complete; })) {
                return QueryError(ErrorCode::Busy, "Prepared Chunks have not completed");
            }
            std::optional<Error> error;
            if (succeeded && !state->Failed && !state->Valid()) error = QueryError(ErrorCode::InvalidState, "A borrowed World was destroyed");
            try {
                if (succeeded && !state->Failed && !error) {
                    auto observer = state->StartObserver();
                    if (!observer) error = observer.GetError();
                }
                if (state->ObserverStarted && state->Capture->Changed) {
                    std::lock_guard observerLock(state->Capture->Changed->Mutex);
                    if (succeeded && !state->Failed && !error) state->Capture->Changed->Observations.swap(state->Pending);
                    state->Capture->Changed->Running = false;
                    state->ObserverStarted = false;
                }
            }
            catch (const std::exception& exception) { error = QueryError(ErrorCode::ConstructionFailed, exception.what()); }
            state->Finished = true;
            lock.unlock();
            state->Capture->ReleaseReservations();
            if (error) return *error;
            if (!succeeded || state->Failed) return QueryError(ErrorCode::TaskFailed, "The prepared task did not succeed");
            return {};
        }
    }

    EntityId QueryBatch::Entity(size_t row) const noexcept {
        if (!Valid()) return {};
        const auto data = m_Data.lock();
        return row < data->Rows.size() ? data->Chunk.Entities[data->Rows[row]] : EntityId{};
    }
    ChunkHandle QueryBatch::Chunk() const noexcept {
        return Valid() ? m_Data.lock()->Chunk.Handle() : ChunkHandle{};
    }

    Result<void> Query::Run(World& world,
        const std::function<Result<void>(QueryBatch&)>& callback, ChangedState* changed) {
        return RunShared(m_Impl, world, callback, changed ? changed->m_Impl : nullptr, false, {});
    }

    Result<void> Query::RunShared(std::shared_ptr<Impl> implementation, World& world,
        const std::function<Result<void>(QueryBatch&)>& callback,
        std::shared_ptr<ChangedState::Impl> changed, bool scheduled, std::span<const WorldBorrow> capturedBorrows) {
        if (!implementation) return QueryError(ErrorCode::InvalidState, "The Query was moved from");
        if (&world.Context() != implementation->Context) return QueryError(ErrorCode::InvalidArgument, "The World belongs to another Context");
        if (!implementation->Context->IsMainThread()) return QueryError(ErrorCode::WrongThread, "Serial query execution requires the Context owner thread");
        std::unique_lock<std::mutex> observerLock;
        if (changed) observerLock = std::unique_lock(changed->Mutex);
        if (!scheduled && (implementation->Context->HasScheduledWork() || implementation->PendingUsers.load(std::memory_order_acquire) != 0 ||
            (changed && changed->PendingUsers != 0))) {
            return QueryError(ErrorCode::Busy, "Direct queries require queued Timeline work to finish first");
        }
        if (implementation->Running || implementation->CacheState->Executing || (changed && changed->Running)) {
            return QueryError(ErrorCode::Busy, "A Query or Changed observer is already executing in this Context");
        }
        if (!implementation->Filter.Changed.empty() && (!changed || changed->Identity == 0)) {
            return QueryError(ErrorCode::InvalidArgument, "Changed queries require a valid consumer state");
        }
        implementation->Running = true;
        implementation->CacheState->Executing = true;
        if (changed) changed->Running = true;
        if (observerLock.owns_lock()) observerLock.unlock();
        struct RunGuard {
            Impl& QueryImpl;
            bool& ContextExecuting;
            ChangedState::Impl* Observer;
            ~RunGuard() {
                QueryImpl.Running = false;
                ContextExecuting = false;
                if (Observer) { std::lock_guard lock(Observer->Mutex); Observer->Running = false; }
            }
        } runGuard{*implementation, implementation->CacheState->Executing, changed.get()};
        std::shared_ptr<Detail::QueryExecution> execution;
        std::vector<WorldBorrow> localBorrows;
        std::span<const WorldBorrow> borrows = capturedBorrows;
        try {
            execution = std::make_shared<Detail::QueryExecution>();
            execution->Context = implementation->Context;
            execution->Random = implementation->Random;
            execution->Resources = implementation->Description.ResourceAccesses;
            std::map<WorldId, World*> targets{{world.Id(), &world}};
            for (const auto& random : implementation->Random) {
                const auto lifetime = random.Lifetime.lock();
                auto* target = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
                if (!target || lifetime->Id != random.World) return QueryError(ErrorCode::InvalidState, "A declared random-access World was destroyed");
                targets.emplace(random.World, target);
            }
            if (!scheduled) localBorrows.reserve(targets.size());
            for (const auto& [id, target] : targets) {
                if (!scheduled) {
                    auto borrow = target->AcquireBorrow();
                    if (!borrow) return borrow.GetError();
                    localBorrows.push_back(std::move(borrow).Value());
                }
                execution->BorrowedWorlds.push_back(target->Lifetime());
                execution->Writes.emplace(id, Detail::QueryExecution::WriteJournal{target->Lifetime(), {}});
            }
            if (!scheduled) borrows = localBorrows;
            auto& cache = implementation->Refresh(world);
            std::vector<std::shared_ptr<Detail::QueryBatchData>> batches;
            std::set<ChunkHandle> matchingChunks;
            for (const auto& [group, layout] : cache.Groups) {
                auto chunks = world.GetGroupChunks(group);
                if (!chunks) return chunks.GetError();
                const auto& binding = cache.Layouts.at(layout);
                for (auto& chunk : chunks.Value()) {
                    matchingChunks.insert(chunk.Handle());
                    ++implementation->Counters.CandidateChunks;
                    auto data = std::make_shared<Detail::QueryBatchData>();
                    data->Execution = execution;
                    data->Lifetime = world.Lifetime();
                    data->StructureVersion = world.StructureVersion();
                    data->IgnoreEnabled = implementation->Description.IgnoreComponentEnabled;
                    data->Chunk = std::move(chunk);
                    data->Changed = binding.Changed;
                    data->Writes = binding.Writes;
                    for (size_t argument = 0; argument < implementation->Description.Columns.size(); ++argument) {
                        const auto& term = implementation->Description.Columns[argument];
                        data->Bindings.push_back({world.Types().Find(term.Type), binding.Columns[argument],
                            FindMask(data->Chunk, term.Type), term.Access});
                    }
                    const RowMatcher rows(data->Chunk, implementation->Filter);
                    for (size_t row = 0; row < data->Chunk.Entities.size(); ++row) {
                        ++implementation->Counters.VisitedRows;
                        if (rows.Matches(row)) data->Rows.push_back(row);
                    }
                    if (!data->Rows.empty()) batches.push_back(std::move(data));
                }
            }

            std::map<ObservationKey, Observation> pending;
            if (changed) {
                pending = changed->Observations;
                std::erase_if(pending, [&](const auto& entry) {
                    const auto lifetime = entry.second.Lifetime.lock();
                    const auto* owner = lifetime ? lifetime->Owner.load(std::memory_order_acquire) : nullptr;
                    if (!owner) return true;
                    // Only dereference the World protected by this execution's borrow.
                    if (entry.first.World != world.Id()) return false;
                    if (owner != &world || !world.InspectChunk(entry.first.Chunk)) return true;
                    return entry.first.Filter == implementation->FilterId && !matchingChunks.contains(entry.first.Chunk);
                });
                for (const auto& batch : batches) for (size_t column : batch->Changed) {
                    const auto type = batch->Chunk.Columns[column].Type->Id;
                    pending.try_emplace({implementation->FilterId, world.Id(), batch->Chunk.Handle(), type}, Observation{world.Lifetime(), 0, 0});
                }
            }

            for (const auto& data : batches) {
                if (std::any_of(borrows.begin(), borrows.end(), [](const auto& borrow) { return !borrow.Valid(); })) {
                    auto published = execution->Publish();
                    execution->Active = false;
                    if (!published) return published;
                    return QueryError(ErrorCode::InvalidState, "A borrowed World was destroyed");
                }
                bool shouldRun = implementation->Filter.Changed.empty();
                // Version counters are inspected only under the live World borrow.
                for (size_t column : data->Changed) {
                    const auto& info = data->Chunk.Columns[column];
                    const ObservationKey key{implementation->FilterId, world.Id(), data->Chunk.Handle(), info.Type->Id};
                    const auto observed = changed->Observations.find(key);
                    const uint64_t version = info.CurrentVersion();
                    shouldRun |= observed == changed->Observations.end() || observed->second.Version != version ||
                        observed->second.MaskVersion != data->Chunk.MaskVersion;
                    auto& next = pending.at(key);
                    next.Version = version;
                    next.MaskVersion = data->Chunk.MaskVersion;
                }
                if (!shouldRun) continue;
                for (size_t column : data->Writes) {
                    const TypeId type = data->Chunk.Columns[column].Type->Id;
                    const size_t mask = FindMask(data->Chunk, type);
                    const bool anyPresent = data->IgnoreEnabled || mask == Missing || std::any_of(data->Rows.begin(), data->Rows.end(), [&](size_t row) {
                        return data->Chunk.ComponentEnabled[mask].Enabled(row);
                    });
                    if (anyPresent) {
                        auto marked = execution->AddWrite(world, {data->Chunk.Handle(), type});
                        if (!marked) return marked;
                    }
                }
                data->Active = true;
                QueryBatch batch(data);
                Result<void> result;
                try { result = callback(batch); }
                catch (const std::exception& exception) { result = QueryError(ErrorCode::TaskFailed, exception.what()); }
                catch (...) { result = QueryError(ErrorCode::TaskFailed, "The query callback threw a non-standard exception"); }
                data->Active = false;
                const bool worldsAlive = std::all_of(borrows.begin(), borrows.end(), [](const auto& borrow) { return borrow.Valid(); });
                auto published = execution->Publish();
                if (!published) { execution->Active = false; return published; }
                if (!worldsAlive) {
                    execution->Active = false;
                    return QueryError(ErrorCode::InvalidState, "A borrowed World was destroyed");
                }
                if (!result) { execution->Active = false; return result; }
            }
            execution->Active = false;
            if (changed) { std::lock_guard lock(changed->Mutex); changed->Observations.swap(pending); }
            return {};
        }
        catch (const std::exception& exception) {
            if (execution) {
                auto published = execution->Publish();
                execution->Active = false;
                if (!published) return published;
            }
            return QueryError(ErrorCode::ConstructionFailed, exception.what());
        }
        catch (...) {
            if (execution) {
                auto published = execution->Publish();
                execution->Active = false;
                if (!published) return published;
            }
            return QueryError(ErrorCode::TaskFailed, "Query execution failed with a non-standard exception");
        }
    }
}
