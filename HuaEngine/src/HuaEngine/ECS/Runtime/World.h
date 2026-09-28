#pragma once

#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/Runtime/EcsContext.h"
#include "HuaEngine/ECS/Runtime/OwnedValue.h"
#include "HuaEngine/ECS/Runtime/Resources.h"

#include <limits>
#include <memory_resource>
#include <atomic>

namespace HE::Ecs {
    using WorldId = uint64_t;
    using LayoutId = uint64_t;
    using GroupId = uint64_t;
    using ChunkId = uint64_t;

    struct GroupHandle {
        GroupId Id = 0;
        uint64_t Generation = 0;
        auto operator<=>(const GroupHandle&) const = default;
    };
    struct ChunkHandle {
        ChunkId Id = 0;
        uint64_t Generation = 0;
        auto operator<=>(const ChunkHandle&) const = default;
    };
    struct ChunkColumnKey {
        ChunkHandle Chunk;
        TypeId Type = InvalidTypeId;
        auto operator<=>(const ChunkColumnKey&) const = default;
    };
    class World;
    struct BorrowedComponent {
        const void* Data = nullptr;
        ChunkColumnKey Column;
    };
    namespace Detail {
        struct WorldLifetime {
            WorldId Id = 0;
            std::atomic<World*> Owner{nullptr};
            std::atomic<size_t> BorrowCount{0};
        };
    }
    class WorldBorrow final {
    public:
        WorldBorrow() = default;
        ~WorldBorrow();
        WorldBorrow(const WorldBorrow&) = delete;
        WorldBorrow& operator=(const WorldBorrow&) = delete;
        WorldBorrow(WorldBorrow&& other) noexcept;
        WorldBorrow& operator=(WorldBorrow&& other) noexcept;
        [[nodiscard]] bool Valid() const noexcept;
        [[nodiscard]] WorldId Id() const noexcept;
        [[nodiscard]] uint64_t StructureVersion() const noexcept;
        [[nodiscard]] Result<BorrowedComponent> Lookup(EntityId entity, TypeId type) const;
        [[nodiscard]] Result<void> PublishWrites(std::span<const ChunkColumnKey> columns) const;
    private:
        friend class World;
        WorldBorrow(std::shared_ptr<Detail::WorldLifetime> lifetime, std::shared_ptr<void> storage);
        void Release() noexcept;
        std::shared_ptr<Detail::WorldLifetime> m_Lifetime;
        std::shared_ptr<void> m_Storage;
    };

    struct WorldOptions {
        size_t ChunkBytes = 16 * 1024;
        size_t ChunkPoolByteLimit = 1024 * 1024;
        uint32_t GenerationLimit = std::numeric_limits<uint32_t>::max();
        // The allocator must outlive the World and its active execution borrows.
        // Only chunk blocks use it.
        std::pmr::memory_resource* ChunkMemory = std::pmr::get_default_resource();
    };

    struct SharedBinding {
        TypeId Type = InvalidTypeId;
        ResourceHandle Object;
        auto operator<=>(const SharedBinding&) const = default;
    };

    struct ColumnInfo {
        const RegisteredType* Type = nullptr;
        // Direct columns contain objects; indirect columns contain owning pointers.
        const void* Data = nullptr;
        uint64_t Version = 0;
        const std::atomic<uint64_t>* VersionCounter = nullptr;
        [[nodiscard]] uint64_t CurrentVersion() const noexcept { return VersionCounter ? VersionCounter->load(std::memory_order_acquire) : Version; }
        [[nodiscard]] const void* At(size_t row) const noexcept {
            if (Type->Descriptor.Storage == StorageKind::Indirect) return static_cast<void* const*>(Data)[row];
            return static_cast<const std::byte*>(Data) + row * Type->Descriptor.Size;
        }
    };

    struct ComponentMaskInfo {
        TypeId Type = InvalidTypeId;
        std::span<const uint64_t> Words;
        [[nodiscard]] bool Enabled(size_t row) const noexcept { return (Words[row / 64] & (uint64_t{1} << (row % 64))) != 0; }
    };
    struct GroupInfo {
        GroupHandle Handle;
        LayoutId Layout = 0;
        std::span<const TypeId> Types;
        std::span<const TypeId> Tags;
        std::span<const SharedBinding> Shared;
    };
    struct GroupChanges {
        uint64_t Revision = 0;
        // Old observers receive a fresh snapshot after the bounded event log rolls over.
        bool Reset = false;
        std::vector<GroupInfo> Added;
        std::vector<GroupHandle> Removed;
    };

    // Views expire at the next successful structural mutation or World destruction.
    struct ChunkInfo {
        ChunkId Id = 0;
        uint64_t Generation = 0;
        LayoutId Layout = 0;
        GroupId Group = 0;
        size_t Capacity = 0;
        size_t AllocationBytes = 0;
        std::span<const EntityId> Entities;
        std::span<const TypeId> Types;
        std::span<const TypeId> Tags;
        std::span<const SharedBinding> Shared;
        std::vector<ColumnInfo> Columns;
        GroupHandle GroupIdentity;
        std::span<const uint64_t> EntityEnabled;
        std::vector<ComponentMaskInfo> ComponentEnabled;
        uint64_t MaskVersion = 0;
        [[nodiscard]] ChunkHandle Handle() const noexcept { return {Id, Generation}; }
        [[nodiscard]] bool Enabled(size_t row) const noexcept { return (EntityEnabled[row / 64] & (uint64_t{1} << (row % 64))) != 0; }
    };

    struct EntityLocation {
        ChunkId Chunk = 0;
        uint64_t ChunkGeneration = 0;
        LayoutId Layout = 0;
        GroupId Group = 0;
        size_t Row = 0;
    };

    struct StorageStats {
        size_t Entities = 0;
        size_t Layouts = 0;
        size_t Groups = 0;
        size_t Chunks = 0;
        size_t EmptyChunks = 0;
        size_t Capacity = 0;
        size_t AllocatedBytes = 0;
        size_t RetiredSlots = 0;
        size_t MaskBytes = 0;
        uint64_t MigrationPlanBuilds = 0;
        uint64_t MigrationCacheHits = 0;
        size_t PoolBlocks = 0;
        size_t PoolBytes = 0;
        uint64_t PoolHits = 0;
        uint64_t ChunkAllocations = 0;
        uint64_t ReturnedChunkBytes = 0;
        uint64_t CompactionMovedRows = 0;
        uint64_t CompactionMovedBytes = 0;
        uint64_t CompactionReleasedChunks = 0;
    };

    struct CompactionStats {
        size_t MovedRows = 0;
        size_t MovedBytes = 0;
        size_t ReleasedChunks = 0;
        size_t ReturnedBytes = 0;
        size_t PoolBytes = 0;
        double Occupancy = 0.0;
    };

    class World final {
    public:
        explicit World(EcsContext& context, WorldOptions options = {});
        ~World();
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;

        [[nodiscard]] EcsContext& Context() noexcept;
        [[nodiscard]] const EcsContext& Context() const noexcept;
        [[nodiscard]] TypeRegistry& Types() noexcept;
        [[nodiscard]] const TypeRegistry& Types() const noexcept;
        [[nodiscard]] WorldId Id() const noexcept;
        [[nodiscard]] uint64_t StructureVersion() const noexcept;
        [[nodiscard]] uint64_t GroupRevision() const noexcept;
        [[nodiscard]] std::weak_ptr<const Detail::WorldLifetime> Lifetime() const noexcept;
        [[nodiscard]] Result<WorldBorrow> AcquireBorrow();

        [[nodiscard]] Result<EntityId> CreateEmpty(std::string_view name = "Entity", EntityUuid uuid = {});
        [[nodiscard]] Result<void> Destroy(EntityId id);
        [[nodiscard]] Result<void> Clear();
        [[nodiscard]] bool IsAlive(EntityId id) const noexcept;
        [[nodiscard]] size_t EntityCount() const noexcept;
        [[nodiscard]] EntityId Find(EntityUuid uuid) const;
        [[nodiscard]] EntityId FindByIndex(uint32_t index) const noexcept;
        [[nodiscard]] EntityUuid Uuid(EntityId id) const noexcept;
        [[nodiscard]] std::string_view Name(EntityId id) const noexcept;
        [[nodiscard]] Result<void> SetName(EntityId id, std::string_view name);
        [[nodiscard]] std::vector<EntityId> Entities() const;

        [[nodiscard]] Result<void> Set(EntityId id, OwnedValue&& value);
        [[nodiscard]] Result<void> AddDefault(EntityId id, TypeId type);
        [[nodiscard]] Result<void> Remove(EntityId id, TypeId type);
        [[nodiscard]] Result<void> SetTag(EntityId id, TypeId type, bool present = true);
        [[nodiscard]] Result<void> SetShared(EntityId id, SharedBinding binding);
        [[nodiscard]] Result<void> RemoveShared(EntityId id, TypeId type);
        [[nodiscard]] ResourceHandle Shared(EntityId id, TypeId type) const;
        [[nodiscard]] Result<void> SetEnabled(EntityId id, bool enabled);
        [[nodiscard]] Result<void> SetComponentEnabled(EntityId id, TypeId type, bool enabled);
        [[nodiscard]] bool IsEnabled(EntityId id) const noexcept;
        [[nodiscard]] bool IsComponentEnabled(EntityId id, TypeId type) const noexcept;
        // Publish after a synchronous write that was made through a borrowed pointer.
        [[nodiscard]] Result<void> PublishWrite(EntityId id, TypeId type);
        [[nodiscard]] Result<void> PublishWrites(std::span<const ChunkColumnKey> columns);
        [[nodiscard]] Result<EntityId> Clone(EntityId id, std::string_view name = {}, EntityUuid uuid = {});
        [[nodiscard]] Result<CompactionStats> Compact(size_t maxColumnBytes = 1024 * 1024);
        [[nodiscard]] Result<CompactionStats> CompactAll();
        [[nodiscard]] Result<size_t> TrimPool();
        [[nodiscard]] bool Has(EntityId id, TypeId type) const;
        [[nodiscard]] void* TryGet(EntityId id, TypeId type);
        [[nodiscard]] const void* TryGet(EntityId id, TypeId type) const;
        [[nodiscard]] std::vector<TypeId> ListTypes(EntityId id) const;

        template<typename T, typename... Args>
        [[nodiscard]] Result<void> Emplace(EntityId id, Args&&... args) {
            if (!Context().IsMainThread()) return Error{ErrorCode::WrongThread, "Emplace", "Structural access requires the Context main thread"};
            if (!IsAlive(id)) return Error{ErrorCode::InvalidEntity, "Emplace", "The entity is not alive"};
            const auto* type = Types().Find<T>();
            if (!type) return Error{ErrorCode::InvalidType, "Emplace", "The component is not registered in this Context"};
            auto value = OwnedValue::Construct<T>(*type, std::forward<Args>(args)...);
            if (!value) return value.GetError();
            return Set(id, std::move(value).Value());
        }
        template<typename T>
        [[nodiscard]] T* TryGet(EntityId id) {
            const auto* type = Types().Find<T>();
            return type ? static_cast<T*>(TryGet(id, type->Id)) : nullptr;
        }
        template<typename T>
        [[nodiscard]] const T* TryGet(EntityId id) const {
            const auto* type = Types().Find<T>();
            return type ? static_cast<const T*>(TryGet(id, type->Id)) : nullptr;
        }
        template<typename T>
        [[nodiscard]] bool Has(EntityId id) const {
            const auto* type = Types().Find<T>();
            return type && Has(id, type->Id);
        }

        [[nodiscard]] EntityLocation Location(EntityId id) const noexcept;
        [[nodiscard]] std::vector<ChunkInfo> Chunks() const;
        [[nodiscard]] GroupChanges ChangesSince(uint64_t revision) const;
        [[nodiscard]] Result<GroupInfo> InspectGroup(GroupHandle group) const;
        [[nodiscard]] Result<std::vector<ChunkInfo>> GetGroupChunks(GroupHandle group) const;
        [[nodiscard]] Result<ChunkInfo> InspectChunk(ChunkHandle chunk) const;
        [[nodiscard]] StorageStats Stats() const noexcept;

    private:
        friend class WorldAccessScope;
        friend class Timeline;
        friend class WorldBorrow;
        [[nodiscard]] Result<void> CheckEditAccess() const;
        [[nodiscard]] Result<void> BeginScope(bool edit);
        void EndScope(bool edit) noexcept;
        void MarkEditAccess() noexcept;
        struct Impl;
        std::shared_ptr<Impl> m_Impl;
    };
}
