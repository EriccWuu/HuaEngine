#include "ECSTestSupport.h"
#include "ECSLifecycleFixtures.h"
#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"

#include <algorithm>
#include <array>
#include <map>
#include <functional>
#include <memory_resource>
#include <thread>
#include <unordered_map>

#if defined(HUAENGINE_ASAN_REQUIRED) && !defined(__SANITIZE_ADDRESS__)
#error The compaction target must actually enable AddressSanitizer.
#endif

namespace {
using namespace ECSTestSupport;
using namespace HE::Ecs;
using namespace ECSLifecycleFixtures;
using HE::EntityId;

struct CountingMemory final : std::pmr::memory_resource {
    size_t Allocations = 0;
    size_t Bytes = 0;
    size_t Returned = 0;
    std::unordered_map<void*, std::pair<size_t, size_t>> Live;
    std::function<void()> OnReturn;
    void* do_allocate(size_t bytes, size_t alignment) override {
        void* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
        Require(Live.emplace(result, std::pair{bytes, alignment}).second, "Expected fresh allocator ownership");
        ++Allocations;
        Bytes += bytes;
        return result;
    }
    void do_deallocate(void* memory, size_t bytes, size_t alignment) override {
        const auto found = Live.find(memory);
        Require(found != Live.end() && found->second == std::pair{bytes, alignment},
            "Expected block release to retain its allocation size and alignment");
        Live.erase(found);
        Bytes -= bytes;
        Returned += bytes;
        std::pmr::new_delete_resource()->deallocate(memory, bytes, alignment);
        auto callback = std::move(OnReturn);
        if (callback) callback();
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
};

template<typename T>
TypeId Register(EcsContext& context, const char* name, bool tag = false) {
    return Take(context.Types().Register<T>(TypeGuid::FromName(name), name, tag));
}

struct Saved {
    EntityId Id;
    HE::EntityUuid Uuid;
    std::string Name;
    std::string Text;
    std::vector<int> Values;
    int Owned = 0;
    const Immovable* Stable = nullptr;
    const IndirectAligned* Aligned = nullptr;
    bool Enabled = true;
    bool ComponentEnabled = true;
    GroupId Group = 0;
};

void VerifySaved(const World& world, std::span<const Saved> saved, TypeId nonpod) {
    size_t rows = 0;
    size_t bytes = 0;
    for (const auto& chunk : world.Chunks()) {
        bytes += chunk.AllocationBytes;
        rows += chunk.Entities.size();
        for (size_t row = 0; row < chunk.Entities.size(); ++row) {
            const auto location = world.Location(chunk.Entities[row]);
            Require(location.Chunk == chunk.Id && location.ChunkGeneration == chunk.Generation &&
                location.Row == row && location.Group == chunk.Group, "Expected exact location updates after compaction");
        }
    }
    Require(rows == saved.size() && rows == world.EntityCount() && bytes == world.Stats().AllocatedBytes,
        "Expected active chunk statistics to exclude pooled bytes");
    for (const auto& value : saved) {
        Require(world.IsAlive(value.Id) && world.Find(value.Uuid) == value.Id && world.FindByIndex(value.Id.Index) == value.Id &&
            world.Name(value.Id) == value.Name, "Expected compaction to preserve entity identity and name");
        const auto* object = world.TryGet<NonPod>(value.Id);
        Require(object && object->Text == value.Text && object->Values == value.Values && object->Reference,
            "Expected non-POD string, vector and resource ownership to survive compaction");
        Require(*world.TryGet<MoveOnly>(value.Id)->Value == value.Owned &&
            world.TryGet<Immovable>(value.Id) == value.Stable && world.TryGet<IndirectAligned>(value.Id) == value.Aligned,
            "Expected move-only values and indirect object addresses to remain valid");
        Require(reinterpret_cast<uintptr_t>(value.Aligned) % alignof(IndirectAligned) == 0,
            "Expected indirect over-alignment after compaction");
        Require(world.IsEnabled(value.Id) == value.Enabled && world.IsComponentEnabled(value.Id, nonpod) == value.ComponentEnabled &&
            world.Location(value.Id).Group == value.Group, "Expected masks and Group membership to survive compaction");
    }
}

void VerifyBudgetLifetimesAndChanged() {
    EcsContext context({.WorkerCount = 2});
    const auto nonpod = Register<NonPod>(context, "Compact.NonPod");
    const auto move = Register<MoveOnly>(context, "Compact.MoveOnly");
    const auto stable = Register<Immovable>(context, "Compact.Immovable");
    const auto aligned = Register<IndirectAligned>(context, "Compact.Aligned");
    const auto tag = Register<ExplicitTag>(context, "Compact.Tag", true);
    const auto shared = Register<Resource>(context, "Compact.Shared");
    const auto sharedA = Take(context.Resources().Register(std::make_shared<Resource>()));
    const auto sharedB = Take(context.Resources().Register(std::make_shared<Resource>()));
    CountingMemory memory;
    {
        World world(context, {.ChunkBytes = 2048, .ChunkPoolByteLimit = 8192, .ChunkMemory = &memory});
        std::vector<EntityId> ids;
        for (size_t index = 0; index < 180; ++index) {
            const auto id = Take(world.CreateEmpty("row-" + std::to_string(index), {0xc07ac7ULL, index + 1}));
            Check(world.Emplace<NonPod>(id, static_cast<int>(index)));
            Check(world.Emplace<MoveOnly>(id, static_cast<int>(index + 1000)));
            Check(world.Emplace<Immovable>(id, static_cast<int>(index)));
            Check(world.Emplace<IndirectAligned>(id, static_cast<int>(index)));
            if (index >= 120) Check(world.SetTag(id, tag));
            Check(world.SetShared(id, {shared, index >= 60 && index < 120 ? sharedB : sharedA}));
            ids.push_back(id);
        }
        std::vector<Saved> saved;
        for (size_t index = 0; index < ids.size(); ++index) {
            if (index % 3 != 0) { Check(world.Destroy(ids[index])); continue; }
            const auto id = ids[index];
            Check(world.SetEnabled(id, index % 12 != 0));
            Check(world.SetComponentEnabled(id, nonpod, index % 15 != 0));
            const auto* value = world.TryGet<NonPod>(id);
            saved.push_back({id, world.Uuid(id), std::string(world.Name(id)), value->Text, value->Values,
                *world.TryGet<MoveOnly>(id)->Value, world.TryGet<Immovable>(id), world.TryGet<IndirectAligned>(id),
                world.IsEnabled(id), world.IsComponentEnabled(id, nonpod), world.Location(id).Group});
        }
        VerifySaved(world, saved, nonpod);
        size_t rowBytes = sizeof(EntityId);
        for (const auto type : {nonpod, move, stable, aligned}) rowBytes += context.Types().Find(type)->Descriptor.SlotSize();
        auto changed = Take(Query::Create(context, {.Columns = {{nonpod}}, .ChangedTypes = {nonpod},
            .IncludeDisabledEntities = true, .IgnoreComponentEnabled = true}));
        ChangedState observer;
        size_t visited = 0;
        Check(changed.Each(world, [&](QueryRow&) -> Result<void> { ++visited; return {}; }, &observer));
        Require(visited == saved.size(), "Expected initial Changed observation of every row");
        const auto zero = Take(world.Compact(0));
        Require(zero.MovedRows == 0 && zero.MovedBytes == 0, "Expected zero-budget compaction to move no rows");
        const auto below = Take(world.Compact(rowBytes - 1));
        Require(below.MovedRows == 0 && below.MovedBytes == 0, "Expected an over-budget row to be skipped");
        const auto exact = Take(world.Compact(rowBytes));
        Require(exact.MovedRows == 1 && exact.MovedBytes == rowBytes,
            "Expected an exact row budget to charge identity plus physical component slots");
        VerifySaved(world, saved, nonpod);
        std::vector<ChunkHandle> oldHandles;
        for (const auto& chunk : world.Chunks()) oldHandles.push_back(chunk.Handle());
        const auto before = world.Stats();
        const auto full = Take(world.CompactAll());
        Require(full.MovedRows > 0 && full.MovedBytes == full.MovedRows * rowBytes && world.Stats().Chunks < before.Chunks,
            "Expected complete compaction to merge sparse chunks within each Group");
        Require(full.Occupancy > 0 && full.Occupancy <= 1 && full.PoolBytes == world.Stats().PoolBytes,
            "Expected physical pool and occupancy reporting");
        VerifySaved(world, saved, nonpod);
        size_t expired = 0;
        for (const auto handle : oldHandles) expired += !world.InspectChunk(handle) ? 1 : 0;
        Require(expired > 0, "Expected removed chunk handles to become invalid");
        visited = 0;
        Check(changed.Each(world, [&](QueryRow&) -> Result<void> { ++visited; return {}; }, &observer));
        Require(visited > 0, "Expected compaction writes to remain visible to Changed observers");
        const auto cache = changed.Stats();
        Check(changed.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &observer));
        Require(changed.Stats().GroupMatchCount == cache.GroupMatchCount && changed.Stats().LayoutBindCount == cache.LayoutBindCount,
            "Expected stable compaction output to retain hot query bindings");
        const auto settled = Take(world.CompactAll());
        Require(settled.MovedRows == 0, "Expected full compaction to reach a stable fixed point");
        for (const auto& value : saved) {
            const auto expected = value.Uuid.Low > 60 && value.Uuid.Low <= 120 ? sharedB : sharedA;
            Require(world.Shared(value.Id, shared) == expected && world.Has(value.Id, tag) == (value.Uuid.Low > 120),
                "Expected no cross-Group Tag or shared-resource merging");
        }
        Require(memory.Bytes == world.Stats().AllocatedBytes + world.Stats().PoolBytes,
            "Expected pooled blocks to remain charged to their actual allocator");
    }
    Require(memory.Live.empty() && memory.Bytes == 0, "Expected World destruction to return active and pooled blocks");
    Require(NonPod::Live == 0 && MoveOnly::Live == 0 && Immovable::Live == 0 && IndirectAligned::Live == 0,
        "Expected exact component lifetime cleanup after compaction");
}

struct alignas(256) Huge { std::array<std::byte, 20 * 1024> Data{}; int Value = 47; };

void VerifyPoolAndAccess() {
    EcsContext context({.WorkerCount = 1});
    const auto plain = Register<PlainValue>(context, "Compact.Plain");
    Register<Huge>(context, "Compact.Huge");
    CountingMemory memory;
    {
        World world(context, {.ChunkBytes = 4096, .ChunkPoolByteLimit = 8192, .ChunkMemory = &memory});
        const auto id = Take(world.CreateEmpty());
        Check(world.Emplace<PlainValue>(id));
        Take(world.Compact(0));
        Require(world.Stats().PoolBytes > 0 && world.Stats().PoolBytes <= 8192,
            "Expected empty ordinary chunks to enter the bounded block pool");
        const auto hits = world.Stats().PoolHits;
        const auto allocations = memory.Allocations;
        const auto reused = Take(world.CreateEmpty());
        Require(world.Stats().PoolHits > hits && memory.Allocations == allocations,
            "Expected compatible empty-layout block reuse without allocator work");
        Check(world.Destroy(reused));
        Take(world.Compact(0));
        const auto pooled = world.Stats().PoolBytes;
        const auto returned = memory.Returned;
        Require(Take(world.TrimPool()) == pooled && world.Stats().PoolBytes == 0 && memory.Returned == returned + pooled,
            "Expected TrimPool to report bytes actually returned to the allocator");
        {
            auto borrow = Take(world.AcquireBorrow());
            const auto blocked = world.CompactAll();
            Require(!blocked && blocked.GetError().Code == ErrorCode::Busy, "Expected compaction to reject active borrows");
        }
        {
            auto read = Take(WorldReadScope::Acquire(world));
            const auto blocked = world.CompactAll();
            Require(!blocked && blocked.GetError().Code == ErrorCode::Busy, "Expected compaction to reject a read scope");
        }
        {
            auto edit = Take(WorldEditScope::Acquire(world));
            const auto blocked = world.CompactAll();
            Require(!blocked && blocked.GetError().Code == ErrorCode::Busy, "Expected compaction to reject an edit scope");
        }
        bool wrongThread = false;
        std::thread worker([&] { auto result = world.Compact(); wrongThread = !result && result.GetError().Code == ErrorCode::WrongThread; });
        worker.join();
        Require(wrongThread, "Expected compaction to remain a main-thread structural action");
        auto query = Take(Query::Create(context, {.Columns = {{plain, Presence::Required, AccessMode::Write}}}));
        {
            Timeline timeline(context, {.Mode = TimelineMode::Serial});
            const auto task = Take(timeline.Submit(query, world, [](TaskBatch& batch) -> Result<void> {
                auto values = Take(batch.View().Column<PlainValue>(0));
                for (size_t row = 0; row < values.Size(); ++row) values.At(row).Value = 89;
                return {};
            }));
            Take(world.CompactAll());
            Require(task.IsComplete() && world.TryGet<PlainValue>(id)->Value == 89,
                "Expected compaction to drain prior access to its World");
        }
        const auto huge = Take(world.CreateEmpty());
        Check(world.Emplace<Huge>(huge));
        Take(world.CompactAll());
        Take(world.TrimPool());
        Check(world.Destroy(huge));
        const auto released = Take(world.Compact(0));
        Require(released.ReturnedBytes > 16 * 1024 && world.Stats().PoolBytes == 0,
            "Expected oversized dedicated blocks to bypass the ordinary pool");
    }
    Require(memory.Live.empty(), "Expected every pooled or dedicated block to be released");
    {
        World world(context, {.ChunkBytes = 4096, .ChunkPoolByteLimit = 1, .ChunkMemory = &memory});
        const auto id = Take(world.CreateEmpty());
        Check(world.Emplace<PlainValue>(id));
        const auto result = Take(world.Compact(0));
        Require(result.ReturnedBytes > 0 && world.Stats().PoolBytes == 0,
            "Expected blocks larger than the configured pool limit to return immediately");
    }
    Require(memory.Live.empty(), "Expected pool-limit case to leave no allocation behind");
}

void VerifyMaintenanceOwnerDestruction() {
    EcsContext context({.WorkerCount = 1});
    const auto plain = Register<PlainValue>(context, "Compact.DestroyOwner");
    for (const bool trim : {false, true}) {
        auto world = std::make_unique<World>(context);
        const auto id = Take(world->CreateEmpty());
        Check(world->Emplace<PlainValue>(id));
        auto query = Take(Query::Create(context, {.Columns = {{plain}}}));
        Timeline timeline(context, {.Mode = TimelineMode::Serial});
        const auto task = Take(timeline.Submit(query, *world, [&](TaskBatch& batch) -> Result<void> {
            const auto values = Take(batch.View().Column<const PlainValue>(0));
            const auto* native = values.TryGet(0);
            world.reset();
            Require(native && native->Value == 37, "Expected execution lease to preserve native data after World destruction");
            return {};
        }));
        World* owner = world.get();
        if (trim) {
            const auto result = owner->TrimPool();
            Require(!result && result.GetError().Code == ErrorCode::InvalidState,
                "Expected TrimPool to reject a World destroyed while synchronizing tasks");
        } else {
            const auto result = owner->CompactAll();
            Require(!result && result.GetError().Code == ErrorCode::InvalidState,
                "Expected Compact to reject a World destroyed while synchronizing tasks");
        }
        Require(!world && task.IsComplete() && !context.HasScheduledWork(),
            "Expected maintenance owner destruction to finish all task bookkeeping");
        (void)timeline.Finish();
    }
    struct ReleaseWorldOnDestruction {
        std::unique_ptr<World>* Owner;
        int* Destroyed;
        ~ReleaseWorldOnDestruction() { ++*Destroyed; Owner->reset(); }
    };
    for (const bool trim : {false, true}) {
        auto world = std::make_unique<World>(context);
        const auto id = Take(world->CreateEmpty());
        Check(world->Emplace<PlainValue>(id));
        auto query = Take(Query::Create(context, {.Columns = {{plain}}}));
        Timeline timeline(context, {.Mode = TimelineMode::Serial});
        int destroyed = 0;
        auto payload = std::unique_ptr<ReleaseWorldOnDestruction>(new ReleaseWorldOnDestruction{&world, &destroyed});
        const auto task = Take(timeline.Submit(query, *world,
            [payload = std::move(payload)](TaskBatch& batch) -> Result<void> {
                Require(payload->Owner->get() && Take(batch.View().Column<const PlainValue>(0)).At(0).Value == 37,
                    "Expected a successful task to retain its World until its captured payload is released");
                return {};
            }));
        World* owner = world.get();
        if (trim) {
            const auto result = owner->TrimPool();
            Require(!result && result.GetError().Code == ErrorCode::InvalidState,
                "Expected TrimPool to detect World destruction by successful task cleanup");
        } else {
            const auto result = owner->CompactAll();
            Require(!result && result.GetError().Code == ErrorCode::InvalidState,
                "Expected Compact to detect World destruction by successful task cleanup");
        }
        Require(!world && destroyed == 1 && task.Status() == TaskStatus::Succeeded && !context.HasScheduledWork(),
            "Expected one payload destruction after successful task execution and complete maintenance cleanup");
        Check(timeline.Finish());
    }
    CountingMemory memory;
    auto world = std::make_unique<World>(context, WorldOptions{.ChunkBytes = 4096, .ChunkMemory = &memory});
    const auto id = Take(world->CreateEmpty());
    Check(world->Emplace<PlainValue>(id));
    Take(world->Compact(0));
    Require(world->Stats().PoolBytes != 0, "Expected a pooled block for the allocator callback regression");
    memory.OnReturn = [&] { world.reset(); };
    World* owner = world.get();
    const auto result = owner->TrimPool();
    Require(!result && result.GetError().Code == ErrorCode::InvalidState && !world && memory.Live.empty(),
        "Expected allocator-driven World destruction during TrimPool to retain storage until maintenance exits");
}
}

int main() {
    VerifyBudgetLifetimesAndChanged();
    VerifyPoolAndAccess();
    VerifyMaintenanceOwnerDestruction();
#if defined(__SANITIZE_ADDRESS__)
    std::cout << "AddressSanitizer instrumentation is enabled\n";
#endif
    std::cout << "ECSCompactionSmoke passed\n";
}
