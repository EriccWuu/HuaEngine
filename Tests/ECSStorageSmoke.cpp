#include "ECSLifecycleFixtures.h"
#include "HuaEngine/ECS/Runtime/Commands.h"
#include "HuaEngine/ECS/Runtime/World.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory_resource>
#include <set>
#include <thread>
#include <unordered_map>

#if defined(HUAENGINE_ASAN_REQUIRED) && !defined(__SANITIZE_ADDRESS__)
#error The standalone storage target must actually enable AddressSanitizer.
#endif

namespace {
using namespace HE::Ecs;
using namespace ECSLifecycleFixtures;
using HE::EntityId;
using HE::EntityUuid;

void Require(bool condition, const char* message) {
	if (!condition) { std::cerr << "[ECSStorageSmoke] " << message << '\n'; std::exit(1); }
}

template<typename T>
T Take(Result<T> result) {
	if (!result) {
		std::cerr << "[ECSStorageSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n';
		std::exit(1);
	}
	return std::move(result).Value();
}

void Check(Result<void> result) {
	if (!result) {
		std::cerr << "[ECSStorageSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n';
		std::exit(1);
	}
}

template<typename T>
TypeId Register(EcsContext& context, const char* name, bool tag = false) {
	return Take(context.Types().Register<T>(TypeGuid::FromName(name), name, tag));
}

template<typename T>
void NoLiveObjects() {
	Require(T::Live == 0 && T::Constructed == T::Destroyed, "Expected exact destruction of every successfully constructed lifetime");
}

struct CountingMemory final : std::pmr::memory_resource {
	bool FailNext = false;
	size_t Allocations = 0;
	size_t Bytes = 0;
	std::unordered_map<void*, size_t> Live;

	void* do_allocate(size_t bytes, size_t alignment) override {
		if (FailNext) { FailNext = false; throw std::bad_alloc(); }
		void* memory = std::pmr::new_delete_resource()->allocate(bytes, alignment);
		Live.emplace(memory, bytes);
		++Allocations;
		Bytes += bytes;
		return memory;
	}
	void do_deallocate(void* memory, size_t bytes, size_t alignment) override {
		const auto found = Live.find(memory);
		Require(found != Live.end() && found->second == bytes, "Expected every chunk deallocation to match its allocation");
		Bytes -= bytes;
		Live.erase(found);
		std::pmr::new_delete_resource()->deallocate(memory, bytes, alignment);
	}
	bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }
};

struct EmptyLifetime {
	inline static int Live = 0;
	inline static int Constructed = 0;
	inline static int Destroyed = 0;
	EmptyLifetime() noexcept { ++Live; ++Constructed; }
	EmptyLifetime(const EmptyLifetime&) noexcept : EmptyLifetime() {}
	EmptyLifetime(EmptyLifetime&&) noexcept : EmptyLifetime() {}
	~EmptyLifetime() noexcept { --Live; ++Destroyed; }
};

struct alignas(256) Oversized {
	std::array<std::byte, 20 * 1024> Padding{};
	int Value = 91;
};

void VerifyLocations(const World& world) {
	size_t rows = 0;
	size_t capacity = 0;
	size_t bytes = 0;
	size_t empty = 0;
	std::set<std::pair<uint32_t, uint32_t>> seen;
	std::unordered_map<GroupId, LayoutId> groupLayouts;
	for (const auto& chunk : world.Chunks()) {
		Require(chunk.Id != 0 && chunk.Generation != 0 && chunk.Capacity > 0, "Expected identified chunks with nonzero capacity");
		Require(chunk.Entities.size() <= chunk.Capacity, "Expected live chunk rows to fit capacity");
		Require(chunk.Types.size() == chunk.Columns.size(), "Expected one column per ordinary component type");
		const auto [entry, inserted] = groupLayouts.emplace(chunk.Group, chunk.Layout);
		Require(inserted || entry->second == chunk.Layout, "Expected each Group to contain exactly one physical layout");
		for (const auto& column : chunk.Columns) {
			Require(column.Type && column.Type->Descriptor.Storage != StorageKind::Tag && column.Data,
				"Expected tags to have no data columns");
			Require(reinterpret_cast<uintptr_t>(column.Data) % column.Type->Descriptor.SlotAlignment() == 0,
				"Expected column allocation alignment");
			for (size_t row = 0; row < chunk.Entities.size(); ++row) {
				const auto* value = column.At(row);
				Require(value && reinterpret_cast<uintptr_t>(value) % column.Type->Descriptor.Alignment == 0,
					"Expected each direct or indirect object to satisfy native alignment");
				Require(value == world.TryGet(chunk.Entities[row], column.Type->Id), "Expected chunk columns and entity lookup to resolve the same object");
				if (row && column.Type->Descriptor.Storage == StorageKind::Direct) {
					Require(static_cast<const std::byte*>(value) - static_cast<const std::byte*>(column.At(row - 1)) ==
						static_cast<ptrdiff_t>(column.Type->Descriptor.Size), "Expected direct component rows to be contiguous SOA columns");
				}
			}
		}
		for (size_t row = 0; row < chunk.Entities.size(); ++row) {
			const auto id = chunk.Entities[row];
			const auto location = world.Location(id);
			Require(world.IsAlive(id) && seen.emplace(id.Index, id.Generation).second, "Expected every live entity to occur in exactly one row");
			Require(location.Chunk == chunk.Id && location.ChunkGeneration == chunk.Generation && location.Layout == chunk.Layout &&
				location.Group == chunk.Group && location.Row == row, "Expected entity location to follow every row move");
			Require(world.Find(world.Uuid(id)) == id, "Expected row moves to preserve UUID lookup");
		}
		rows += chunk.Entities.size(); capacity += chunk.Capacity; bytes += chunk.AllocationBytes;
		empty += chunk.Entities.empty() ? 1 : 0;
	}
	const auto stats = world.Stats();
	Require(rows == world.EntityCount() && rows == stats.Entities && capacity == stats.Capacity && bytes == stats.AllocatedBytes && empty == stats.EmptyChunks,
		"Expected storage statistics to match the public chunk snapshot");
	Require(stats.Chunks == world.Chunks().size() && world.Entities().size() == rows, "Expected complete chunk and entity enumeration");
}

void VerifyLayoutAndGroups() {
	EcsContext context;
	const auto plain = Register<PlainValue>(context, "Storage.Plain");
	const auto nonPod = Register<NonPod>(context, "Storage.NonPod");
	const auto tag = Register<ExplicitTag>(context, "Storage.Tag", true);
	const auto empty = Register<EmptyLifetime>(context, "Storage.EmptyLifetime");
	const auto sharedType = Register<Resource>(context, "Storage.Resource");
	{
		World world(context);
		const auto first = Take(world.CreateEmpty("first"));
		const auto second = Take(world.CreateEmpty("second"));
		Require(world.ListTypes(first).empty(), "Expected CreateEmpty to add no implicit scene component");
		Check(world.Emplace<PlainValue>(first, PlainValue{1}));
		Check(world.Emplace<NonPod>(first, 11));
		Check(world.Emplace<NonPod>(second, 22));
		Check(world.Emplace<PlainValue>(second, PlainValue{2}));
		Require(world.Location(first).Layout == world.Location(second).Layout && world.Location(first).Group == world.Location(second).Group,
			"Expected component insertion order to converge on the same interned layout and group");
		const auto layout = world.Location(first).Layout;
		Check(world.SetTag(first, tag));
		Require(world.Location(first).Layout == layout && world.Location(first).Group != world.Location(second).Group && world.Has(first, tag),
			"Expected a tag to change Group without changing physical layout");
		Check(world.SetTag(second, tag));
		Require(world.Location(first).Group == world.Location(second).Group, "Expected identical tags to reuse a group");
		Check(world.SetTag(second, tag, false));
		Check(world.SetTag(first, tag, false));
		Check(world.Emplace<EmptyLifetime>(first));
		Require(world.TryGet(first, empty) != nullptr && context.Types().Find(empty)->Descriptor.Storage == StorageKind::Direct,
			"Expected an ordinary empty type to retain its object lifetime and data column");
		Check(world.Remove(first, empty));

		auto object = std::make_shared<Resource>();
		auto equalObject = std::make_shared<Resource>();
		const auto handle = Take(context.Resources().Register(object));
		const auto sameHandle = Take(context.Resources().Register(object));
		const auto equalHandle = Take(context.Resources().Register(equalObject));
		Require(handle == sameHandle && handle != equalHandle, "Expected shared resources to use object identity rather than value equality");
		Check(world.SetShared(first, { sharedType, handle }));
		Check(world.SetShared(second, { sharedType, equalHandle }));
		Require(world.Location(first).Layout == layout && world.Location(second).Layout == layout &&
			world.Location(first).Group != world.Location(second).Group, "Expected shared identity to partition groups without adding columns");
		Check(world.SetShared(second, { sharedType, handle }));
		Require(world.Location(first).Group == world.Location(second).Group && world.Shared(second, sharedType) == handle,
			"Expected the same shared resource identity to reuse a group");
		const auto clone = Take(world.Clone(first, "shared clone"));
		Require(world.Shared(clone, sharedType) == handle && world.Location(clone).Group == world.Location(first).Group &&
			world.TryGet<NonPod>(clone)->Values == world.TryGet<NonPod>(first)->Values,
			"Expected cloning to retain shared identity and copy ordinary component values");
		const auto incompatible = Take(context.Resources().Register(std::make_shared<int>(17)));
		Require(!world.SetShared(first, { sharedType, incompatible }), "Expected shared component and resource native types to match");
		EcsContext foreign;
		const auto foreignHandle = Take(foreign.Resources().Register(std::make_shared<Resource>()));
		Require(!world.SetShared(first, { sharedType, foreignHandle }) && world.Shared(first, sharedType) == handle,
			"Expected foreign resource handles to be rejected without replacing the existing binding");
		Check(world.RemoveShared(first, sharedType));
		Require(!world.Shared(first, sharedType), "Expected removal of a shared binding");
		Require(world.ListTypes(first) == std::vector<TypeId>({ plain, nonPod }), "Expected ListTypes to exclude shared bindings");
		VerifyLocations(world);
	}
	NoLiveObjects<NonPod>();
	NoLiveObjects<EmptyLifetime>();
}

void VerifyChunkRowsAndLifetimes() {
	CountingMemory memory;
	EcsContext context;
	const auto plain = Register<PlainValue>(context, "Storage.RowsPlain");
	Register<NonPod>(context, "Storage.RowsNonPod");
	const auto tag = Register<ExplicitTag>(context, "Storage.RowsTag", true);
	Register<MoveOnly>(context, "Storage.RowsMoveOnly");
	Register<Immovable>(context, "Storage.RowsImmovable");
	Register<ThrowingMove>(context, "Storage.RowsThrowingMove");
	Register<OverAligned>(context, "Storage.RowsOverAligned");
	Register<IndirectAligned>(context, "Storage.RowsIndirectAligned");
	Register<CopyOnly>(context, "Storage.RowsCopyOnly");
	{
		World world(context, { .ChunkMemory = &memory });
		std::vector<EntityId> entities;
		for (int index = 0; index < 900; ++index) {
			const auto id = Take(world.CreateEmpty("row", { 1, static_cast<uint64_t>(index + 1) }));
			Check(world.Emplace<PlainValue>(id, PlainValue{index}));
			Check(world.Emplace<NonPod>(id, index));
			entities.push_back(id);
		}
		bool full = false;
		size_t populated = 0;
		for (const auto& chunk : world.Chunks()) {
			Require(chunk.AllocationBytes == 16 * 1024, "Expected ordinary chunk blocks to use the 16 KiB default");
			if (!chunk.Entities.empty()) { ++populated; full = full || chunk.Entities.size() == chunk.Capacity; }
		}
		Require(full && populated > 1, "Expected this workload to fill a chunk and span multiple chunks");
		VerifyLocations(world);
		for (size_t index = 0; index < entities.size(); index += 3) Check(world.Destroy(entities[index]));
		for (size_t index = 0; index < entities.size(); ++index) {
			if (index % 3 == 0) { Require(!world.IsAlive(entities[index]), "Expected destroyed rows to invalidate their entity"); continue; }
			Require(world.TryGet<PlainValue>(entities[index])->Value == static_cast<int>(index) &&
				world.TryGet<NonPod>(entities[index])->Values.front() == static_cast<int>(index), "Expected tail-row compaction to preserve every survivor value");
			if (index % 5 == 0) Check(world.SetTag(entities[index], tag));
			if (index % 7 == 0) Check(world.Remove(entities[index], plain));
		}
		VerifyLocations(world);
		const auto special = entities[1];
		Check(world.Emplace<MoveOnly>(special, 123));
		Check(world.Emplace<Immovable>(special, 124));
		Check(world.Emplace<ThrowingMove>(special, 125));
		Check(world.Emplace<OverAligned>(special, 126));
		Check(world.Emplace<IndirectAligned>(special, 127));
		Check(world.Emplace<CopyOnly>(special));
		const auto* immovable = world.TryGet<Immovable>(special);
		const auto* indirectAligned = world.TryGet<IndirectAligned>(special);
		for (int step = 0; step < 8; ++step) {
			Check(world.SetTag(special, tag, step % 2 == 0));
			Check(world.Emplace<PlainValue>(special, PlainValue{step}));
			Check(world.Remove(special, plain));
			Require(world.TryGet<Immovable>(special) == immovable && world.TryGet<IndirectAligned>(special) == indirectAligned,
				"Expected indirect objects to retain physical identity through layout and group migration");
			Require(*world.TryGet<MoveOnly>(special)->Value == 123 && ThrowingMove::MoveAttempts == 0,
				"Expected owned move-only payload preservation without invoking a throwing move");
			Require(world.TryGet<CopyOnly>(special)->Value == 41, "Expected noexcept copy relocation to preserve types with deleted moves");
		}
		std::array<EntityId, 4> indirectRows;
		std::array<const Immovable*, 4> indirectAddresses;
		std::array<const IndirectAligned*, 4> alignedAddresses;
		for (size_t index = 0; index < indirectRows.size(); ++index) {
			indirectRows[index] = Take(world.CreateEmpty("indirect fill-hole"));
			Check(world.Emplace<Immovable>(indirectRows[index], static_cast<int>(200 + index)));
			Check(world.Emplace<IndirectAligned>(indirectRows[index], static_cast<int>(300 + index)));
			indirectAddresses[index] = world.TryGet<Immovable>(indirectRows[index]);
			alignedAddresses[index] = world.TryGet<IndirectAligned>(indirectRows[index]);
		}
		Require(world.Location(indirectRows.front()).Chunk == world.Location(indirectRows.back()).Chunk,
			"Expected several indirect rows in one chunk for tail-fill coverage");
		Check(world.Destroy(indirectRows[0]));
		Check(world.Destroy(indirectRows[1]));
		for (size_t index : {2u, 3u}) {
			Require(world.TryGet<Immovable>(indirectRows[index]) == indirectAddresses[index] &&
				world.TryGet<IndirectAligned>(indirectRows[index]) == alignedAddresses[index] &&
				indirectAddresses[index]->Value == static_cast<int>(200 + index),
				"Expected indirect tail-fill to transfer pointers without moving or duplicating the owned objects");
		}
		Check(world.SetTag(indirectRows[3], tag));
		Require(world.TryGet<IndirectAligned>(indirectRows[3]) == alignedAddresses[3], "Expected migration after indirect tail-fill to preserve identity");
		VerifyLocations(world);
		const auto cloneError = world.Clone(special);
		Require(!cloneError && cloneError.GetError().Code == ErrorCode::UnsupportedOperation,
			"Expected cloning noncopyable components to return an explicit capability error");
		Check(world.Clear());
		Require(world.EntityCount() == 0, "Expected Clear to destroy every live row");
		VerifyLocations(world);
	}
	Require(memory.Live.empty() && memory.Bytes == 0, "Expected all real chunk blocks to be released by World destruction");
	NoLiveObjects<NonPod>(); NoLiveObjects<MoveOnly>(); NoLiveObjects<Immovable>();
	NoLiveObjects<ThrowingMove>(); NoLiveObjects<OverAligned>(); NoLiveObjects<IndirectAligned>();
	NoLiveObjects<CopyOnly>();
}

void VerifyOversizedAndFailureSafety() {
	CountingMemory memory;
	EcsContext context;
	Register<Oversized>(context, "Storage.Oversized");
	Register<NonPod>(context, "Storage.FailureNonPod");
	const auto noDefault = Register<NoDefault>(context, "Storage.NoDefault");
	const auto throwingDefault = Register<ThrowingDefault>(context, "Storage.ThrowDefault");
	Register<ThrowingCopy>(context, "Storage.ThrowCopy");
	Register<NonStandardThrow>(context, "Storage.NonStandard");
	{
		World world(context, { .ChunkMemory = &memory });
		const auto id = Take(world.CreateEmpty("failure-source", { 3, 1 }));
		Check(world.Emplace<NonPod>(id, 55));
		const auto oldTypes = world.ListTypes(id);
		const auto oldVersion = world.StructureVersion();
		memory.FailNext = true;
		const auto allocationFailure = world.Emplace<Oversized>(id);
		Require(!allocationFailure && !memory.FailNext, "Expected injected chunk allocation failure to reach the structural preparation path");
		Require(world.EntityCount() == 1 && world.IsAlive(id) && world.Name(id) == "failure-source" && world.Uuid(id) == EntityUuid{3, 1} &&
			world.ListTypes(id) == oldTypes && world.TryGet<NonPod>(id)->Values.front() == 55 && world.StructureVersion() == oldVersion,
			"Expected failed migration to preserve identity, fields, signature and structure version");
		Check(world.Emplace<Oversized>(id));
		for (const auto& chunk : world.Chunks()) {
			if (chunk.Id == world.Location(id).Chunk) Require(chunk.Capacity == 1 && chunk.AllocationBytes >= sizeof(Oversized),
				"Expected an oversized direct component to use a capacity-one aligned block");
		}
		VerifyLocations(world);
		const auto noDefaultError = world.AddDefault(id, noDefault);
		Require(!noDefaultError && noDefaultError.GetError().Code == ErrorCode::UnsupportedOperation,
			"Expected unsupported default construction to leave the existing entity unchanged");
		Check(world.Emplace<NoDefault>(id, 1234));
		for (int attempt = 0; attempt < 16; ++attempt) {
			ThrowingDefault::Fail = true;
			const auto failure = world.AddDefault(id, throwingDefault);
			ThrowingDefault::Fail = false;
			Require(!failure && !world.Has(id, throwingDefault) && world.TryGet<NonPod>(id)->Values.front() == 55,
				"Expected repeated constructor failures to leave no half-live component");
			Require(!world.Emplace<NonStandardThrow>(id, true), "Expected nonstandard constructor exceptions to become structural operation errors");
		}
		Check(world.AddDefault(id, throwingDefault));
		world.TryGet<ThrowingDefault>(id)->Value = 77;
		ThrowingDefault::Fail = true;
		Require(!world.Emplace<ThrowingDefault>(id), "Expected replacement constructor failure");
		ThrowingDefault::Fail = false;
		Require(world.TryGet<ThrowingDefault>(id)->Value == 77, "Expected replacement failure to retain the old value");
		Check(world.Emplace<ThrowingCopy>(id, "clone payload"));
		for (int attempt = 0; attempt < 16; ++attempt) {
			ThrowingCopy::Fail = true;
			const auto cloneFailure = world.Clone(id, "failed clone", { 3, static_cast<uint64_t>(100 + attempt) });
			ThrowingCopy::Fail = false;
			Require(!cloneFailure && world.EntityCount() == 1 && !world.Find({3, static_cast<uint64_t>(100 + attempt)}),
				"Expected failed clone preparation not to publish an entity or UUID");
			Require(world.TryGet<ThrowingCopy>(id)->Text == "clone payload", "Expected cloning failure to preserve source component state");
		}
		const auto clone = Take(world.Clone(id, "good clone", {3, 2}));
		Require(world.TryGet<NonPod>(clone)->Values.front() == 55 && world.TryGet<NoDefault>(clone)->Value == 1234 &&
			world.TryGet<ThrowingCopy>(clone)->Text == "clone payload", "Expected a subsequent successful clone after injected failures");
		world.TryGet<NonPod>(clone)->Values.front() = 999;
		Require(world.TryGet<NonPod>(id)->Values.front() == 55, "Expected clone to own independent non-POD value storage");
		VerifyLocations(world);
	}
	Require(memory.Live.empty(), "Expected failed and successful oversized blocks to be released");
	NoLiveObjects<NonPod>(); NoLiveObjects<NoDefault>(); NoLiveObjects<ThrowingDefault>();
	NoLiveObjects<ThrowingCopy>(); NoLiveObjects<NonStandardThrow>();
}

void VerifyIdentityAndOwnership() {
	EcsContext context;
	const auto plain = Register<PlainValue>(context, "Storage.IdentityPlain");
	World world(context, { .GenerationLimit = 2 });
	const auto first = Take(world.CreateEmpty("", { 4, 1 }));
	Require(!world.Name(first).empty() && world.Uuid(first) == EntityUuid{4, 1}, "Expected empty names to normalize and explicit UUIDs to persist");
	Check(world.Emplace<PlainValue>(first, PlainValue{73}));
	const auto duplicate = Take(world.CreateEmpty("renamed accidentally", {4, 1}));
	Require(duplicate == first && world.Name(first) != "renamed accidentally" && world.TryGet<PlainValue>(first)->Value == 73,
		"Expected repeated UUID creation to return the original entity without changing its state");
	const auto version = world.StructureVersion();
	Check(world.Destroy({}));
	Check(world.Remove(first, Register<EmptyValue>(context, "Storage.IdentityEmpty")));
	Require(world.EntityCount() == 1 && world.StructureVersion() == version, "Expected invalid destruction and absent removal to be harmless no-ops");
	Require(!world.Remove(first, std::numeric_limits<TypeId>::max()), "Expected an unknown type ID to be rejected");
	Check(world.Destroy(first));
	const auto reused = Take(world.CreateEmpty("reused"));
	Require(reused.Index == first.Index && reused.Generation != first.Generation && !world.IsAlive(first), "Expected slot reuse to advance generation");
	Check(world.Destroy(reused));
	Require(world.Stats().RetiredSlots >= 1, "Expected a slot at the configured generation limit to retire");
	const auto fresh = Take(world.CreateEmpty("fresh"));
	Require(fresh.Index != reused.Index && !world.IsAlive(reused) && world.Uuid(fresh) != EntityUuid{}, "Expected retired slots never to revive and zero UUIDs to generate an identity");
	std::vector<EntityId> previous{first, reused, fresh};
	for (int pass = 0; pass < 6; ++pass) {
		Check(world.Clear());
		for (const auto stale : previous) Require(!world.IsAlive(stale), "Expected Clear to permanently invalidate all former handles");
		previous.push_back(Take(world.CreateEmpty("after Clear")));
	}
	const auto alive = previous.back();
	EcsContext other;
	const auto otherType = Register<PlainValue>(other, "Storage.IdentityPlain");
	auto foreignValue = Take(OwnedValue::Construct<PlainValue>(*other.Types().Find(otherType), PlainValue{19}));
	Require(!world.Set(alive, std::move(foreignValue)) && !world.Has(alive, plain), "Expected a foreign registry value handle to be rejected");
	bool workerRejected = false;
	std::thread worker([&] { const auto result = world.CreateEmpty("worker"); workerRejected = !result && result.GetError().Code == ErrorCode::WrongThread; });
	worker.join();
	Require(workerRejected && world.EntityCount() == 1, "Expected structural mutation from a non-owner thread to be rejected");
	VerifyLocations(world);
	World freeSlots(context, { .GenerationLimit = 3 });
	const auto beforeClear = Take(freeSlots.CreateEmpty());
	Check(freeSlots.Destroy(beforeClear));
	Check(freeSlots.Clear());
	const auto afterClear = Take(freeSlots.CreateEmpty());
	Require(afterClear.Index == beforeClear.Index && afterClear.Generation == 3 && !freeSlots.IsAlive(beforeClear),
		"Expected Clear to advance free slots as well as live slots");
	Check(freeSlots.Destroy(afterClear));
	Require(freeSlots.Stats().RetiredSlots == 1, "Expected a free slot advanced by Clear to retire at its limit");
	World uuidEdge(context);
	const auto highest = Take(uuidEdge.CreateEmpty("highest explicit UUID", {0, std::numeric_limits<uint64_t>::max()}));
	const auto generated = Take(uuidEdge.CreateEmpty());
	Require(uuidEdge.Uuid(generated) != EntityUuid{} && uuidEdge.Uuid(generated) != uuidEdge.Uuid(highest),
		"Expected explicit maximum UUID not to prevent later automatic UUID allocation");
	Require(uuidEdge.Id() != world.Id() && uuidEdge.Id() != 0, "Expected distinct nonzero World identities");
}

void VerifyCommands() {
	EcsContext context;
	const auto moveOnly = Register<MoveOnly>(context, "Storage.CommandMoveOnly");
	const auto plain = Register<PlainValue>(context, "Storage.CommandPlain");
	const auto noDefault = Register<NoDefault>(context, "Storage.CommandNoDefault");
	const auto tag = Register<ExplicitTag>(context, "Storage.CommandTag", true);
	const auto sharedType = Register<Resource>(context, "Storage.CommandResource");
	{
		CommandBuffer abandoned(context);
		const auto pending = Take(abandoned.CreateEmpty("never published"));
		Check(abandoned.Set(pending, Take(OwnedValue::Construct<MoveOnly>(*context.Types().Find(moveOnly), 321))));
		Require(MoveOnly::Live == 1, "Expected a queued move-only value to own its payload before playback");
	}
	NoLiveObjects<MoveOnly>();
	{
		World world(context);
		CommandBuffer first(context);
		CommandBuffer second(context);
		const auto firstTemporary = Take(first.CreateEmpty("first", {5, 1}));
		const auto secondTemporary = Take(second.CreateEmpty("second", {5, 2}));
		Require(firstTemporary.Index == secondTemporary.Index && firstTemporary.Buffer != secondTemporary.Buffer,
			"Expected first temporary handles from distinct buffers to use distinct namespaces");
		const auto originalCount = second.CommandCount();
		Require(!second.SetName(firstTemporary, "wrong buffer") && second.CommandCount() == originalCount,
			"Expected a foreign temporary target to fail at recording without appending a command");
		Require(!first.Resolve(firstTemporary), "Expected unresolved temporary handles to fail before playback");
		Check(first.Set(firstTemporary, Take(OwnedValue::Construct<MoveOnly>(*context.Types().Find(moveOnly), 77))));
		Check(first.SetName(firstTemporary, "renamed by same buffer"));
		Check(second.Set(secondTemporary, Take(OwnedValue::Construct<MoveOnly>(*context.Types().Find(moveOnly), 88))));
		CommandBuffer moved(std::move(first));
		EcsContext wrongContext;
		World wrongWorld(wrongContext);
		Require(!moved.Playback(wrongWorld) && !moved.Consumed() && moved.AppliedCount() == 0 && MoveOnly::Live == 2,
			"Expected playback into a wrong Context to preserve the unconsumed buffer and payloads");
		bool wrongThread = false;
		std::thread worker([&] { const auto result = moved.Playback(world); wrongThread = !result && result.GetError().Code == ErrorCode::WrongThread; });
		worker.join();
		Require(wrongThread && !moved.Consumed(), "Expected owner-thread preflight failure not to consume commands");
		Check(moved.Playback(world));
		Check(second.Playback(world));
		const auto firstId = Take(moved.Resolve(firstTemporary));
		const auto secondId = Take(second.Resolve(secondTemporary));
		Require(firstId != secondId && world.Name(firstId) == "renamed by same buffer" &&
			*world.TryGet<MoveOnly>(firstId)->Value == 77 && *world.TryGet<MoveOnly>(secondId)->Value == 88,
			"Expected local command order and buffer namespaces to resolve their own payloads");
		Require(moved.AppliedCount() == 3 && moved.Consumed() && !moved.Playback(world) && world.EntityCount() == 2,
			"Expected successful playback to consume the buffer exactly once");
		Check(world.Clear());
		NoLiveObjects<MoveOnly>();

		CommandBuffer failed(context);
		const auto prefix = Take(failed.CreateEmpty("prefix", {5, 3}));
		Check(failed.SetName(prefix, "committed prefix"));
		Check(failed.AddDefault(prefix, noDefault));
		Check(failed.Set(prefix, Take(OwnedValue::Construct<MoveOnly>(*context.Types().Find(moveOnly), 99))));
		const auto skipped = Take(failed.CreateEmpty("skipped", {5, 4}));
		Require(failed.CommandCount() == 5, "Expected failure position to count the recorded local sequence");
		const auto error = failed.Playback(world);
		Require(!error && error.GetError().Command == 3 && failed.AppliedCount() == 2 && failed.Consumed(),
			"Expected playback failure to report the one-based failing command and successful prefix");
		const auto prefixId = Take(failed.Resolve(prefix));
		Require(world.EntityCount() == 1 && world.Name(prefixId) == "committed prefix" && !world.Has(prefixId, noDefault) &&
			!world.Has(prefixId, moveOnly) && !world.Find({5, 4}) && !failed.Resolve(skipped),
			"Expected prefix preservation without applying failed or later commands");
		NoLiveObjects<MoveOnly>();
		Require(!failed.Playback(world) && world.EntityCount() == 1, "Expected a failed consumed buffer never to replay its prefix");

		const auto resource = Take(context.Resources().Register(std::make_shared<Resource>()));
		CommandBuffer operations(context);
		const auto created = Take(operations.CreateEmpty("all operations", {5, 5}));
		Check(operations.AddDefault(created, plain));
		Check(operations.SetTag(created, tag));
		Check(operations.SetShared(created, {sharedType, resource}));
		const auto cloned = Take(operations.Clone(created, "cloned commands", {5, 6}));
		Check(operations.Remove(cloned, plain));
		Check(operations.SetTag(cloned, tag, false));
		Check(operations.RemoveShared(cloned, sharedType));
		Check(operations.Destroy(created));
		Check(operations.Playback(world));
		const auto clonedId = Take(operations.Resolve(cloned));
		Require(!world.IsAlive(Take(operations.Resolve(created))) && world.Name(clonedId) == "cloned commands" &&
			world.ListTypes(clonedId).empty() && !world.Shared(clonedId, sharedType), "Expected clone, removal and destruction commands to follow serial order");
		CommandBuffer clear(context);
		Check(clear.ClearWorld());
		const auto afterClear = Take(clear.CreateEmpty("after buffered clear"));
		Check(clear.Playback(world));
		Require(world.EntityCount() == 1 && world.IsAlive(Take(clear.Resolve(afterClear))) && !world.IsAlive(clonedId),
			"Expected later commands to observe a buffered Clear and its generation invalidation");
		VerifyLocations(world);
	}
	NoLiveObjects<MoveOnly>(); NoLiveObjects<NoDefault>();
}
}

int main() {
	VerifyLayoutAndGroups();
	VerifyChunkRowsAndLifetimes();
	VerifyOversizedAndFailureSafety();
	VerifyIdentityAndOwnership();
	VerifyCommands();
	NoLiveObjects<Resource>();
#if defined(__SANITIZE_ADDRESS__)
	std::cout << "AddressSanitizer instrumentation is enabled\n";
#endif
	std::cout << "ECSStorageSmoke passed: layouts, chunk rows, lifetimes, failures and entity identity\n";
	return 0;
}
