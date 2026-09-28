#include "HuaEngine/ECS/Runtime/World.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <stdexcept>
#include <utility>

namespace HE::Ecs {
    namespace {
        std::atomic<WorldId> NextWorldId{1};
        WorldId AllocateWorldId() noexcept {
            auto value = NextWorldId.load(std::memory_order_relaxed);
            while (value != std::numeric_limits<WorldId>::max()) {
                if (NextWorldId.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) return value;
            }
            return 0;
        }
        Error Failure(std::string operation, const std::exception& exception) {
            return {ErrorCode::ConstructionFailed, std::move(operation), exception.what()};
        }
        size_t Align(size_t value, size_t alignment) {
            if (value > std::numeric_limits<size_t>::max() - (alignment - 1)) throw std::length_error("Chunk layout overflow");
            return (value + alignment - 1) & ~(alignment - 1);
        }
        size_t AddColumn(size_t offset, size_t count, size_t stride, size_t alignment) {
            offset = Align(offset, alignment);
            if (count > (std::numeric_limits<size_t>::max() - offset) / stride) throw std::length_error("Chunk layout overflow");
            return offset + count * stride;
        }
        template<typename T>
        void ReserveExtra(std::vector<T>& values, size_t extra) {
            if (extra > values.max_size() - values.size()) throw std::length_error("Storage metadata overflow");
            const size_t needed = values.size() + extra;
            if (needed > values.capacity()) values.reserve(std::max(needed, values.capacity() <= values.max_size() / 2 ? std::max<size_t>(8, values.capacity() * 2) : values.max_size()));
        }
        bool Bit(std::span<const uint64_t> words, size_t row) noexcept { return (words[row / 64] & (uint64_t{1} << (row % 64))) != 0; }
        void SetBit(std::span<uint64_t> words, size_t row, bool enabled) noexcept {
            const auto bit = uint64_t{1} << (row % 64);
            if (enabled) words[row / 64] |= bit; else words[row / 64] &= ~bit;
        }
        struct MaintenanceGuard {
            explicit MaintenanceGuard(bool& active) noexcept : Active(active) { Active = true; }
            ~MaintenanceGuard() { Active = false; }
            MaintenanceGuard(const MaintenanceGuard&) = delete;
            MaintenanceGuard& operator=(const MaintenanceGuard&) = delete;
            bool& Active;
        };
    }

    WorldBorrow::WorldBorrow(std::shared_ptr<Detail::WorldLifetime> lifetime, std::shared_ptr<void> storage)
        : m_Lifetime(std::move(lifetime)), m_Storage(std::move(storage)) { m_Lifetime->BorrowCount.fetch_add(1); }
    WorldBorrow::~WorldBorrow() { Release(); }
    WorldBorrow::WorldBorrow(WorldBorrow&& other) noexcept : m_Lifetime(std::move(other.m_Lifetime)), m_Storage(std::move(other.m_Storage)) {}
    WorldBorrow& WorldBorrow::operator=(WorldBorrow&& other) noexcept {
        if (this != &other) { Release(); m_Lifetime = std::move(other.m_Lifetime); m_Storage = std::move(other.m_Storage); }
        return *this;
    }
    void WorldBorrow::Release() noexcept {
        if (m_Lifetime) { m_Lifetime->BorrowCount.fetch_sub(1); m_Lifetime.reset(); }
        m_Storage.reset();
    }
    bool WorldBorrow::Valid() const noexcept { return m_Lifetime && m_Lifetime->Owner.load() != nullptr; }
    WorldId WorldBorrow::Id() const noexcept { return m_Lifetime ? m_Lifetime->Id : 0; }

    struct World::Impl {
        struct Layout {
            LayoutId Id = 0;
            std::vector<TypeId> Types;
            std::vector<const RegisteredType*> Descriptors;
            std::vector<size_t> Offsets;
            size_t Capacity = 0;
            size_t Bytes = 0;
            size_t Alignment = alignof(EntityId);
            size_t Column(TypeId type) const noexcept {
                const auto found = std::lower_bound(Types.begin(), Types.end(), type);
                return found != Types.end() && *found == type ? static_cast<size_t>(found - Types.begin()) : Types.size();
            }
        };
        struct Group;
        struct Block {
            std::pmr::memory_resource* Allocator;
            size_t Bytes;
            size_t Alignment;
            std::byte* Data;
            Block(std::pmr::memory_resource* allocator, size_t bytes, size_t alignment)
                : Allocator(allocator), Bytes(bytes), Alignment(alignment), Data(static_cast<std::byte*>(allocator->allocate(bytes, alignment))) {}
            ~Block() { Reset(); }
            Block(const Block&) = delete;
            Block& operator=(const Block&) = delete;
            Block(Block&& other) noexcept
                : Allocator(other.Allocator), Bytes(other.Bytes), Alignment(other.Alignment), Data(std::exchange(other.Data, nullptr)) {}
            Block& operator=(Block&& other) noexcept {
                if (this != &other) {
                    Reset();
                    Allocator = other.Allocator;
                    Bytes = other.Bytes;
                    Alignment = other.Alignment;
                    Data = std::exchange(other.Data, nullptr);
                }
                return *this;
            }
            void Reset() noexcept {
                if (Data) {
                    Allocator->deallocate(Data, Bytes, Alignment);
                    Data = nullptr;
                }
            }
        };
        struct PooledBlock {
            size_t Capacity;
            Block Memory;
        };
        struct Chunk {
            ChunkId Id;
            uint64_t Generation = 1;
            Layout* Shape;
            Group* Owner;
            size_t GroupIndex;
            size_t Count = 0;
            std::unique_ptr<std::atomic<uint64_t>[]> Versions;
            std::vector<TypeId> MaskTypes;
            size_t MaskWords;
            std::vector<uint64_t> EntityBits;
            std::vector<uint64_t> ComponentBits;
            uint64_t MaskVersion = 1;
            Block Memory;
            Chunk(ChunkId id, Layout& layout, Group& group, size_t groupIndex, std::span<const TypeId> masks, Block&& memory)
                : Id(id), Shape(&layout), Owner(&group), GroupIndex(groupIndex), Versions(std::make_unique<std::atomic<uint64_t>[]>(layout.Types.size())),
                  MaskTypes(masks.begin(), masks.end()), MaskWords((layout.Capacity + 63) / 64), EntityBits(MaskWords, ~uint64_t{0}),
                  ComponentBits(MaskWords * masks.size(), ~uint64_t{0}), Memory(std::move(memory)) {
                for (size_t column = 0; column < layout.Types.size(); ++column) Versions[column].store(1);
            }
            ~Chunk() { Clear(); }
            EntityId* Entities() const noexcept { return reinterpret_cast<EntityId*>(Memory.Data); }
            void* Slot(size_t column, size_t row) const noexcept {
                return Memory.Data + Shape->Offsets[column] + row * Shape->Descriptors[column]->Descriptor.SlotSize();
            }
            void* Object(size_t column, size_t row) const noexcept {
                void* slot = Slot(column, row);
                return Shape->Descriptors[column]->Descriptor.Storage == StorageKind::Indirect ? *static_cast<void**>(slot) : slot;
            }
            size_t MaskIndex(TypeId type) const noexcept {
                const auto found = std::find(MaskTypes.begin(), MaskTypes.end(), type);
                return static_cast<size_t>(found - MaskTypes.begin());
            }
            std::span<uint64_t> Mask(size_t index) noexcept { return {ComponentBits.data() + index * MaskWords, MaskWords}; }
            std::span<const uint64_t> Mask(size_t index) const noexcept { return {ComponentBits.data() + index * MaskWords, MaskWords}; }
            void DefaultMasks(size_t row) noexcept {
                SetBit(EntityBits, row, true);
                for (size_t index = 0; index < MaskTypes.size(); ++index) SetBit(Mask(index), row, true);
                ++MaskVersion;
            }
            void DestroyCell(size_t column, size_t row) noexcept {
                const auto& type = Shape->Descriptors[column]->Descriptor;
                void* object = Object(column, row);
                type.Destroy(object);
                if (type.Storage == StorageKind::Indirect) {
                    ::operator delete(object, std::align_val_t(type.Alignment));
                    *static_cast<void**>(Slot(column, row)) = nullptr;
                }
            }
            void Clear() noexcept {
                for (size_t column = 0; column < Shape->Types.size(); ++column) {
                    for (size_t row = 0; row < Count; ++row) DestroyCell(column, row);
                    ++Versions[column];
                }
                Count = 0;
                ++MaskVersion;
            }
        };
        struct GroupKey {
            std::vector<TypeId> Types;
            std::vector<TypeId> Tags;
            std::vector<SharedBinding> Shared;
            auto operator<=>(const GroupKey&) const = default;
        };
        struct Group {
            GroupId Id;
            uint64_t Generation;
            Layout* Shape;
            std::vector<TypeId> Tags;
            std::vector<SharedBinding> Shared;
            std::vector<std::unique_ptr<Chunk>> Chunks;
            size_t FirstAvailable = 0;
            std::vector<TypeId> MaskTypes;
        };
        struct GroupEvent { uint64_t Revision; GroupHandle Handle; bool Added; };
        enum class TransitionKind { Add, Remove, AddTag, RemoveTag, Shared, RemoveShared };
        struct TransitionKey {
            GroupId Source;
            TransitionKind Operation;
            TypeId Type;
            ResourceId Resource = 0;
            auto operator<=>(const TransitionKey&) const = default;
        };
        struct MigrationPlan {
            Group* Target;
            std::vector<size_t> SourceColumns;
            std::vector<size_t> RemovedColumns;
            std::vector<size_t> SourceMasks;
        };
        struct Record {
            uint32_t Generation = 1;
            bool Retired = false;
            Chunk* Block = nullptr;
            size_t Row = 0;
            EntityUuid Uuid;
            std::string Name;
        };

        EcsContext& Context;
        WorldOptions Options;
        WorldId Identity = AllocateWorldId();
        uint64_t Version = 1;
        uint64_t GroupVersion = 0;
        uint64_t StorageGeneration = 1;
        uint64_t HistoryFloor = 0;
        std::shared_ptr<Detail::WorldLifetime> Lifetime = std::make_shared<Detail::WorldLifetime>();
        uint64_t NextUuid = 1;
        LayoutId NextLayout = 1;
        GroupId NextGroup = 1;
        ChunkId NextChunk = 1;
        std::map<std::vector<TypeId>, std::unique_ptr<Layout>> Layouts;
        std::map<GroupKey, std::unique_ptr<Group>> Groups;
        std::vector<Group*> GroupsById;
        std::vector<Chunk*> ChunksById;
        std::vector<GroupEvent> GroupEvents;
        std::map<TransitionKey, MigrationPlan> Migrations;
        uint64_t MigrationPlanBuilds = 0;
        uint64_t MigrationCacheHits = 0;
        std::vector<PooledBlock> Pool;
        size_t PoolBytes = 0;
        uint64_t PoolHits = 0;
        uint64_t ChunkAllocations = 0;
        uint64_t ReturnedChunkBytes = 0;
        uint64_t CompactionMovedRows = 0;
        uint64_t CompactionMovedBytes = 0;
        uint64_t CompactionReleasedChunks = 0;
        size_t ReadScopes = 0;
        bool EditScope = false;
        bool InMaintenance = false;
        mutable bool EditTouched = false;
        std::vector<Record> Records;
        std::vector<uint32_t> Free;
        std::unordered_map<EntityUuid, EntityId> ByUuid;
        size_t Count = 0;

        Impl(EcsContext& context, WorldOptions options) : Context(context), Options(options) {
            if (!context.IsMainThread()) throw std::logic_error("World must be constructed on the Context main thread");
            if (!Identity || options.ChunkBytes < sizeof(EntityId) || !options.GenerationLimit || !options.ChunkMemory) {
                throw std::invalid_argument("Invalid World configuration or exhausted World identities");
            }
        }
        Result<void> Check(std::string operation) const {
            if (!Context.IsMainThread()) return Error{ErrorCode::WrongThread, std::move(operation), "Structural access requires the Context main thread"};
            if (InMaintenance) return Error{ErrorCode::Busy, std::move(operation), "World storage maintenance is in progress"};
            if (ReadScopes) return Error{ErrorCode::Busy, std::move(operation), "A read scope is holding this World"};
            if (Lifetime->BorrowCount.load() != 0) return Error{ErrorCode::Busy, std::move(operation), "A query execution is borrowing this World"};
            return {};
        }
        Record* Get(EntityId id) noexcept {
            if (!id || id.Index >= Records.size()) return nullptr;
            auto& record = Records[id.Index];
            return record.Block && record.Generation == id.Generation ? &record : nullptr;
        }
        const Record* Get(EntityId id) const noexcept { return const_cast<Impl*>(this)->Get(id); }
        static GroupKey Key(const Record& record) {
            const auto& group = *record.Block->Owner;
            return {group.Shape->Types, group.Tags, group.Shared};
        }
        Layout& GetLayout(const std::vector<TypeId>& types) {
            if (const auto found = Layouts.find(types); found != Layouts.end()) return *found->second;
            auto layout = std::make_unique<Layout>();
            layout->Id = NextLayout++;
            layout->Types = types;
            for (TypeId id : types) {
                const auto* type = Context.Types().Find(id);
                if (!type || type->Descriptor.Storage == StorageKind::Tag) throw std::invalid_argument("Layout requires registered stored types");
                layout->Descriptors.push_back(type);
                layout->Alignment = std::max(layout->Alignment, type->Descriptor.SlotAlignment());
            }
            auto bytesFor = [&](size_t capacity) {
                size_t bytes = AddColumn(0, capacity, sizeof(EntityId), alignof(EntityId));
                for (const auto* type : layout->Descriptors) {
                    bytes = AddColumn(bytes, capacity, type->Descriptor.SlotSize(), type->Descriptor.SlotAlignment());
                }
                return bytes;
            };
            const size_t single = bytesFor(1);
            if (single > Options.ChunkBytes) {
                layout->Capacity = 1;
                layout->Bytes = single;
            }
            else {
                size_t low = 1;
                size_t high = Options.ChunkBytes / sizeof(EntityId);
                while (low < high) {
                    const size_t middle = low + (high - low + 1) / 2;
                    bool fits = false;
                    try { fits = bytesFor(middle) <= Options.ChunkBytes; }
                    catch (const std::length_error&) {}
                    if (fits) low = middle; else high = middle - 1;
                }
                layout->Capacity = low;
                layout->Bytes = Options.ChunkBytes;
            }
            size_t offset = layout->Capacity * sizeof(EntityId);
            for (const auto* type : layout->Descriptors) {
                offset = Align(offset, type->Descriptor.SlotAlignment());
                layout->Offsets.push_back(offset);
                offset += layout->Capacity * type->Descriptor.SlotSize();
            }
            auto* result = layout.get();
            Layouts.emplace(types, std::move(layout));
            return *result;
        }
        Group& GetGroup(GroupKey key) {
            if (const auto found = Groups.find(key); found != Groups.end()) return *found->second;
            auto group = std::make_unique<Group>();
            group->Id = NextGroup++;
            group->Generation = StorageGeneration;
            group->Shape = &GetLayout(key.Types);
            group->Tags = key.Tags;
            group->Shared = key.Shared;
            group->MaskTypes = key.Types;
            group->MaskTypes.insert(group->MaskTypes.end(), key.Tags.begin(), key.Tags.end());
            ReserveExtra(GroupEvents, 1);
            if (GroupsById.size() <= group->Id) GroupsById.resize(static_cast<size_t>(group->Id + 1), nullptr);
            auto* result = group.get();
            Groups.emplace(std::move(key), std::move(group));
            GroupsById[static_cast<size_t>(result->Id)] = result;
            GroupEvents.push_back({++GroupVersion, {result->Id, result->Generation}, true});
            TrimHistory();
            return *result;
        }
        Block AcquireBlock(const Layout& layout) {
            const auto found = std::find_if(Pool.begin(), Pool.end(), [&](const PooledBlock& value) {
                return value.Capacity == layout.Capacity && value.Memory.Bytes == layout.Bytes &&
                    value.Memory.Alignment == layout.Alignment;
            });
            if (found != Pool.end()) {
                Block result(std::move(found->Memory));
                PoolBytes -= result.Bytes;
                Pool.erase(found);
                ++PoolHits;
                return result;
            }
            Block result(Options.ChunkMemory, layout.Bytes, layout.Alignment);
            ++ChunkAllocations;
            return result;
        }
        Chunk& Destination(Group& group) {
            while (group.FirstAvailable < group.Chunks.size() && group.Chunks[group.FirstAvailable]->Count == group.Shape->Capacity) ++group.FirstAvailable;
            if (group.FirstAvailable < group.Chunks.size()) return *group.Chunks[group.FirstAvailable];
            auto chunk = std::make_unique<Chunk>(NextChunk++, *group.Shape, group, group.Chunks.size(), group.MaskTypes, AcquireBlock(*group.Shape));
            chunk->Generation = StorageGeneration;
            if (ChunksById.size() <= chunk->Id) ChunksById.resize(static_cast<size_t>(chunk->Id + 1), nullptr);
            auto* result = chunk.get();
            group.Chunks.push_back(std::move(chunk));
            ChunksById[static_cast<size_t>(result->Id)] = result;
            return *result;
        }
        Group* Resolve(GroupHandle handle) const noexcept {
            if (!handle.Id || handle.Id >= GroupsById.size()) return nullptr;
            auto* group = GroupsById[static_cast<size_t>(handle.Id)];
            return group && group->Generation == handle.Generation ? group : nullptr;
        }
        Chunk* Resolve(ChunkHandle handle) const noexcept {
            if (!handle.Id || handle.Id >= ChunksById.size()) return nullptr;
            auto* chunk = ChunksById[static_cast<size_t>(handle.Id)];
            return chunk && chunk->Generation == handle.Generation ? chunk : nullptr;
        }
        Result<void> Publish(std::span<const ChunkColumnKey> columns) {
            for (const auto& key : columns) {
                const auto* chunk = Resolve(key.Chunk);
                if (!chunk) return Error{ErrorCode::InvalidState, "PublishWrites", "The chunk handle has expired"};
                if (chunk->Shape->Column(key.Type) == chunk->Shape->Types.size()) return Error{ErrorCode::InvalidType, "PublishWrites", "The chunk does not contain this stored component"};
            }
            for (const auto& key : columns) {
                auto* chunk = Resolve(key.Chunk);
                chunk->Versions[chunk->Shape->Column(key.Type)].fetch_add(1, std::memory_order_release);
            }
            return {};
        }
        static GroupInfo Describe(const Group& group) {
            return {{group.Id, group.Generation}, group.Shape->Id, group.Shape->Types, group.Tags, group.Shared};
        }
        static ChunkInfo Describe(const Chunk& chunk) {
            ChunkInfo info;
            info.Id = chunk.Id; info.Generation = chunk.Generation;
            info.Layout = chunk.Shape->Id; info.Group = chunk.Owner->Id;
            info.Capacity = chunk.Shape->Capacity; info.AllocationBytes = chunk.Shape->Bytes;
            info.Entities = {chunk.Entities(), chunk.Count}; info.Types = chunk.Shape->Types;
            info.Tags = chunk.Owner->Tags; info.Shared = chunk.Owner->Shared;
            info.GroupIdentity = {chunk.Owner->Id, chunk.Owner->Generation}; info.EntityEnabled = chunk.EntityBits;
            info.MaskVersion = chunk.MaskVersion;
            info.Columns.reserve(chunk.Shape->Types.size());
            for (size_t column = 0; column < chunk.Shape->Types.size(); ++column) {
                info.Columns.push_back({chunk.Shape->Descriptors[column], chunk.Slot(column, 0), chunk.Versions[column].load(), &chunk.Versions[column]});
            }
            info.ComponentEnabled.reserve(chunk.MaskTypes.size());
            for (size_t mask = 0; mask < chunk.MaskTypes.size(); ++mask) info.ComponentEnabled.push_back({chunk.MaskTypes[mask], chunk.Mask(mask)});
            return info;
        }
        void TrimHistory() noexcept {
            constexpr size_t RetainedEvents = 4096;
            if (GroupEvents.size() <= RetainedEvents * 2) return;
            const size_t removed = GroupEvents.size() - RetainedEvents;
            HistoryFloor = GroupEvents[removed - 1].Revision;
            GroupEvents.erase(GroupEvents.begin(), GroupEvents.begin() + static_cast<std::ptrdiff_t>(removed));
        }
        const MigrationPlan& Transition(const Record& record, TransitionKind operation, TypeId type, ResourceHandle resource = {}) {
            const auto& source = *record.Block->Owner;
            const TransitionKey transition{source.Id, operation, type, resource.Id};
            if (const auto found = Migrations.find(transition); found != Migrations.end()) { ++MigrationCacheHits; return found->second; }
            auto key = Key(record);
            switch (operation) {
            case TransitionKind::Add: key.Types.insert(std::lower_bound(key.Types.begin(), key.Types.end(), type), type); break;
            case TransitionKind::Remove: key.Types.erase(std::lower_bound(key.Types.begin(), key.Types.end(), type)); break;
            case TransitionKind::AddTag: key.Tags.insert(std::lower_bound(key.Tags.begin(), key.Tags.end(), type), type); break;
            case TransitionKind::RemoveTag: key.Tags.erase(std::lower_bound(key.Tags.begin(), key.Tags.end(), type)); break;
            case TransitionKind::Shared: {
                const auto position = std::lower_bound(key.Shared.begin(), key.Shared.end(), type, [](const auto& value, TypeId id) { return value.Type < id; });
                if (position != key.Shared.end() && position->Type == type) *position = {type, resource};
                else key.Shared.insert(position, {type, resource});
                break;
            }
            case TransitionKind::RemoveShared: std::erase_if(key.Shared, [type](const auto& value) { return value.Type == type; }); break;
            }
            auto& target = GetGroup(std::move(key));
            MigrationPlan plan{&target};
            for (TypeId targetType : target.Shape->Types) plan.SourceColumns.push_back(source.Shape->Column(targetType));
            for (size_t column = 0; column < source.Shape->Types.size(); ++column) {
                if (target.Shape->Column(source.Shape->Types[column]) == target.Shape->Types.size()) plan.RemovedColumns.push_back(column);
            }
            for (TypeId targetType : target.MaskTypes) {
                const auto found = std::find(source.MaskTypes.begin(), source.MaskTypes.end(), targetType);
                plan.SourceMasks.push_back(static_cast<size_t>(found - source.MaskTypes.begin()));
            }
            const auto inserted = Migrations.emplace(transition, std::move(plan));
            ++MigrationPlanBuilds;
            return inserted.first->second;
        }
        static void MoveCell(Chunk& source, size_t sourceColumn, size_t sourceRow, Chunk& target, size_t targetColumn, size_t targetRow) noexcept {
            const auto& type = source.Shape->Descriptors[sourceColumn]->Descriptor;
            if (type.Storage == StorageKind::Indirect) {
                auto* sourceSlot = static_cast<void**>(source.Slot(sourceColumn, sourceRow));
                std::construct_at(static_cast<void**>(target.Slot(targetColumn, targetRow)), std::exchange(*sourceSlot, nullptr));
            }
            else type.Relocate(target.Slot(targetColumn, targetRow), source.Slot(sourceColumn, sourceRow));
        }
        static void Install(OwnedValue& value, Chunk& target, size_t column, size_t row) noexcept {
            const auto& type = value.Type()->Descriptor;
            void* object = value.Release();
            if (type.Storage == StorageKind::Indirect) std::construct_at(static_cast<void**>(target.Slot(column, row)), object);
            else {
                type.Relocate(target.Slot(column, row), object);
                ::operator delete(object, std::align_val_t(type.Alignment));
            }
        }
        void EraseRow(Chunk& chunk, size_t row, bool destroy) noexcept {
            const size_t last = chunk.Count - 1;
            for (size_t column = 0; column < chunk.Shape->Types.size(); ++column) {
                if (destroy) chunk.DestroyCell(column, row);
                if (row != last) MoveCell(chunk, column, last, chunk, column, row);
                ++chunk.Versions[column];
            }
            if (row != last) {
                const EntityId moved = chunk.Entities()[last];
                chunk.Entities()[row] = moved;
                Records[moved.Index].Row = row;
                SetBit(chunk.EntityBits, row, Bit(chunk.EntityBits, last));
                for (size_t mask = 0; mask < chunk.MaskTypes.size(); ++mask) SetBit(chunk.Mask(mask), row, Bit(chunk.Mask(mask), last));
            }
            --chunk.Count;
            ++chunk.MaskVersion;
            chunk.Owner->FirstAvailable = std::min(chunk.Owner->FirstAvailable, chunk.GroupIndex);
        }
        void Migrate(EntityId id, const MigrationPlan& plan, OwnedValue* added = nullptr) {
            auto& record = *Get(id);
            Chunk& source = *record.Block;
            const size_t sourceRow = record.Row;
            Chunk& target = Destination(*plan.Target);
            const size_t row = target.Count;
            // Everything from this point is a no-throw ownership transfer.
            for (size_t column = 0; column < target.Shape->Types.size(); ++column) {
                const size_t from = plan.SourceColumns[column];
                if (from != source.Shape->Types.size()) MoveCell(source, from, sourceRow, target, column, row);
                else Install(*added, target, column, row);
                ++target.Versions[column];
            }
            for (size_t column : plan.RemovedColumns) source.DestroyCell(column, sourceRow);
            SetBit(target.EntityBits, row, Bit(source.EntityBits, sourceRow));
            for (size_t mask = 0; mask < plan.SourceMasks.size(); ++mask) {
                const size_t from = plan.SourceMasks[mask];
                SetBit(target.Mask(mask), row, from == source.MaskTypes.size() || Bit(source.Mask(from), sourceRow));
            }
            ++target.MaskVersion;
            std::construct_at(target.Entities() + row, id);
            ++target.Count;
            EraseRow(source, sourceRow, false);
            record.Block = &target;
            record.Row = row;
            ++Version;
        }
        EntityUuid GenerateUuid() {
            for (;;) {
                if (NextUuid == 0) NextUuid = 1;
                const EntityUuid candidate{0, NextUuid++};
                if (!ByUuid.contains(candidate)) return candidate;
            }
        }
        Result<EntityId> Create(Group& group, std::string_view name, EntityUuid uuid, std::vector<OwnedValue>* values = nullptr) {
            if (uuid == EntityUuid{}) uuid = GenerateUuid();
            if (const auto existing = ByUuid.find(uuid); existing != ByUuid.end()) return existing->second;
            std::string ownedName(name.empty() ? "Entity" : name);
            const bool reuse = !Free.empty();
            if (!reuse && Records.size() >= std::numeric_limits<uint32_t>::max()) throw std::length_error("Entity slot space is exhausted");
            if (!reuse && Records.size() == Records.capacity()) Records.reserve(std::max<size_t>(8, Records.size() * 2));
            Free.reserve(Records.capacity());
            Chunk& chunk = Destination(group);
            const uint32_t index = reuse ? Free.back() : static_cast<uint32_t>(Records.size());
            const EntityId id{index, reuse ? Records[index].Generation : 1};
            ByUuid.emplace(uuid, id);
            // Publication follows the last allocating operation.
            if (!reuse) Records.emplace_back(); else Free.pop_back();
            auto& record = Records[index];
            record.Block = &chunk;
            record.Row = chunk.Count;
            record.Uuid = uuid;
            record.Name.swap(ownedName);
            if (values) {
                for (size_t column = 0; column < values->size(); ++column) Install((*values)[column], chunk, column, chunk.Count);
            }
            std::construct_at(chunk.Entities() + chunk.Count, id);
            ++chunk.Count;
            for (size_t column = 0; column < chunk.Shape->Types.size(); ++column) ++chunk.Versions[column];
            chunk.DefaultMasks(chunk.Count - 1);
            ++Count;
            ++Version;
            if (uuid.High == 0 && uuid.Low >= NextUuid) NextUuid = uuid.Low + 1;
            return id;
        }
        void RetireOrRelease(uint32_t index) noexcept {
            auto& record = Records[index];
            record.Block = nullptr;
            record.Uuid = {};
            record.Name.clear();
            if (record.Generation >= Options.GenerationLimit) record.Retired = true;
            else {
                ++record.Generation;
                Free.push_back(index);
            }
        }
    };

    uint64_t WorldBorrow::StructureVersion() const noexcept {
        return m_Storage ? static_cast<const World::Impl*>(m_Storage.get())->Version : 0;
    }
    Result<BorrowedComponent> WorldBorrow::Lookup(EntityId entity, TypeId type) const {
        if (!Valid() || !m_Storage) return Error{ErrorCode::InvalidState, "BorrowLookup", "The borrowed World is no longer alive"};
        const auto& storage = *static_cast<const World::Impl*>(m_Storage.get());
        const auto* record = storage.Get(entity);
        if (!record) return Error{ErrorCode::InvalidEntity, "BorrowLookup", "The entity is not alive"};
        const auto& chunk = *record->Block;
        const size_t column = chunk.Shape->Column(type);
        if (column == chunk.Shape->Types.size()) return Error{ErrorCode::InvalidType, "BorrowLookup", "The entity does not contain the stored component"};
        return BorrowedComponent{chunk.Object(column, record->Row), {{chunk.Id, chunk.Generation}, type}};
    }
    Result<void> WorldBorrow::PublishWrites(std::span<const ChunkColumnKey> columns) const {
        if (!m_Storage) return Error{ErrorCode::InvalidState, "PublishWrites", "The World storage lease has expired"};
        return static_cast<World::Impl*>(m_Storage.get())->Publish(columns);
    }

    World::World(EcsContext& context, WorldOptions options) : m_Impl(std::make_shared<Impl>(context, options)) {
        m_Impl->Lifetime->Id = Id();
        m_Impl->Lifetime->Owner.store(this, std::memory_order_release);
    }
    World::~World() {
        if (const auto control = Context().TimelineSnapshot()) control->WorldDestroyed(*this);
        m_Impl->Lifetime->Owner.store(nullptr, std::memory_order_release);
    }
    EcsContext& World::Context() noexcept { return m_Impl->Context; }
    const EcsContext& World::Context() const noexcept { return m_Impl->Context; }
    TypeRegistry& World::Types() noexcept { return Context().Types(); }
    const TypeRegistry& World::Types() const noexcept { return Context().Types(); }
    WorldId World::Id() const noexcept { return m_Impl->Identity; }
    uint64_t World::StructureVersion() const noexcept { return m_Impl->Version; }
    uint64_t World::GroupRevision() const noexcept { return m_Impl->GroupVersion; }
    std::weak_ptr<const Detail::WorldLifetime> World::Lifetime() const noexcept { return m_Impl->Lifetime; }
    Result<WorldBorrow> World::AcquireBorrow() {
        if (!Context().IsMainThread()) return Error{ErrorCode::WrongThread, "AcquireBorrow", "A World borrow must start on the Context main thread"};
        if (m_Impl->InMaintenance) return Error{ErrorCode::Busy, "AcquireBorrow", "World storage maintenance is in progress"};
        if (m_Impl->ReadScopes || m_Impl->EditScope) return Error{ErrorCode::Busy, "AcquireBorrow", "A synchronous scope is holding this World"};
        return WorldBorrow(m_Impl->Lifetime, m_Impl);
    }
    Result<void> World::BeginScope(bool edit) {
        if (!Context().IsMainThread()) return Error{ErrorCode::WrongThread, "WorldScope", "Synchronous scopes require the Context main thread"};
        if (m_Impl->InMaintenance) return Error{ErrorCode::Busy, "WorldScope", "World storage maintenance is in progress"};
        if (m_Impl->Lifetime->BorrowCount.load() || m_Impl->EditScope || (edit && m_Impl->ReadScopes)) {
            return Error{ErrorCode::Busy, "WorldScope", "The World is already borrowed or held by an incompatible scope"};
        }
        if (edit) { m_Impl->EditScope = true; m_Impl->EditTouched = false; }
        else ++m_Impl->ReadScopes;
        return {};
    }
    Result<void> World::CheckEditAccess() const { return m_Impl->Check("CommitCommands"); }
    void World::MarkEditAccess() noexcept { if (m_Impl->EditScope) m_Impl->EditTouched = true; }
    void World::EndScope(bool edit) noexcept {
        if (!edit) { if (m_Impl->ReadScopes) --m_Impl->ReadScopes; return; }
        if (m_Impl->EditScope && m_Impl->EditTouched) {
            for (const auto& [key, group] : m_Impl->Groups) for (const auto& chunk : group->Chunks) {
                for (size_t column = 0; column < chunk->Shape->Types.size(); ++column) chunk->Versions[column].fetch_add(1, std::memory_order_release);
            }
        }
        m_Impl->EditScope = false;
        m_Impl->EditTouched = false;
    }
    bool World::IsAlive(EntityId id) const noexcept { return m_Impl->Get(id) != nullptr; }
    size_t World::EntityCount() const noexcept { return m_Impl->Count; }
    EntityId World::Find(EntityUuid uuid) const {
        const auto found = m_Impl->ByUuid.find(uuid);
        return found == m_Impl->ByUuid.end() ? EntityId{} : found->second;
    }
    EntityId World::FindByIndex(uint32_t index) const noexcept {
        if (index >= m_Impl->Records.size()) return {};
        const auto& record = m_Impl->Records[index];
        return record.Block ? EntityId{index, record.Generation} : EntityId{};
    }
    EntityUuid World::Uuid(EntityId id) const noexcept {
        const auto* record = m_Impl->Get(id);
        return record ? record->Uuid : EntityUuid{};
    }
    std::string_view World::Name(EntityId id) const noexcept {
        const auto* record = m_Impl->Get(id);
        return record ? std::string_view(record->Name) : std::string_view("Entity");
    }
    Result<EntityId> World::CreateEmpty(std::string_view name, EntityUuid uuid) {
        if (auto access = m_Impl->Check("CreateEmpty"); !access) return access.GetError();
        try { return m_Impl->Create(m_Impl->GetGroup({}), name, uuid); }
        catch (const std::exception& exception) { return Failure("CreateEmpty", exception); }
    }
    Result<void> World::Destroy(EntityId id) {
        if (auto access = m_Impl->Check("Destroy"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return {};
        m_Impl->ByUuid.erase(record->Uuid);
        m_Impl->EraseRow(*record->Block, record->Row, true);
        m_Impl->RetireOrRelease(id.Index);
        --m_Impl->Count;
        ++m_Impl->Version;
        return {};
    }
    Result<void> World::Clear() {
        if (auto access = m_Impl->Check("Clear"); !access) return access;
        if (m_Impl->StorageGeneration == std::numeric_limits<uint64_t>::max()) return Error{ErrorCode::InvalidState, "Clear", "Storage generation space is exhausted"};
        try { ReserveExtra(m_Impl->GroupEvents, m_Impl->Groups.size()); }
        catch (const std::exception& exception) { return Failure("Clear", exception); }
        for (const auto& [key, group] : m_Impl->Groups) {
            m_Impl->GroupEvents.push_back({++m_Impl->GroupVersion, {group->Id, group->Generation}, false});
        }
        m_Impl->Migrations.clear();
        m_Impl->ChunksById.clear();
        m_Impl->GroupsById.clear();
        m_Impl->Groups.clear();
        ++m_Impl->StorageGeneration;
        m_Impl->NextGroup = m_Impl->NextChunk = 1;
        m_Impl->TrimHistory();
        m_Impl->Free.clear();
        for (uint32_t index = 0; index < m_Impl->Records.size(); ++index) {
            if (!m_Impl->Records[index].Retired) m_Impl->RetireOrRelease(index);
        }
        m_Impl->ByUuid.clear();
        m_Impl->Count = 0;
        ++m_Impl->Version;
        return {};
    }
    Result<void> World::SetName(EntityId id, std::string_view name) {
        if (auto access = m_Impl->Check("SetName"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "SetName", "The entity is not alive"};
        try {
            std::string replacement(name.empty() ? "Entity" : name);
            record->Name.swap(replacement);
            return {};
        }
        catch (const std::exception& exception) { return Failure("SetName", exception); }
    }
    std::vector<EntityId> World::Entities() const {
        std::vector<EntityId> result;
        result.reserve(m_Impl->Count);
        for (uint32_t index = 0; index < m_Impl->Records.size(); ++index) {
            const auto& record = m_Impl->Records[index];
            if (record.Block) result.push_back({index, record.Generation});
        }
        return result;
    }
    Result<void> World::Set(EntityId id, OwnedValue&& value) {
        if (auto access = m_Impl->Check("Set"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "Set", "The entity is not alive"};
        if (!value || !value.Type() || !Types().Owns(*value.Type())) return Error{ErrorCode::InvalidType, "Set", "The value must belong to this Context"};
        const TypeId type = value.Type()->Id;
        auto& chunk = *record->Block;
        const size_t column = chunk.Shape->Column(type);
        if (column != chunk.Shape->Types.size()) {
            chunk.DestroyCell(column, record->Row);
            Impl::Install(value, chunk, column, record->Row);
            ++chunk.Versions[column];
            ++m_Impl->Version;
            return {};
        }
        try {
            m_Impl->Migrate(id, m_Impl->Transition(*record, Impl::TransitionKind::Add, type), &value);
            return {};
        }
        catch (const std::exception& exception) { return Failure("Set", exception); }
    }
    Result<void> World::AddDefault(EntityId id, TypeId type) {
        if (auto access = m_Impl->Check("AddDefault"); !access) return access;
        if (!IsAlive(id)) return Error{ErrorCode::InvalidEntity, "AddDefault", "The entity is not alive"};
        const auto* registered = Types().Find(type);
        if (!registered) return Error{ErrorCode::InvalidType, "AddDefault", "The type is not registered in this Context"};
        if (registered->Descriptor.Storage == StorageKind::Tag) return SetTag(id, type);
        auto value = OwnedValue::Default(*registered);
        if (!value) return value.GetError();
        return Set(id, std::move(value).Value());
    }
    Result<void> World::Remove(EntityId id, TypeId type) {
        if (auto access = m_Impl->Check("Remove"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "Remove", "The entity is not alive"};
        const auto* registered = Types().Find(type);
        if (!registered) return Error{ErrorCode::InvalidType, "Remove", "The type is not registered in this Context"};
        if (registered->Descriptor.Storage == StorageKind::Tag) return SetTag(id, type, false);
        if (record->Block->Shape->Column(type) == record->Block->Shape->Types.size()) return {};
        try {
            m_Impl->Migrate(id, m_Impl->Transition(*record, Impl::TransitionKind::Remove, type));
            return {};
        }
        catch (const std::exception& exception) { return Failure("Remove", exception); }
    }
    Result<void> World::SetTag(EntityId id, TypeId type, bool present) {
        if (auto access = m_Impl->Check("SetTag"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "SetTag", "The entity is not alive"};
        const auto* registered = Types().Find(type);
        if (!registered || registered->Descriptor.Storage != StorageKind::Tag) return Error{ErrorCode::InvalidType, "SetTag", "Only explicit Tag types can be used as tags"};
        if (Has(id, type) == present) return {};
        try {
            m_Impl->Migrate(id, m_Impl->Transition(*record, present ? Impl::TransitionKind::AddTag : Impl::TransitionKind::RemoveTag, type));
            return {};
        }
        catch (const std::exception& exception) { return Failure("SetTag", exception); }
    }
    Result<void> World::SetShared(EntityId id, SharedBinding binding) {
        if (auto access = m_Impl->Check("SetShared"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "SetShared", "The entity is not alive"};
        const auto* type = Types().Find(binding.Type);
        const auto* resource = Context().Resources().Find(binding.Object);
        if (!type || !resource || type->Descriptor.NativeKey != resource->NativeKey || type->Descriptor.Storage == StorageKind::Tag) {
            return Error{ErrorCode::InvalidType, "SetShared", "The shared handle and type must belong to this Context and refer to the same native type"};
        }
        if (Shared(id, binding.Type) == binding.Object) return {};
        try {
            m_Impl->Migrate(id, m_Impl->Transition(*record, Impl::TransitionKind::Shared, binding.Type, binding.Object));
            return {};
        }
        catch (const std::exception& exception) { return Failure("SetShared", exception); }
    }
    Result<void> World::RemoveShared(EntityId id, TypeId type) {
        if (auto access = m_Impl->Check("RemoveShared"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "RemoveShared", "The entity is not alive"};
        if (!Types().Find(type)) return Error{ErrorCode::InvalidType, "RemoveShared", "The type is not registered in this Context"};
        if (!Shared(id, type)) return {};
        try {
            m_Impl->Migrate(id, m_Impl->Transition(*record, Impl::TransitionKind::RemoveShared, type));
            return {};
        }
        catch (const std::exception& exception) { return Failure("RemoveShared", exception); }
    }
    ResourceHandle World::Shared(EntityId id, TypeId type) const {
        const auto* record = m_Impl->Get(id);
        if (!record) return {};
        for (const auto& binding : record->Block->Owner->Shared) if (binding.Type == type) return binding.Object;
        return {};
    }
    Result<void> World::SetEnabled(EntityId id, bool enabled) {
        if (auto access = m_Impl->Check("SetEnabled"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "SetEnabled", "The entity is not alive"};
        if (Bit(record->Block->EntityBits, record->Row) != enabled) {
            SetBit(record->Block->EntityBits, record->Row, enabled);
            ++record->Block->MaskVersion;
            ++m_Impl->Version;
        }
        return {};
    }
    Result<void> World::SetComponentEnabled(EntityId id, TypeId type, bool enabled) {
        if (auto access = m_Impl->Check("SetComponentEnabled"); !access) return access;
        auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "SetComponentEnabled", "The entity is not alive"};
        auto& chunk = *record->Block;
        const size_t mask = chunk.MaskIndex(type);
        if (mask == chunk.MaskTypes.size()) return Error{ErrorCode::InvalidType, "SetComponentEnabled", "The entity does not contain this component or tag"};
        if (Bit(chunk.Mask(mask), record->Row) != enabled) {
            SetBit(chunk.Mask(mask), record->Row, enabled);
            ++chunk.MaskVersion;
            ++m_Impl->Version;
        }
        return {};
    }
    bool World::IsEnabled(EntityId id) const noexcept {
        const auto* record = m_Impl->Get(id);
        return record && Bit(record->Block->EntityBits, record->Row);
    }
    bool World::IsComponentEnabled(EntityId id, TypeId type) const noexcept {
        const auto* record = m_Impl->Get(id);
        if (!record) return false;
        const auto& chunk = *record->Block;
        const size_t mask = chunk.MaskIndex(type);
        return mask != chunk.MaskTypes.size() && Bit(chunk.Mask(mask), record->Row);
    }
    Result<void> World::PublishWrite(EntityId id, TypeId type) {
        if (m_Impl->InMaintenance)
            return Error{ErrorCode::Busy, "PublishWrite", "World storage maintenance is in progress"};
        const auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "PublishWrite", "The entity is not alive"};
        const ChunkColumnKey key{{record->Block->Id, record->Block->Generation}, type};
        return PublishWrites(std::span<const ChunkColumnKey>(&key, 1));
    }
    Result<void> World::PublishWrites(std::span<const ChunkColumnKey> columns) {
        if (m_Impl->InMaintenance)
            return Error{ErrorCode::Busy, "PublishWrites", "World storage maintenance is in progress"};
        // Execution borrows keep chunk storage stable while writers publish completion.
        return m_Impl->Publish(columns);
    }
    Result<EntityId> World::Clone(EntityId id, std::string_view name, EntityUuid uuid) {
        if (auto access = m_Impl->Check("Clone"); !access) return access.GetError();
        const auto* record = m_Impl->Get(id);
        if (!record) return Error{ErrorCode::InvalidEntity, "Clone", "The entity is not alive"};
        if (uuid != EntityUuid{} && Find(uuid)) return Error{ErrorCode::InvalidArgument, "Clone", "The destination UUID is already in use"};
        try {
            std::vector<OwnedValue> copies;
            const auto& chunk = *record->Block;
            const size_t sourceRow = record->Row;
            copies.reserve(chunk.Shape->Types.size());
            for (size_t column = 0; column < chunk.Shape->Types.size(); ++column) {
                auto copy = OwnedValue::Copy(*chunk.Shape->Descriptors[column], chunk.Object(column, record->Row));
                if (!copy) return copy.GetError();
                copies.push_back(std::move(copy).Value());
            }
            auto created = m_Impl->Create(*chunk.Owner, name.empty() ? std::string_view(record->Name) : name, uuid, &copies);
            if (!created) return created;
            auto& target = *m_Impl->Get(created.Value());
            SetBit(target.Block->EntityBits, target.Row, Bit(chunk.EntityBits, sourceRow));
            for (size_t mask = 0; mask < chunk.MaskTypes.size(); ++mask) SetBit(target.Block->Mask(mask), target.Row, Bit(chunk.Mask(mask), sourceRow));
            ++target.Block->MaskVersion;
            return created;
        }
        catch (const std::exception& exception) { return Failure("Clone", exception); }
    }
    Result<CompactionStats> World::Compact(size_t maxColumnBytes) {
        if (!Context().IsMainThread()) return Error{ErrorCode::WrongThread, "Compact", "Compaction requires the Context main thread"};
        auto state = m_Impl;
        const auto lifetime = state->Lifetime;
        if (state->ReadScopes || state->EditScope || state->InMaintenance) {
            return Error{ErrorCode::Busy, "Compact", "A scope or storage maintenance is holding this World"};
        }
        if (const auto control = state->Context.TimelineSnapshot()) {
            auto synchronized = control->SynchronizeWorld(*this);
            if (!synchronized) return synchronized.GetError();
        }
        if (!lifetime->Owner.load(std::memory_order_acquire))
            return Error{ErrorCode::InvalidState, "Compact", "The World was destroyed while synchronizing tasks"};
        if (auto access = state->Check("Compact"); !access) return access.GetError();

        try {
            size_t chunkCount = 0;
            for (const auto& [key, group] : state->Groups) chunkCount += group->Chunks.size();
            ReserveExtra(state->Pool, chunkCount);
            ReserveExtra(state->GroupEvents, state->Groups.size());
        }
        catch (const std::exception& exception) { return Failure("Compact", exception); }

        if (!lifetime->Owner.load(std::memory_order_acquire))
            return Error{ErrorCode::InvalidState, "Compact", "The World was destroyed while preparing storage"};
        if (auto access = state->Check("Compact"); !access) return access.GetError();
        MaintenanceGuard maintenance(state->InMaintenance);
        CompactionStats result;
        bool removedGroup = false;
        for (auto groupIt = state->Groups.begin(); groupIt != state->Groups.end();) {
            auto& group = *groupIt->second;
            size_t rowBytes = sizeof(EntityId);
            for (const auto* type : group.Shape->Descriptors) rowBytes += type->Descriptor.SlotSize();

            size_t front = 0;
            size_t back = group.Chunks.size();
            while (front < back) {
                while (front < back && group.Chunks[front]->Count == group.Shape->Capacity) ++front;
                while (front < back && group.Chunks[back - 1]->Count == 0) --back;
                if (front + 1 >= back || rowBytes > maxColumnBytes - result.MovedBytes) break;

                auto& target = *group.Chunks[front];
                auto& source = *group.Chunks[back - 1];
                const size_t sourceRow = source.Count - 1;
                const size_t targetRow = target.Count;
                const EntityId moved = source.Entities()[sourceRow];

                for (size_t column = 0; column < group.Shape->Types.size(); ++column) {
                    Impl::MoveCell(source, column, sourceRow, target, column, targetRow);
                    ++source.Versions[column];
                    ++target.Versions[column];
                }
                std::construct_at(target.Entities() + targetRow, moved);
                SetBit(target.EntityBits, targetRow, Bit(source.EntityBits, sourceRow));
                for (size_t mask = 0; mask < group.MaskTypes.size(); ++mask) {
                    SetBit(target.Mask(mask), targetRow, Bit(source.Mask(mask), sourceRow));
                }
                ++target.Count;
                --source.Count;
                ++target.MaskVersion;
                ++source.MaskVersion;
                auto& record = state->Records[moved.Index];
                record.Block = &target;
                record.Row = targetRow;
                ++result.MovedRows;
                result.MovedBytes += rowBytes;
                if (source.Count == 0) --back;
            }

            for (auto chunkIt = group.Chunks.begin(); chunkIt != group.Chunks.end();) {
                auto& chunk = **chunkIt;
                if (chunk.Count != 0) { ++chunkIt; continue; }
                state->ChunksById[static_cast<size_t>(chunk.Id)] = nullptr;
                const size_t bytes = chunk.Memory.Bytes;
                if (bytes <= state->Options.ChunkBytes && bytes <= state->Options.ChunkPoolByteLimit - state->PoolBytes) {
                    state->Pool.push_back({chunk.Shape->Capacity, std::move(chunk.Memory)});
                    state->PoolBytes += bytes;
                }
                else {
                    result.ReturnedBytes += bytes;
                    state->ReturnedChunkBytes += bytes;
                }
                chunkIt = group.Chunks.erase(chunkIt);
                ++result.ReleasedChunks;
            }
            for (size_t index = 0; index < group.Chunks.size(); ++index) group.Chunks[index]->GroupIndex = index;
            group.FirstAvailable = 0;

            if (group.Chunks.empty()) {
                state->GroupsById[static_cast<size_t>(group.Id)] = nullptr;
                state->GroupEvents.push_back({++state->GroupVersion, {group.Id, group.Generation}, false});
                groupIt = state->Groups.erase(groupIt);
                removedGroup = true;
            }
            else ++groupIt;
        }
        if (removedGroup) state->Migrations.clear();
        if (result.MovedRows || result.ReleasedChunks || removedGroup) ++state->Version;
        state->CompactionMovedRows += result.MovedRows;
        state->CompactionMovedBytes += result.MovedBytes;
        state->CompactionReleasedChunks += result.ReleasedChunks;
        state->TrimHistory();
        result.PoolBytes = state->PoolBytes;
        size_t capacity = 0;
        for (const auto& [key, group] : state->Groups)
            for (const auto& chunk : group->Chunks) capacity += chunk->Shape->Capacity;
        result.Occupancy = capacity ? static_cast<double>(state->Count) / static_cast<double>(capacity) : 0.0;
        if (!lifetime->Owner.load(std::memory_order_acquire))
            return Error{ErrorCode::InvalidState, "Compact", "The World was destroyed during storage maintenance"};
        return result;
    }
    Result<CompactionStats> World::CompactAll() { return Compact(std::numeric_limits<size_t>::max()); }
    Result<size_t> World::TrimPool() {
        if (!Context().IsMainThread()) return Error{ErrorCode::WrongThread, "TrimPool", "Pool trimming requires the Context main thread"};
        auto state = m_Impl;
        const auto lifetime = state->Lifetime;
        if (state->ReadScopes || state->EditScope || state->InMaintenance) {
            return Error{ErrorCode::Busy, "TrimPool", "A scope or storage maintenance is holding this World"};
        }
        if (const auto control = state->Context.TimelineSnapshot()) {
            auto synchronized = control->SynchronizeWorld(*this);
            if (!synchronized) return synchronized.GetError();
        }
        if (!lifetime->Owner.load(std::memory_order_acquire))
            return Error{ErrorCode::InvalidState, "TrimPool", "The World was destroyed while synchronizing tasks"};
        if (auto access = state->Check("TrimPool"); !access) return access.GetError();
        MaintenanceGuard maintenance(state->InMaintenance);
        const size_t bytes = state->PoolBytes;
        state->Pool.clear();
        state->PoolBytes = 0;
        state->ReturnedChunkBytes += bytes;
        if (!lifetime->Owner.load(std::memory_order_acquire))
            return Error{ErrorCode::InvalidState, "TrimPool", "The World was destroyed during storage maintenance"};
        return bytes;
    }
    bool World::Has(EntityId id, TypeId type) const {
        const auto* record = m_Impl->Get(id);
        if (!record) return false;
        const auto& chunk = *record->Block;
        return chunk.Shape->Column(type) != chunk.Shape->Types.size() || std::binary_search(chunk.Owner->Tags.begin(), chunk.Owner->Tags.end(), type);
    }
    const void* World::TryGet(EntityId id, TypeId type) const {
        if (m_Impl->InMaintenance) return nullptr;
        const auto* record = m_Impl->Get(id);
        if (!record) return nullptr;
        const size_t column = record->Block->Shape->Column(type);
        return column == record->Block->Shape->Types.size() ? nullptr : record->Block->Object(column, record->Row);
    }
    void* World::TryGet(EntityId id, TypeId type) {
        if (!Context().IsMainThread() || m_Impl->InMaintenance || m_Impl->ReadScopes || m_Impl->Lifetime->BorrowCount.load()) return nullptr;
        MarkEditAccess();
        auto* record = m_Impl->Get(id);
        if (!record) return nullptr;
        const size_t column = record->Block->Shape->Column(type);
        if (column == record->Block->Shape->Types.size()) return nullptr;
        // Mutable synchronous access conservatively publishes a possible write.
        ++record->Block->Versions[column];
        return record->Block->Object(column, record->Row);
    }
    std::vector<TypeId> World::ListTypes(EntityId id) const {
        const auto* record = m_Impl->Get(id);
        if (!record) return {};
        std::vector<TypeId> result = record->Block->Shape->Types;
        result.insert(result.end(), record->Block->Owner->Tags.begin(), record->Block->Owner->Tags.end());
        std::sort(result.begin(), result.end());
        return result;
    }
    EntityLocation World::Location(EntityId id) const noexcept {
        const auto* record = m_Impl->Get(id);
        if (!record) return {};
        return {record->Block->Id, record->Block->Generation, record->Block->Shape->Id, record->Block->Owner->Id, record->Row};
    }
    std::vector<ChunkInfo> World::Chunks() const {
        std::vector<ChunkInfo> result;
        for (const auto& [key, group] : m_Impl->Groups) {
            for (const auto& chunk : group->Chunks) result.push_back(Impl::Describe(*chunk));
        }
        return result;
    }
    GroupChanges World::ChangesSince(uint64_t revision) const {
        GroupChanges result;
        result.Revision = m_Impl->GroupVersion;
        if (revision < m_Impl->HistoryFloor || revision > result.Revision) {
            result.Reset = true;
            for (const auto& [key, group] : m_Impl->Groups) result.Added.push_back(Impl::Describe(*group));
            return result;
        }
        for (const auto& event : m_Impl->GroupEvents) {
            if (event.Revision <= revision) continue;
            if (event.Added) {
                if (const auto* group = m_Impl->Resolve(event.Handle)) result.Added.push_back(Impl::Describe(*group));
            }
            else result.Removed.push_back(event.Handle);
        }
        return result;
    }
    Result<GroupInfo> World::InspectGroup(GroupHandle group) const {
        const auto* found = m_Impl->Resolve(group);
        if (!found) return Error{ErrorCode::InvalidState, "InspectGroup", "The group handle has expired"};
        return Impl::Describe(*found);
    }
    Result<std::vector<ChunkInfo>> World::GetGroupChunks(GroupHandle group) const {
        const auto* found = m_Impl->Resolve(group);
        if (!found) return Error{ErrorCode::InvalidState, "GetGroupChunks", "The group handle has expired"};
        try {
            std::vector<ChunkInfo> result;
            result.reserve(found->Chunks.size());
            for (const auto& chunk : found->Chunks) if (chunk->Count) result.push_back(Impl::Describe(*chunk));
            return result;
        }
        catch (const std::exception& exception) { return Failure("GetGroupChunks", exception); }
    }
    Result<ChunkInfo> World::InspectChunk(ChunkHandle chunk) const {
        const auto* found = m_Impl->Resolve(chunk);
        if (!found) return Error{ErrorCode::InvalidState, "InspectChunk", "The chunk handle has expired"};
        try { return Impl::Describe(*found); }
        catch (const std::exception& exception) { return Failure("InspectChunk", exception); }
    }
    StorageStats World::Stats() const noexcept {
        StorageStats result;
        result.Entities = m_Impl->Count;
        result.Layouts = m_Impl->Layouts.size();
        result.Groups = m_Impl->Groups.size();
        for (const auto& [key, group] : m_Impl->Groups) {
            for (const auto& chunk : group->Chunks) {
                ++result.Chunks;
                result.EmptyChunks += chunk->Count == 0;
                result.Capacity += chunk->Shape->Capacity;
                result.AllocatedBytes += chunk->Shape->Bytes;
                result.MaskBytes += (chunk->EntityBits.size() + chunk->ComponentBits.size()) * sizeof(uint64_t);
            }
        }
        for (const auto& record : m_Impl->Records) result.RetiredSlots += record.Retired;
        result.MigrationPlanBuilds = m_Impl->MigrationPlanBuilds;
        result.MigrationCacheHits = m_Impl->MigrationCacheHits;
        result.PoolBlocks = m_Impl->Pool.size();
        result.PoolBytes = m_Impl->PoolBytes;
        result.PoolHits = m_Impl->PoolHits;
        result.ChunkAllocations = m_Impl->ChunkAllocations;
        result.ReturnedChunkBytes = m_Impl->ReturnedChunkBytes;
        result.CompactionMovedRows = m_Impl->CompactionMovedRows;
        result.CompactionMovedBytes = m_Impl->CompactionMovedBytes;
        result.CompactionReleasedChunks = m_Impl->CompactionReleasedChunks;
        return result;
    }
}
