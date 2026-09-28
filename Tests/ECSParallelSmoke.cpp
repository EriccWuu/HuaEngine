#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/ECS/Runtime/QuerySubmission.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#if defined(HUAENGINE_ASAN_REQUIRED) && !defined(__SANITIZE_ADDRESS__)
#error The standalone parallel target must actually enable AddressSanitizer.
#endif

namespace {
using namespace HE::Ecs;
using HE::EntityId;
using HE::EntityUuid;
using namespace std::chrono_literals;

struct Position { int Value = 0; };
struct Velocity { int Value = 0; };
struct Counter { int Value = 0; };
struct Marker {};
struct ParallelPayload {
	inline static std::atomic<int> Live = 0;
	std::unique_ptr<int> Value;
	explicit ParallelPayload(int value = 0) : Value(std::make_unique<int>(value)) { Live.fetch_add(1); }
	ParallelPayload(const ParallelPayload&) = delete;
	ParallelPayload(ParallelPayload&& other) noexcept : Value(std::move(other.Value)) { Live.fetch_add(1); }
	~ParallelPayload() noexcept { Live.fetch_sub(1); }
};

void Require(bool condition, const char* message) {
	if (!condition) { std::cerr << "[ECSParallelSmoke] " << message << '\n'; std::exit(1); }
}
template<typename T> T Take(Result<T> result) {
	if (!result) { std::cerr << "[ECSParallelSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
	return std::move(result).Value();
}
void Check(Result<void> result) {
	if (!result) { std::cerr << "[ECSParallelSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
}
Error Failure(const char* message) { return {ErrorCode::TaskFailed, "ParallelSmoke", message}; }
template<typename T> TypeId Register(EcsContext& context, const char* name, bool tag = false) {
	return Take(context.Types().Register<T>(TypeGuid::FromName(name), name, tag));
}
Query MakeQuery(EcsContext& context, TypeId type, AccessMode access = AccessMode::Read) {
	QuerySpec spec; spec.Columns = {{type, Presence::Required, access}};
	return Take(Query::Create(context, std::move(spec)));
}
std::vector<EntityId> Populate(World& world, size_t count, uint64_t prefix = 1) {
	std::vector<EntityId> result;
	for (size_t index = 0; index < count; ++index) {
		const auto entity = Take(world.CreateEmpty("parallel row", {prefix, index + 1}));
		Check(world.Emplace<Position>(entity, Position{static_cast<int>(index)}));
		result.push_back(entity);
	}
	return result;
}

class Gate {
public:
	void Signal(uint64_t bits) { std::lock_guard lock(m_Mutex); m_Bits |= bits; m_Changed.notify_all(); }
	bool Wait(uint64_t bits) {
		std::unique_lock lock(m_Mutex);
		if (!m_Changed.wait_for(lock, 5s, [&] { return m_Aborted || (m_Bits & bits) == bits; })) {
			m_Aborted = true; m_Changed.notify_all();
		}
		return !m_Aborted;
	}
	void Abort() { std::lock_guard lock(m_Mutex); m_Aborted = true; m_Changed.notify_all(); }
private:
	std::mutex m_Mutex;
	std::condition_variable m_Changed;
	uint64_t m_Bits = 0;
	bool m_Aborted = false;
};

template<typename Predicate> bool AwaitCompletion(Predicate&& predicate) {
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (!predicate()) {
		if (std::chrono::steady_clock::now() >= deadline) return false;
		std::this_thread::yield();
	}
	return true;
}

void VerifyTrueOverlap() {
	// Each task has one batch so a rendezvous cannot occupy both workers with one task.
	for (int scenario = 0; scenario < 8; ++scenario) {
		EcsContext context({.WorkerCount = 2});
		Require(context.WorkerCount() == 2, "Expected exactly two configured workers for overlap evidence");
		const auto position = Register<Position>(context, "Parallel.OverlapPosition");
		const auto velocity = Register<Velocity>(context, "Parallel.OverlapVelocity");
		const auto marker = Register<Marker>(context, "Parallel.OverlapMarker", true);
		World world(context), other(context);
		const auto entities = Populate(world, scenario == 2 ? 2 : 1);
		const auto otherId = Populate(other, 1, 2).front();
		for (auto entity : entities) Check(world.Emplace<Velocity>(entity));
		Check(other.Emplace<Velocity>(otherId));
		if (scenario == 2) Check(world.SetTag(entities.front(), marker));
		QuerySpec firstSpec, secondSpec;
		World* secondWorld = &world;
		firstSpec.Columns = {{position}}; secondSpec.Columns = {{position}};
		if (scenario == 1) { firstSpec.Columns.front().Access = AccessMode::Write; secondSpec.Columns = {{velocity, Presence::Required, AccessMode::Write}}; }
		if (scenario == 2) {
			firstSpec.Columns.front().Access = secondSpec.Columns.front().Access = AccessMode::Write;
			firstSpec.Required = {marker}; secondSpec.Exclude = {marker};
		}
		if (scenario == 3) { secondWorld = &other; firstSpec.Columns.front().Access = secondSpec.Columns.front().Access = AccessMode::Write; }
		if (scenario == 4 || scenario == 5) {
			secondWorld = &other;
			const auto resource = Take(context.Resources().Register(std::make_shared<Counter>()));
			const auto secondResource = scenario == 4 ? resource : Take(context.Resources().Register(std::make_shared<Counter>()));
			const auto access = scenario == 4 ? AccessMode::Read : AccessMode::Write;
			firstSpec.ResourceAccesses = {{resource, access}}; secondSpec.ResourceAccesses = {{secondResource, access}};
		}
		if (scenario >= 6) {
			firstSpec.Columns = secondSpec.Columns = {{velocity}};
			firstSpec.RandomAccesses = {{&world, position, scenario == 6 ? AccessMode::Write : AccessMode::Read}};
			secondSpec.RandomAccesses = {{scenario == 6 ? &other : &world, position, scenario == 6 ? AccessMode::Write : AccessMode::Read}};
		}
		auto first = Take(Query::Create(context, firstSpec)); auto second = Take(Query::Create(context, secondSpec));
		Timeline timeline(context);
		Gate gate;
		std::mutex mutex;
		std::set<std::thread::id> threads;
		const auto owner = std::this_thread::get_id();
		auto callback = [&](uint64_t bit) {
			return [&, bit](TaskBatch&) -> Result<void> {
				if (std::this_thread::get_id() == owner) return Failure("Parallel callbacks must use workers, including small tasks");
				{ std::lock_guard lock(mutex); threads.insert(std::this_thread::get_id()); }
				gate.Signal(bit);
				if (!gate.Wait(3)) return Failure("Independent callbacks did not rendezvous on two workers");
				return {};
			};
		};
		const auto a = Take(timeline.Submit(first, world, callback(1)));
		const auto b = Take(timeline.Submit(second, *secondWorld, callback(2)));
		Check(timeline.Finish());
		Require(a.Status() == TaskStatus::Succeeded && b.Status() == TaskStatus::Succeeded && threads.size() == 2,
			"Expected proven overlap for RAR, distinct columns/chunks/Worlds, shared reads and distinct resources");
		std::cout << "Overlap scenario " << scenario << ": two worker callbacks rendezvoused\n";
	}
}

enum class ConflictDomain { Local, LocalThenRandom, RandomThenLocal, Resource };

void VerifyConflictDomains() {
	for (const auto domain : {ConflictDomain::Local, ConflictDomain::LocalThenRandom, ConflictDomain::RandomThenLocal, ConflictDomain::Resource}) {
		for (const auto modes : {std::pair{AccessMode::Write, AccessMode::Read}, std::pair{AccessMode::Read, AccessMode::Write}, std::pair{AccessMode::Write, AccessMode::Write}}) {
			EcsContext context({.WorkerCount = 2});
			const auto position = Register<Position>(context, "Parallel.ConflictPosition");
			const auto velocity = Register<Velocity>(context, "Parallel.ConflictVelocity");
			World target(context), source(context), witnessWorld(context);
			const auto targetId = Populate(target, 1).front();
			const auto sourceId = Populate(source, 1, 2).front();
			(void)Populate(witnessWorld, 1, 3);
			Check(target.Emplace<Velocity>(targetId)); Check(source.Emplace<Velocity>(sourceId));
			const auto object = std::make_shared<Counter>();
			const auto resource = Take(context.Resources().Register(object));
			QuerySpec firstSpec, secondSpec;
			World* firstWorld = &target; World* secondWorld = &target;
			firstSpec.Columns = {{position, Presence::Required, modes.first}};
			secondSpec.Columns = {{position, Presence::Required, modes.second}};
			if (domain == ConflictDomain::LocalThenRandom) {
				secondWorld = &source; secondSpec.Columns = {{velocity}}; secondSpec.RandomAccesses = {{&target, position, modes.second}};
			}
			if (domain == ConflictDomain::RandomThenLocal) {
				firstWorld = &source; firstSpec.Columns = {{velocity}}; firstSpec.RandomAccesses = {{&target, position, modes.first}};
			}
			if (domain == ConflictDomain::Resource) {
				secondWorld = &source; firstSpec.Columns = secondSpec.Columns = {{velocity}};
				firstSpec.ResourceAccesses = {{resource, modes.first}}; secondSpec.ResourceAccesses = {{resource, modes.second}};
			}
			auto firstQuery = Take(Query::Create(context, firstSpec)); auto secondQuery = Take(Query::Create(context, secondSpec));
			auto witness = MakeQuery(context, position);
			Timeline timeline(context);
			Gate gate;
			std::atomic<bool> completed = false, successorEntered = false, violation = false;
			auto access = [&](TaskBatch& batch, bool first) {
				const bool write = (first ? modes.first : modes.second) == AccessMode::Write;
				const bool random = (first && domain == ConflictDomain::RandomThenLocal) || (!first && domain == ConflictDomain::LocalThenRandom);
				const int expected = first || modes.first == AccessMode::Read ? 0 : 41;
				if (domain == ConflictDomain::Resource) {
					if (write) Take(batch.View().Resource<Counter>(resource)).get().Value = first ? 41 : 42;
					else Require(Take(batch.View().Resource<const Counter>(resource)).get().Value == expected, "Expected the predecessor's resource write before a read");
				} else if (random) {
					if (write) Take(Take(batch.View().Random<Position>(target)).TryGet(targetId))->Value = first ? 41 : 42;
					else Require(Take(Take(batch.View().Random<const Position>(target)).TryGet(targetId))->Value == expected, "Expected the predecessor's random-domain write before a read");
				} else {
					if (write) Take(batch.View().Column<Position>(0)).At(0).Value = first ? 41 : 42;
					else Require(Take(batch.View().Column<const Position>(0)).At(0).Value == expected, "Expected the predecessor's local write before a read");
				}
			};
			const auto first = Take(timeline.Submit(firstQuery, *firstWorld, [&](TaskBatch& batch) -> Result<void> {
				access(batch, true); gate.Signal(1);
				if (!gate.Wait(4)) return Failure("Conflict predecessor was not released");
				completed.store(true, std::memory_order_release); return {};
			}));
			const auto second = Take(timeline.Submit(secondQuery, *secondWorld, [&](TaskBatch& batch) -> Result<void> {
				successorEntered.store(true);
				if (!completed.load(std::memory_order_acquire)) { violation.store(true); return Failure("Conflicting successor overlapped its predecessor"); }
				access(batch, false); return {};
			}));
			(void)Take(timeline.Submit(witness, witnessWorld, [&](TaskBatch&) -> Result<void> {
				if (!gate.Wait(1)) return Failure("Independent witness did not observe a running predecessor");
				gate.Signal(2); return {};
			}));
			std::thread release([&] {
				if (!gate.Wait(3)) { violation.store(true); gate.Abort(); return; }
				if (successorEntered.load()) violation.store(true);
				gate.Signal(4);
			});
			const auto finish = timeline.Finish(); release.join(); Check(finish);
			Require(!violation.load() && first.Status() == TaskStatus::Succeeded && second.Status() == TaskStatus::Succeeded,
				"Expected RAW/WAR/WAW ordering with independent worker progress in every local/random/resource direction");
		}
	}
	std::cout << "Conflict domains: 12 controlled RAW/WAR/WAW traces passed\n";
}

void VerifyIndependentDomainDoesNotEraseDependency() {
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.DomainPosition");
	const auto marker = Register<Marker>(context, "Parallel.DomainMarker", true);
	World world(context);
	const auto entities = Populate(world, 2);
	Check(world.SetTag(entities.front(), marker));
	QuerySpec aSpec; aSpec.Columns = {{position, Presence::Required, AccessMode::Write}}; aSpec.Required = {marker};
	QuerySpec bSpec = aSpec; bSpec.Required.clear(); bSpec.Exclude = {marker};
	QuerySpec readSpec = aSpec; readSpec.Columns.front().Access = AccessMode::Read;
	auto a = Take(Query::Create(context, aSpec)); auto b = Take(Query::Create(context, bSpec)); auto readA = Take(Query::Create(context, readSpec));
	Timeline timeline(context);
	Gate gate;
	std::atomic<bool> aDone = false, readerEntered = false, violation = false;
	(void)Take(timeline.Submit(a, world, [&](TaskBatch& batch) -> Result<void> {
		Take(batch.View().Column<Position>(0)).At(0).Value = 71;
		gate.Signal(1);
		if (!gate.Wait(4)) return Failure("A writer was not released");
		aDone.store(true, std::memory_order_release); return {};
	}));
	(void)Take(timeline.Submit(b, world, [&](TaskBatch& batch) -> Result<void> {
		if (!gate.Wait(1)) return Failure("B writer did not observe running A");
		Take(batch.View().Column<Position>(0)).At(0).Value = 82; gate.Signal(2); return {};
	}));
	(void)Take(timeline.Submit(readA, world, [&](TaskBatch& batch) -> Result<void> {
		readerEntered.store(true);
		if (!aDone.load(std::memory_order_acquire)) { violation.store(true); return Failure("B domain erased A's outstanding dependency"); }
		Require(Take(batch.View().Column<const Position>(0)).At(0).Value == 71, "Expected A read after A write despite an intervening B write");
		return {};
	}));
	std::thread release([&] {
		if (!gate.Wait(3)) { violation.store(true); gate.Abort(); return; }
		if (readerEntered.load()) violation.store(true);
		gate.Signal(4);
	});
	const auto finish = timeline.Finish(); release.join(); Check(finish);
	Require(!violation.load() && readerEntered, "Expected outstanding dependencies to remain distinct per Chunk domain");
	std::cout << "A-write/B-write/A-read: independent progress retained the outstanding A dependency\n";
}

void VerifyBatchBoundaries() {
	{
		EcsContext automatic;
		const auto hardware = std::thread::hardware_concurrency();
		Require(automatic.WorkerCount() == (hardware > 1 ? hardware - 1 : 1), "Expected the documented automatic worker count, including the unknown-hardware fallback");
		bool invalidBatchSize = false;
		try { Timeline invalid(automatic, {.RowsPerBatch = 0}); }
		catch (const std::invalid_argument&) { invalidBatchSize = true; }
		Require(invalidBatchSize, "Expected a zero batch size to be rejected at Timeline construction");
	}
	for (const auto mode : {TimelineMode::Serial, TimelineMode::Parallel}) {
		for (const size_t count : {0u, 1u, 255u, 256u, 257u, 511u, 512u, 513u}) {
			EcsContext context({.WorkerCount = 2});
			const auto position = Register<Position>(context, "Parallel.BoundaryPosition");
			World world(context, {.ChunkBytes = 65536});
			(void)Populate(world, count);
			auto query = MakeQuery(context, position);
			Timeline timeline(context, {.Mode = mode});
			std::mutex mutex;
			std::vector<std::pair<size_t, size_t>> batches;
			std::set<int> values;
			std::set<std::thread::id> threads;
			Gate gate;
			const auto owner = std::this_thread::get_id();
			const auto task = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
				if (mode == TimelineMode::Parallel && count >= 512 && batch.BatchIndex() < 2) {
					gate.Signal(uint64_t{1} << batch.BatchIndex());
					if (!gate.Wait(3)) return Failure("Large tasks did not split into independent worker units");
				}
				auto column = Take(batch.View().Column<const Position>(0));
				std::lock_guard lock(mutex);
				batches.emplace_back(batch.BatchIndex(), column.Size()); threads.insert(std::this_thread::get_id());
				for (size_t row = 0; row < column.Size(); ++row) Require(values.insert(column.At(row).Value).second, "Expected every candidate row to occur in exactly one batch");
				return {};
			}));
			Check(timeline.Wait(task));
			std::sort(batches.begin(), batches.end());
			std::cout << "Boundary mode=" << (mode == TimelineMode::Serial ? "Serial" : "Parallel") << " rows=" << count
				<< " visited=" << values.size() << " batches=" << batches.size() << '\n';
			Require(values.size() == count && batches.size() == (count + 255) / 256, "Expected exact coverage at the 256-row and 512-row boundaries");
			for (size_t index = 0; index < batches.size(); ++index) Require(batches[index].first == index && batches[index].second == std::min<size_t>(256, count - index * 256),
				"Expected stable batch identities and the default 256-row split");
			if (count) {
				if (mode == TimelineMode::Serial) Require(threads == std::set<std::thread::id>{owner}, "Expected Serial callbacks to remain on the owner thread");
				else Require(!threads.contains(owner) && (count >= 512 ? threads.size() == 2 : threads.size() == 1),
					"Expected small tasks to use one worker unit and large tasks to expose parallel units");
			}
		}
	}
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.ConfiguredPosition");
	World world(context, {.ChunkBytes = 65536}); (void)Populate(world, 257);
	auto query = MakeQuery(context, position);
	Timeline timeline(context, {.Mode = TimelineMode::Parallel, .RowsPerBatch = 128, .SmallTaskRows = 256});
	std::mutex mutex; std::vector<size_t> sizes;
	const auto task = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		std::lock_guard lock(mutex); sizes.push_back(batch.View().Size()); return {};
	}));
	Check(timeline.Wait(task)); std::sort(sizes.begin(), sizes.end());
	Require(sizes == std::vector<size_t>{1, 128, 128}, "Expected explicit batch and small-task configuration to replace the defaults");
	std::cout << "Batch boundaries: Serial/Parallel 0,1,255,256,257,511,512,513 and configured 128 passed\n";
}

void VerifyMaskedBatchingAndDisabledMerge() {
	for (const auto mode : {TimelineMode::Serial, TimelineMode::Parallel}) {
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.MaskedPosition");
		World world(context, {.ChunkBytes = 65536});
		const auto entities = Populate(world, 1026);
		for (size_t row = 1; row < entities.size(); row += 2) Check(world.SetEnabled(entities[row], false));
		auto query = MakeQuery(context, position);
		Timeline timeline(context, {.Mode = mode});
		std::mutex mutex; std::vector<std::pair<size_t, size_t>> sizes; std::set<int> seen;
		const auto task = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
			const auto column = Take(batch.View().Column<const Position>(0));
			if (column.Size() > 1) Require(!column.TryAsSpan(), "Expected a masked logical batch with row gaps not to expose a span");
			std::lock_guard lock(mutex); sizes.emplace_back(batch.BatchIndex(), column.Size());
			for (size_t row = 0; row < column.Size(); ++row) Require(column.At(row).Value % 2 == 0 && seen.insert(column.At(row).Value).second,
				"Expected disabled rows to consume no logical batch slots");
			return {};
		}));
		Check(timeline.Wait(task)); std::sort(sizes.begin(), sizes.end());
		Require(sizes == std::vector<std::pair<size_t, size_t>>{{0, 256}, {1, 256}, {2, 1}} && seen.size() == 513,
			"Expected Serial/Parallel to split 513 enabled logical rows identically despite 1026 physical rows");
	}
	{
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.NoMergePosition");
		World world(context, {.ChunkBytes = 65536}); (void)Populate(world, 257);
		auto query = MakeQuery(context, position);
		Timeline timeline(context, {.RowsPerBatch = 256, .SmallTaskRows = 0});
		Gate gate;
		const auto task = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
			gate.Signal(uint64_t{1} << batch.BatchIndex());
			if (!gate.Wait(3)) return Failure("SmallTaskRows=0 did not disable work-unit merging");
			return {};
		}));
		Check(timeline.Wait(task));
	}
	std::cout << "Logical masks: 513 enabled rows split 256/256/1; zero small-task threshold disabled merging\n";
}

void VerifyChunkCompletionAndChanged() {
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.CompletionPosition");
	World world(context, {.ChunkBytes = 65536}), witnessWorld(context);
	const auto entities = Populate(world, 513);
	(void)Populate(witnessWorld, 1, 2);
	const auto sourceChunk = world.Location(entities.front()).Chunk;
	Require(world.Location(entities.back()).Chunk == sourceChunk, "Expected all completion-test rows in a single chunk");
	auto writer = MakeQuery(context, position, AccessMode::Write);
	QuerySpec changedSpec; changedSpec.Columns = {{position}}; changedSpec.ChangedTypes = {position};
	auto observer = Take(Query::Create(context, changedSpec));
	auto witness = MakeQuery(context, position);
	ChangedState first, second;
	Check(observer.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &first));
	Check(observer.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &second));
	Timeline timeline(context);
	Gate gate;
	std::atomic<size_t> readRows = 0, duplicateRows = 0, independentRows = 0;
	std::atomic<bool> released = false, violation = false;
	const auto writing = Take(timeline.Submit(writer, world, [&](TaskBatch& batch) -> Result<void> {
		auto values = Take(batch.View().Column<Position>(0));
		for (size_t row = 0; row < values.Size(); ++row) values.At(row).Value += 1000;
		gate.Signal(uint64_t{1} << batch.BatchIndex());
		if (batch.BatchIndex() == 2 && !gate.Wait(16)) return Failure("The last sub-batch was not released");
		return {};
	}));
	auto read = [&](ChangedState& state, std::atomic<size_t>& rows) {
		return Take(timeline.Submit(observer, world, [&](TaskBatch& batch) -> Result<void> {
			if (!released.load(std::memory_order_acquire)) { violation.store(true); return Failure("Changed observed a partially completed chunk"); }
			auto values = Take(batch.View().Column<const Position>(0));
			for (size_t row = 0; row < values.Size(); ++row) Require(values.At(row).Value >= 1000, "Expected Changed readers to see every completed sub-batch write");
			rows.fetch_add(values.Size()); return {};
		}, &state));
	};
	const auto reading = read(first, readRows);
	const auto repeated = read(first, duplicateRows);
	const auto independent = read(second, independentRows);
	(void)Take(timeline.Submit(witness, witnessWorld, [&](TaskBatch&) -> Result<void> { gate.Signal(8); return {}; }));
	std::thread release([&] {
		if (!gate.Wait(15)) { violation.store(true); gate.Abort(); return; }
		if (readRows.load() || duplicateRows.load() || independentRows.load()) violation.store(true);
		released.store(true, std::memory_order_release); gate.Signal(16);
	});
	const auto finish = timeline.Finish(); release.join(); Check(finish);
	Require(!violation.load() && readRows == 513 && duplicateRows == 0 && independentRows == 513 && writing.IsComplete() && reading.IsComplete() && repeated.IsComplete() && independent.IsComplete(),
		"Expected full-chunk completion before dependent Changed work and independent observer progress");
	Check(first.Reset()); Check(second.Reset());

	// A small unit spanning two groups must wait for the union of both groups' dependencies.
	const auto marker = Register<Marker>(context, "Parallel.CompletionMarker", true);
	World combined(context, {.ChunkBytes = 65536});
	const auto combinedEntities = Populate(combined, 300, 3);
	for (size_t index = 0; index < 100; ++index) Check(combined.SetTag(combinedEntities[index], marker));
	QuerySpec groupSpec; groupSpec.Columns = {{position, Presence::Required, AccessMode::Write}}; groupSpec.Required = {marker};
	auto groupWriter = Take(Query::Create(context, groupSpec));
	auto all = MakeQuery(context, position);
	Gate merged;
	std::atomic<bool> predecessorDone = false;
	std::atomic<size_t> mergedRows = 0;
	(void)Take(timeline.Submit(groupWriter, combined, [&](TaskBatch&) -> Result<void> {
		merged.Signal(1); if (!merged.Wait(4)) return Failure("Merged-unit predecessor was not released");
		predecessorDone.store(true, std::memory_order_release); return {};
	}));
	(void)Take(timeline.Submit(all, combined, [&](TaskBatch& batch) -> Result<void> {
		if (!predecessorDone.load(std::memory_order_acquire)) return Failure("Small unit started before every covered domain was ready");
		mergedRows.fetch_add(batch.View().Size()); return {};
	}));
	(void)Take(timeline.Submit(witness, witnessWorld, [&](TaskBatch&) -> Result<void> { merged.Signal(2); return {}; }));
	std::thread mergedRelease([&] { if (!merged.Wait(3)) { merged.Abort(); return; } merged.Signal(4); });
	const auto mergedFinish = timeline.Finish(); mergedRelease.join(); Check(mergedFinish);
	Require(mergedRows == 300, "Expected the small merged unit to cover both groups after all dependencies are ready");
	std::cout << "Chunk completion: last sub-batch barrier, Changed consumers and merged dependency union passed\n";
}
struct AliasSnapshot {
	std::vector<int> Values;
	int Tally = 0;
	bool operator==(const AliasSnapshot&) const = default;
};

void VerifySkippedChangedWriterDoesNotPublish() {
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.SkippedPosition");
	World world(context); (void)Populate(world, 1);
	QuerySpec spec; spec.Columns = {{position}}; spec.ChangedTypes = {position};
	auto reader = Take(Query::Create(context, spec));
	spec.Columns.front().Access = AccessMode::Write;
	auto writer = Take(Query::Create(context, spec));
	ChangedState writerState, readerState;
	Check(reader.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &writerState));
	Check(reader.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &readerState));
	std::atomic<size_t> writerCalls = 0, readerCalls = 0;
	Timeline timeline(context);
	const auto write = Take(timeline.Submit(writer, world, [&](TaskBatch&) -> Result<void> { ++writerCalls; return {}; }, &writerState));
	const auto read = Take(timeline.Submit(reader, world, [&](TaskBatch&) -> Result<void> { ++readerCalls; return {}; }, &readerState));
	Check(timeline.Finish());
	Require(write.Status() == TaskStatus::Succeeded && read.Status() == TaskStatus::Succeeded && writerCalls == 0 && readerCalls == 0,
		"Expected Changed-skipped writer chunks to complete without callbacks or fictitious write publication");
	Check(writerState.Reset()); Check(readerState.Reset());
	std::cout << "Changed skip: declared but unstarted writes published no version\n";
}

void VerifyPreparedCancellationAndExecutionLease() {
	{
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.PreparedPosition");
		World world(context, {.ChunkBytes = 65536}); const auto entities = Populate(world, 513);
		QuerySpec spec; spec.Columns = {{position}}; spec.ChangedTypes = {position};
		auto observer = Take(Query::Create(context, spec));
		spec.Columns.front().Access = AccessMode::Write;
		auto writer = Take(Query::Create(context, spec));
		ChangedState observation, failedObservation;
		Check(observer.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &observation));
		auto capture = Take(Detail::QuerySubmission::Capture(writer, world, &failedObservation));
		auto prepared = Take(capture.Prepare());
		Require(prepared.Chunks().size() == 1 && prepared.Batches().size() == 3 && Take(prepared.BeginChunk(0)),
			"Expected one prepared Chunk with three child batches");
		Check(prepared.RunBatch(0, [](QueryBatch& batch) -> Result<void> { Take(batch.Column<Position>(0)).At(0).Value += 20; return {}; }));
		const auto failed = prepared.RunBatch(1, [](QueryBatch& batch) -> Result<void> {
			Take(batch.Column<Position>(0)).At(0).Value += 30; return Failure("injected prepared child failure");
		});
		Require(!failed, "Expected explicit prepared callback errors to propagate");
		const auto cancelled = prepared.CompleteChunk(0, true);
		const auto finished = prepared.Finish(false);
		Require(!cancelled && cancelled.GetError().Code == ErrorCode::Cancelled && !finished && failedObservation.ObservationCount() == 0,
			"Expected mixed completed, failed and unstarted children to cancel without acknowledging the observation");
		Check(failedObservation.Reset());
		size_t changedRows = 0;
		Check(observer.Each(world, [&](QueryRow&) -> Result<void> { ++changedRows; return {}; }, &observation));
		Require(changedRows == 513 && static_cast<const World&>(world).TryGet<Position>(entities.front())->Value == 20 &&
			static_cast<const World&>(world).TryGet<Position>(entities[256])->Value == 286 &&
			static_cast<const World&>(world).TryGet<Position>(entities.back())->Value == 512,
			"Expected only started child data to change while their Chunk write version remains observable");
	}
	{
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.PreparedLeasePosition");
		const auto payload = Register<ParallelPayload>(context, "Parallel.PreparedLeasePayload");
		World world(context); const auto entity = Populate(world, 1).front(); Check(world.Emplace<ParallelPayload>(entity, 93));
		QuerySpec readSpec; readSpec.Columns = {{position}}; readSpec.ChangedTypes = {position};
		auto observer = Take(Query::Create(context, readSpec));
		QuerySpec writeSpec = readSpec; writeSpec.Columns.front().Access = AccessMode::Write; writeSpec.Columns.push_back({payload});
		auto writer = Take(Query::Create(context, writeSpec));
		ChangedState observation, abandoned;
		Check(observer.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &observation));
		auto capture = Take(Detail::QuerySubmission::Capture(writer, world, &abandoned));
		auto prepared = std::make_unique<Detail::PreparedExecution>(Take(capture.Prepare()));
		Require(Take(prepared->BeginChunk(0)), "Expected the lease test's first Changed observation to run");
		Check(prepared->RunBatch(0, [&](QueryBatch& batch) -> Result<void> {
			auto& native = Take(batch.Column<const ParallelPayload>(1)).At(0);
			prepared.reset();
			Require(ParallelPayload::Live == 1 && *native.Value == 93 && batch.Valid(),
				"Expected the active execution lease to outlive its PreparedExecution facade");
			Take(batch.Column<Position>(0)).At(0).Value = 88; return {};
		}));
		Require(!prepared && abandoned.ObservationCount() == 0, "Expected abandoned execution not to acknowledge the observer");
		Check(abandoned.Reset());
		size_t observed = 0;
		Check(observer.Each(world, [&](QueryRow&) -> Result<void> { ++observed; return {}; }, &observation));
		Require(observed == 1 && static_cast<const World&>(world).TryGet<Position>(entity)->Value == 88,
			"Expected the final execution lease to publish started writes before releasing its borrow");
		Check(world.Clear()); Require(ParallelPayload::Live == 0, "Expected lease fallback not to leak native objects");
	}
	std::cout << "Prepared lifetime: mixed-child cancellation and callback facade release passed\n";
}

void VerifyChangedReservationAcrossContexts() {
	EcsContext first({.WorkerCount = 1}), second({.WorkerCount = 1});
	const auto firstType = Register<Position>(first, "Parallel.CrossContextPosition");
	const auto secondType = Register<Position>(second, "Parallel.CrossContextPosition");
	World a(first), b(second); (void)Populate(a, 1); (void)Populate(b, 1, 2);
	QuerySpec firstSpec; firstSpec.Columns = {{firstType}}; firstSpec.ChangedTypes = {firstType};
	QuerySpec secondSpec; secondSpec.Columns = {{secondType}}; secondSpec.ChangedTypes = {secondType};
	auto firstQuery = Take(Query::Create(first, firstSpec)); auto secondQuery = Take(Query::Create(second, secondSpec));
	Timeline firstTimeline(first), secondTimeline(second);
	ChangedState observer;
	Gate gate;
	const auto firstTask = Take(firstTimeline.Submit(firstQuery, a, [&](TaskBatch&) -> Result<void> {
		if (!gate.Wait(1)) return Failure("Cross-Context observer predecessor was not released"); return {};
	}, &observer));
	const auto rejected = secondTimeline.Submit(secondQuery, b, [](TaskBatch&) -> Result<void> { return {}; }, &observer);
	Require(!rejected && rejected.GetError().Code == ErrorCode::Busy,
		"Expected one Changed observer not to queue concurrently in different Contexts");
	gate.Signal(1); Check(firstTimeline.Finish());
	std::atomic<size_t> rows = 0;
	const auto secondTask = Take(secondTimeline.Submit(secondQuery, b, [&](TaskBatch& batch) -> Result<void> { rows += batch.View().Size(); return {}; }, &observer));
	Check(secondTimeline.Finish());
	Require(firstTask.Status() == TaskStatus::Succeeded && secondTask.Status() == TaskStatus::Succeeded && rows == 1,
		"Expected the observer to change Context after every previous reservation has drained");
	Check(observer.Reset());
	std::cout << "Changed reservation: concurrent cross-Context capture rejected, drained reuse passed\n";
}

void VerifyChangedPrunesDestroyedChunkGenerations() {
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.PrunedPosition");
	const auto marker = Register<Marker>(context, "Parallel.PrunedMarker", true);
	World world(context); const auto first = Populate(world, 1).front(); Check(world.SetTag(first, marker));
	QuerySpec allSpec; allSpec.Columns = {{position}}; allSpec.ChangedTypes = {position};
	auto all = Take(Query::Create(context, allSpec));
	QuerySpec markedSpec = allSpec; markedSpec.Required = {marker};
	auto marked = Take(Query::Create(context, markedSpec));
	ChangedState state;
	Check(all.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &state));
	Check(marked.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &state));
	Require(state.ObservationCount() == 2, "Expected two filter identities to retain independent observations of the same live Chunk");
	Check(world.Clear()); (void)Populate(world, 1, 2);
	Timeline timeline(context);
	std::atomic<size_t> rows = 0;
	const auto task = Take(timeline.Submit(all, world, [&](TaskBatch& batch) -> Result<void> { rows += batch.View().Size(); return {}; }, &state));
	Check(timeline.Wait(task));
	Require(rows == 1 && state.ObservationCount() == 1,
		"Expected successful observation to prune dead Chunk generations across every filter without retaining Clear history");
	Check(state.Reset());
	std::cout << "Changed pruning: destroyed Chunk generations removed across filter identities\n";
}

std::pair<std::vector<size_t>, std::string> RunSkippedBatchIdentity(TimelineMode mode) {
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.StableBatchPosition");
	World world(context, {.ChunkBytes = 512}); const auto entities = Populate(world, 200);
	QuerySpec spec; spec.Columns = {{position}}; spec.ChangedTypes = {position};
	auto query = Take(Query::Create(context, spec));
	ChangedState observer;
	Timeline timeline(context, {.Mode = mode});
	std::mutex mutex;
	std::vector<std::pair<size_t, ChunkId>> original;
	const auto initial = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		std::lock_guard lock(mutex); original.emplace_back(batch.BatchIndex(), batch.View().Chunk().Id); return {};
	}, &observer));
	Check(timeline.Wait(initial));
	const auto target = entities.back(); const auto chunk = world.Location(target).Chunk;
	const auto old = std::find_if(original.begin(), original.end(), [&](const auto& entry) { return entry.second == chunk; });
	Require(original.size() > 1 && old != original.end() && old->first > 0,
		"Expected the changed target to belong to a later captured Chunk");
	const auto expected = old->first;
	world.TryGet<Position>(target)->Value += 1000; Check(world.PublishWrite(target, position));
	std::vector<size_t> changed;
	const auto updated = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		Require(batch.View().Chunk().Id == chunk, "Expected only the explicitly published later Chunk to pass Changed");
		Check(batch.Commands().SetName(target, "stable batch " + std::to_string(batch.BatchIndex())));
		std::lock_guard lock(mutex); changed.push_back(batch.BatchIndex()); return {};
	}, &observer));
	Check(timeline.Wait(updated)); Check(timeline.CommitCommands());
	Require(changed == std::vector<size_t>{expected}, "Expected a skipped earlier Chunk not to renumber later batch identities");
	return {changed, std::string(world.Name(target))};
}

void VerifyStableBatchIdentityAndPendingResolve() {
	Require(RunSkippedBatchIdentity(TimelineMode::Serial) == RunSkippedBatchIdentity(TimelineMode::Parallel),
		"Expected identical batch IDs and command values when Changed skips earlier Chunks in both modes");
	EcsContext context({.WorkerCount = 1});
	const auto position = Register<Position>(context, "Parallel.PendingResolvePosition");
	World world(context); (void)Populate(world, 1);
	auto query = MakeQuery(context, position);
	Timeline timeline(context);
	Gate gate;
	TemporaryEntity temporary{};
	const auto task = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		temporary = Take(batch.Commands().CreateEmpty("pending resolution"));
		gate.Signal(1);
		if (!gate.Wait(2)) return Failure("Resolve test did not release the recording worker");
		return {};
	}));
	Require(gate.Wait(1), "Expected the worker to record a temporary entity before the Resolve probe");
	const auto pending = timeline.Resolve(task, temporary);
	Require(!pending && pending.GetError().Code == ErrorCode::Busy,
		"Expected Resolve to reject access to command buffers while their task is still recording");
	gate.Signal(2); Check(timeline.Wait(task)); Check(timeline.CommitCommands());
	Require(world.IsAlive(Take(timeline.Resolve(task, temporary))), "Expected the same temporary entity to resolve after completion and commit");
	std::cout << "Stable batching: skipped Chunk IDs agree in both modes; pending Resolve returns Busy\n";
}

AliasSnapshot RunAliasScenario(TimelineMode mode, int kind) {
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.AliasPosition");
	World world(context, {.ChunkBytes = 2048}), target(context);
	const auto entities = Populate(world, 768);
	const auto missingComponent = Take(world.CreateEmpty("random target without Position", {1, 1000}));
	const auto targetId = Populate(target, 1, 2).front();
	const auto object = std::make_shared<Counter>();
	const auto resource = Take(context.Resources().Register(object));
	QuerySpec spec; spec.Columns = {{position, Presence::Required, kind == 0 ? AccessMode::Write : AccessMode::Read}};
	if (kind == 0) spec.RandomAccesses = {{&world, position, AccessMode::Read}};
	if (kind == 1) spec.RandomAccesses = {{&target, position, AccessMode::Write}};
	if (kind == 2) spec.ResourceAccesses = {{resource, AccessMode::Write}};
	auto query = Take(Query::Create(context, spec));
	Timeline timeline(context, {.Mode = mode});
	std::atomic<int> active = 0;
	std::atomic<size_t> batches = 0;
	const auto task = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		if (active.fetch_add(1) != 0) { active.fetch_sub(1); return Failure("One task overlapped callbacks with a random alias or mutable resource"); }
		struct Guard { std::atomic<int>& Active; ~Guard() { Active.fetch_sub(1); } } guard{active};
		batches.fetch_add(1);
		if (kind == 0) {
			auto values = Take(batch.View().Column<Position>(0));
			auto random = Take(batch.View().Random<const Position>(world));
			Require(Take(random.TryGet(EntityId{})) == nullptr && Take(random.TryGet(missingComponent)) == nullptr,
				"Expected random access to missing entities or components to return a successful null result");
			for (size_t row = 0; row < values.Size(); ++row) {
				const auto index = static_cast<size_t>(values.At(row).Value);
				values.At(row).Value = Take(random.TryGet(entities[(index + entities.size() - 1) % entities.size()]))->Value + 1;
			}
		} else if (kind == 1) {
			auto* value = Take(Take(batch.View().Random<Position>(target)).TryGet(targetId));
			for (size_t row = 0; row < batch.View().Size(); ++row) ++value->Value;
		} else {
			auto& value = Take(batch.View().Resource<Counter>(resource)).get();
			for (size_t row = 0; row < batch.View().Size(); ++row) ++value.Value;
		}
		return {};
	}));
	Check(timeline.Wait(task));
	Require(batches > 1 && active == 0, "Expected multiple internally serialized batches with balanced callback ownership");
	AliasSnapshot snapshot;
	for (const auto entity : entities) snapshot.Values.push_back(static_cast<const World&>(world).TryGet<Position>(entity)->Value);
	snapshot.Tally = kind == 1 ? static_cast<const World&>(target).TryGet<Position>(targetId)->Value : object->Value;
	if (kind != 0) Require(snapshot.Tally == 768, "Expected every serialised shared write to be retained");
	return snapshot;
}

struct CommandSnapshot {
	std::vector<int> Values;
	std::vector<std::tuple<EntityUuid, std::string, int>> Created;
	std::string LastName;
	bool operator==(const CommandSnapshot&) const = default;
};

CommandSnapshot RunCommandScenario(TimelineMode mode) {
	using Payload = ParallelPayload;
	Require(Payload::Live == 0, "Expected no live command payloads at scenario entry");
	EcsContext context({.WorkerCount = 2});
	const auto position = Register<Position>(context, "Parallel.CommandPosition");
	const auto payload = Register<Payload>(context, "Parallel.CommandPayload");
	const auto* descriptor = context.Types().Find(payload);
	World world(context, {.ChunkBytes = 65536});
	const auto entities = Populate(world, 768, 4);
	auto query = MakeQuery(context, position, AccessMode::Write);
	Timeline timeline(context, {.Mode = mode});
	Gate gate;
	std::mutex mutex;
	struct Mapping { size_t Pass; size_t Batch; TemporaryEntity Entity; };
	std::vector<Mapping> mappings;
	std::vector<size_t> completed;
	std::vector<TaskHandle> tasks;
	for (size_t pass = 0; pass < 2; ++pass) {
		tasks.push_back(Take(timeline.Submit(query, world, [&, pass](TaskBatch& batch) -> Result<void> {
			if (mode == TimelineMode::Parallel && pass == 0 && batch.BatchIndex() == 0 && !gate.Wait(1)) return Failure("Later command batch did not finish before the blocked first batch");
			auto values = Take(batch.View().Column<Position>(0));
			for (size_t row = 0; row < values.Size(); ++row) values.At(row).Value += 100 + static_cast<int>(batch.BatchIndex());
			const auto temp = Take(batch.Commands().CreateEmpty("ordered command", {4, 1000 + pass * 10 + batch.BatchIndex()}));
			Check(batch.Commands().Set(temp, Take(OwnedValue::Construct<Payload>(*descriptor, static_cast<int>(pass * 10 + batch.BatchIndex())))));
			Check(batch.Commands().SetName(entities.front(), std::to_string(pass) + ":" + std::to_string(batch.BatchIndex()) + ":first"));
			Check(batch.Commands().SetName(entities.front(), std::to_string(pass) + ":" + std::to_string(batch.BatchIndex()) + ":last"));
			{ std::lock_guard lock(mutex); mappings.push_back({pass, batch.BatchIndex(), temp}); if (pass == 0) completed.push_back(batch.BatchIndex()); }
			if (pass == 0 && batch.BatchIndex() == 1) gate.Signal(1);
			return {};
		})));
	}
	Check(timeline.Finish());
	Require(world.EntityCount() == entities.size() && Payload::Live == 6, "Expected all parallel command buffers to remain invisible and own their values until commit");
	if (mode == TimelineMode::Parallel) Require(std::find(completed.begin(), completed.end(), 1) < std::find(completed.begin(), completed.end(), 0),
		"Expected a controlled reversal of first-task callback completion order");
	Check(timeline.CommitCommands());
	CommandSnapshot snapshot;
	for (auto entity : entities) snapshot.Values.push_back(static_cast<const World&>(world).TryGet<Position>(entity)->Value);
	for (const auto& mapping : mappings) {
		const auto entity = Take(timeline.Resolve(tasks[mapping.Pass], mapping.Entity));
		snapshot.Created.emplace_back(world.Uuid(entity), std::string(world.Name(entity)), *static_cast<const World&>(world).TryGet<Payload>(entity)->Value);
	}
	std::sort(snapshot.Created.begin(), snapshot.Created.end(), [](const auto& left, const auto& right) {
		const auto& a = std::get<0>(left); const auto& b = std::get<0>(right);
		return std::pair{a.High, a.Low} < std::pair{b.High, b.Low};
	});
	snapshot.LastName = world.Name(entities.front());
	Require(snapshot.LastName == "1:2:last" && snapshot.Created.size() == 6, "Expected deterministic task/batch/local command merge order");
	Check(timeline.CommitCommands());
	Require(world.EntityCount() == 774, "Expected repeated commit not to replay parallel buffers");
	Check(world.Clear());
	Require(Payload::Live == 0, "Expected no move-only command payload leak after committed entities are cleared");
	return snapshot;
}

void VerifyAliasesAndDeterministicCommands() {
	for (int kind = 0; kind < 3; ++kind) Require(RunAliasScenario(TimelineMode::Serial, kind) == RunAliasScenario(TimelineMode::Parallel, kind),
		"Expected Serial/Parallel equivalence for same-task random aliases and mutable shared resources");
	Require(RunCommandScenario(TimelineMode::Serial) == RunCommandScenario(TimelineMode::Parallel),
		"Expected Serial/Parallel component, UUID and command results to match despite reversed batch completion");
	std::cout << "Serial/Parallel snapshots: aliases, move-only commands and reversed completion passed\n";
}
void VerifySingleWorkerAndFailure() {
	{
		EcsContext context({.WorkerCount = 1});
		Require(context.WorkerCount() == 1, "Expected an explicit single-worker pool");
		const auto position = Register<Position>(context, "Parallel.ChainPosition");
		World world(context, {.ChunkBytes = 65536});
		const auto entities = Populate(world, 513);
		auto writer = MakeQuery(context, position, AccessMode::Write);
		auto reader = MakeQuery(context, position);
		Timeline timeline(context);
		std::set<std::thread::id> threads;
		const auto owner = std::this_thread::get_id();
		for (size_t step = 0; step < 64; ++step) {
			(void)Take(timeline.Submit(writer, world, [&](TaskBatch& batch) -> Result<void> {
				Require(std::this_thread::get_id() != owner, "Expected even a single configured worker to execute callbacks off the owner thread");
				threads.insert(std::this_thread::get_id());
				auto values = Take(batch.View().Column<Position>(0));
				for (size_t row = 0; row < values.Size(); ++row) ++values.At(row).Value;
				return {};
			}));
		}
		std::atomic<size_t> reads = 0;
		for (size_t branch = 0; branch < 8; ++branch) {
			(void)Take(timeline.Submit(reader, world, [&](TaskBatch& batch) -> Result<void> { reads.fetch_add(batch.View().Size()); return {}; }));
		}
		const auto last = Take(timeline.Submit(writer, world, [&](TaskBatch& batch) -> Result<void> {
			Require(reads == 8 * entities.size(), "Expected the fan-in writer to wait for every preceding reader");
			auto values = Take(batch.View().Column<Position>(0));
			for (size_t row = 0; row < values.Size(); ++row) values.At(row).Value += 2;
			return {};
		}));
		Check(timeline.Wait(last));
		Require(threads.size() == 1, "Expected one reusable worker to finish the chain and fan-in/fan-out");
		for (size_t index = 0; index < entities.size(); ++index) Require(static_cast<const World&>(world).TryGet<Position>(entities[index])->Value == static_cast<int>(index + 66),
			"Expected complete serial-equivalent results without worker dependency waits");
	}
	for (int failureKind = 0; failureKind < 3; ++failureKind) {
		Require(ParallelPayload::Live == 0, "Expected balanced payload lifetimes before a parallel failure");
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.FailurePosition");
		const auto velocity = Register<Velocity>(context, "Parallel.FailureVelocity");
		const auto payload = Register<ParallelPayload>(context, "Parallel.FailurePayload");
		const auto* descriptor = context.Types().Find(payload);
		World world(context, {.ChunkBytes = 65536}), other(context);
		const auto entities = Populate(world, 513); const auto otherId = Populate(other, 1, 2).front();
		for (auto entity : entities) Check(world.Emplace<Velocity>(entity));
		QuerySpec failedSpec; failedSpec.Columns = {{position, Presence::Required, AccessMode::Write}}; failedSpec.ChangedTypes = {position};
		auto failedQuery = Take(Query::Create(context, failedSpec));
		QuerySpec cancelledSpec; cancelledSpec.Columns = {{position}, {velocity, Presence::Required, AccessMode::Write}}; cancelledSpec.ChangedTypes = {velocity};
		auto cancelledQuery = Take(Query::Create(context, cancelledSpec));
		QuerySpec observeSpec; observeSpec.Columns = {{velocity}}; observeSpec.ChangedTypes = {velocity};
		auto observer = Take(Query::Create(context, observeSpec));
		QuerySpec startedSpec; startedSpec.Columns = {{position}}; startedSpec.ChangedTypes = {position};
		auto startedObserver = Take(Query::Create(context, startedSpec));
		ChangedState state, velocityState, positionState;
		Check(observer.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &velocityState));
		Check(startedObserver.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &positionState));
		auto independentQuery = MakeQuery(context, position, AccessMode::Write);
		Timeline timeline(context);
		Gate gate;
		std::atomic<size_t> forbidden = 0;
		const auto failed = Take(timeline.Submit(failedQuery, world, [&](TaskBatch& batch) -> Result<void> {
			Take(batch.View().Column<Position>(0)).At(0).Value += 5;
			if (batch.BatchIndex() != 0) return {};
			if (!gate.Wait(1)) return Failure("Failure task was not released after successor submission");
			if (failureKind == 1) throw std::runtime_error("injected parallel task exception");
			if (failureKind == 2) throw 31;
			return Failure("injected parallel task error");
		}, &state));
		const auto cancelled = Take(timeline.Submit(cancelledQuery, world, [&](TaskBatch&) -> Result<void> { forbidden.fetch_add(1); return {}; }, &state));
		const auto independent = Take(timeline.Submit(independentQuery, other, [&](TaskBatch& batch) -> Result<void> {
			Take(batch.View().Column<Position>(0)).At(0).Value = 99;
			const auto temp = Take(batch.Commands().CreateEmpty("discard parallel failure commands"));
			Check(batch.Commands().Set(temp, Take(OwnedValue::Construct<ParallelPayload>(*descriptor, 17))));
			return {};
		}));
		Require(!state.Reset(), "Expected queued or running Changed reservations to prevent Reset");
		gate.Signal(1);
		const auto finish = timeline.Finish();
		Require(!finish && finish.GetError().Task == failed.Sequence() && cancelled.Status() == TaskStatus::Cancelled && independent.Status() == TaskStatus::Succeeded && forbidden == 0,
			"Expected explicit and thrown parallel failures to cancel dependents while independent tasks finish");
		Require(state.ObservationCount() == 0 && static_cast<const World&>(other).TryGet<Position>(otherId)->Value == 99 && other.EntityCount() == 1 && ParallelPayload::Live == 0,
			"Expected failed observations to remain unacknowledged and uncommitted payloads to be discarded");
		size_t phantomRows = 0;
		Check(observer.Each(world, [&](QueryRow&) -> Result<void> { ++phantomRows; return {}; }, &velocityState));
		Require(phantomRows == 0 && !timeline.Submit(independentQuery, other, [](TaskBatch&) -> Result<void> { return {}; }),
			"Expected cancelled writes not to publish versions and failures to block new submissions");
		size_t startedRows = 0;
		Check(startedObserver.Each(world, [&](QueryRow&) -> Result<void> { ++startedRows; return {}; }, &positionState));
		Require(startedRows == entities.size(), "Expected an already-started failed writer to publish its Chunk version without acknowledging the failed consumer");
		Check(state.Reset()); Check(timeline.ResetError());
		const auto recovered = Take(timeline.Submit(independentQuery, other, [](TaskBatch&) -> Result<void> { return {}; }));
		Check(timeline.Wait(recovered)); Check(timeline.CommitCommands());
		Require(other.EntityCount() == 1, "Expected reset not to replay discarded parallel command buffers");
	}
	std::cout << "Ready queue: one-worker chain/fan-in/fan-out and three failure channels passed\n";
}
void VerifyParallelShutdown() {
	for (int kind = 0; kind < 3; ++kind) {
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.ShutdownPosition");
		const auto payload = Register<ParallelPayload>(context, "Parallel.ShutdownPayload");
		const auto* descriptor = context.Types().Find(payload);
		auto world = std::make_unique<World>(context);
		const auto entity = Populate(*world, 1).front();
		auto writer = MakeQuery(context, position, AccessMode::Write);
		auto timeline = std::make_unique<Timeline>(context);
		Gate gate;
		const auto task = Take(timeline->Submit(writer, *world, [&](TaskBatch& batch) -> Result<void> {
			gate.Signal(1);
			if (!gate.Wait(4)) return Failure("Shutdown did not release a running worker");
			Take(batch.View().Column<Position>(0)).At(0).Value = 55;
			if (kind == 2) {
				const auto temp = Take(batch.Commands().CreateEmpty("discard on destructor"));
				Check(batch.Commands().Set(temp, Take(OwnedValue::Construct<ParallelPayload>(*descriptor, 55))));
			}
			return {};
		}));
		std::atomic<bool> timedOut = false;
		std::thread release([&] { if (!gate.Wait(3)) { timedOut.store(true); gate.Abort(); return; } gate.Signal(4); });
		gate.Signal(2);
		if (kind == 0) {
			auto scope = Take(WorldReadScope::Acquire(*world));
			Require(scope.Get().TryGet<Position>(entity)->Value == 55 && task.IsComplete(), "Expected ReadScope to drain a running parallel writer");
		} else if (kind == 1) world.reset();
		else timeline.reset();
		release.join();
		Require(!timedOut.load() && task.Status() == TaskStatus::Succeeded && !context.HasScheduledWork() && ParallelPayload::Live == 0,
			"Expected scope, World and Timeline shutdown to release all running work and uncommitted payloads");
		if (world) Require(world->EntityCount() == 1, "Expected destructor shutdown not to commit buffered entity creation");
		if (timeline) Check(timeline->Finish());
	}
	{
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.SelfWorldPosition");
		const auto payload = Register<ParallelPayload>(context, "Parallel.SelfWorldPayload");
		auto world = std::make_unique<World>(context);
		const auto entity = Populate(*world, 1).front();
		Check(world->Emplace<ParallelPayload>(entity, 81));
		QuerySpec spec; spec.Columns = {{position, Presence::Required, AccessMode::Write}, {payload}}; spec.ChangedTypes = {position};
		auto query = Take(Query::Create(context, spec));
		ChangedState state;
		Timeline timeline(context);
		const auto task = Take(timeline.Submit(query, *world, [&](TaskBatch& batch) -> Result<void> {
			auto& object = Take(batch.View().Column<const ParallelPayload>(1)).At(0);
			world.reset();
			Require(ParallelPayload::Live == 1 && *object.Value == 81 && !batch.View().Valid(),
				"Expected worker-side World destruction to invalidate views while retaining borrowed objects until callback return");
			return {};
		}, &state));
		const auto finish = timeline.Finish();
		Require(!finish && finish.GetError().Code == ErrorCode::InvalidState && task.Status() == TaskStatus::Failed &&
			ParallelPayload::Live == 0 && state.ObservationCount() == 0 && !context.HasScheduledWork(),
			"Expected self-destruction on a worker to fail safely, release storage and leave Changed unacknowledged");
		Check(state.Reset()); Check(timeline.ResetError());
	}
	{
		EcsContext context({.WorkerCount = 2});
		const auto position = Register<Position>(context, "Parallel.SelfTimelinePosition");
		World world(context), other(context);
		(void)Populate(world, 1); const auto otherEntity = Populate(other, 1, 2).front();
		auto writer = MakeQuery(context, position, AccessMode::Write); auto reader = MakeQuery(context, position);
		auto owner = std::make_unique<Timeline>(context);
		Gate gate;
		std::atomic<size_t> forbidden = 0;
		const auto failed = Take(owner->Submit(writer, world, [token = std::make_unique<ParallelPayload>(77), &owner, &gate](TaskBatch& batch) -> Result<void> {
			if (!gate.Wait(1)) return Failure("Closing callback was not released after successors were queued");
			(void)Take(batch.Commands().CreateEmpty("discard closing worker command"));
			owner.reset();
			Require(ParallelPayload::Live == 1 && *token->Value == 77, "Expected a worker callback's move-only capture to survive Timeline facade destruction");
			Take(batch.View().Column<Position>(0)).At(0).Value = 77;
			gate.Signal(2);
			return Failure("injected worker error after Timeline facade destruction");
		}));
		const auto cancelled = Take(owner->Submit(reader, world, [&](TaskBatch&) -> Result<void> { forbidden.fetch_add(1); return {}; }));
		const auto independent = Take(owner->Submit(writer, other, [](TaskBatch& batch) -> Result<void> { Take(batch.View().Column<Position>(0)).At(0).Value = 99; return {}; }));
		gate.Signal(1);
		Require(gate.Wait(2), "Expected worker Timeline self-destruction to signal after resetting the facade");
		Require(AwaitCompletion([&] { return failed.IsComplete() && cancelled.IsComplete() && independent.IsComplete() && !context.HasScheduledWork(); }),
			"Expected autonomous closing to settle retained handles without accessing a destroyed Timeline facade");
		Require(!owner && failed.Status() == TaskStatus::Failed && cancelled.Status() == TaskStatus::Cancelled && independent.Status() == TaskStatus::Succeeded && forbidden == 0,
			"Expected worker-side Timeline destruction to preserve failure cancellation and settle independent tasks");
		Require(ParallelPayload::Live == 0 && !context.HasScheduledWork() && world.EntityCount() == 1 &&
			static_cast<const World&>(other).TryGet<Position>(otherEntity)->Value == 99,
			"Expected worker shutdown to release its capture and commands without losing independent writes");
		Timeline replacement(context);
		const auto recovery = Take(replacement.Submit(reader, world, [](TaskBatch&) -> Result<void> { return {}; }));
		Check(replacement.Wait(recovery)); Check(replacement.Finish());
	}
	std::cout << "Parallel shutdown: scopes, World/Timeline drain and worker self-destruction passed\n";
}
}

int main() {
	std::cout << std::unitbuf;
	VerifyTrueOverlap();
	VerifyConflictDomains();
	VerifyIndependentDomainDoesNotEraseDependency();
	VerifyBatchBoundaries();
	VerifyMaskedBatchingAndDisabledMerge();
	VerifyChunkCompletionAndChanged();
	VerifySkippedChangedWriterDoesNotPublish();
	VerifyPreparedCancellationAndExecutionLease();
	VerifyChangedReservationAcrossContexts();
	VerifyChangedPrunesDestroyedChunkGenerations();
	VerifyStableBatchIdentityAndPendingResolve();
	VerifyAliasesAndDeterministicCommands();
	VerifySingleWorkerAndFailure();
	VerifyParallelShutdown();
#if defined(__SANITIZE_ADDRESS__)
	std::cout << "AddressSanitizer instrumentation is enabled\n";
#endif
	std::cout << "ECSParallelSmoke passed\n";
	return 0;
}
