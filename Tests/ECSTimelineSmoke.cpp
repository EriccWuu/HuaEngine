#include "ECSLifecycleFixtures.h"
#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(HUAENGINE_ASAN_REQUIRED) && !defined(__SANITIZE_ADDRESS__)
#error The standalone timeline target must actually enable AddressSanitizer.
#endif

namespace {
using namespace HE::Ecs;
using HE::EntityId;
using HE::EntityUuid;

struct Position { int Value = 0; };
struct Velocity { int Value = 0; };
struct Counter { int Value = 0; };
struct GroupMarker {};

void Require(bool condition, const char* message) {
	if (!condition) { std::cerr << "[ECSTimelineSmoke] " << message << '\n'; std::exit(1); }
}

template<typename T>
T Take(Result<T> result) {
	if (!result) {
		std::cerr << "[ECSTimelineSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n';
		std::exit(1);
	}
	return std::move(result).Value();
}

void Check(Result<void> result) {
	if (!result) {
		std::cerr << "[ECSTimelineSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n';
		std::exit(1);
	}
}

template<typename T>
TypeId Register(EcsContext& context, const char* name) {
	return Take(context.Types().Register<T>(TypeGuid::FromName(name), name));
}

Query MakeQuery(EcsContext& context, TypeId type, AccessMode access = AccessMode::Read, bool changed = false) {
	QuerySpec spec; spec.Columns = {{type, Presence::Required, access}};
	if (changed) spec.ChangedTypes = {type};
	return Take(Query::Create(context, std::move(spec)));
}

template<typename Exception = std::logic_error, typename Action>
void Throws(Action&& action) {
	bool rejected = false;
	try { action(); } catch (const Exception&) { rejected = true; }
	Require(rejected, "Expected invalid ownership or scope access to fail explicitly");
}

size_t Observe(Query& query, World& world, ChangedState& state) {
	size_t rows = 0;
	Check(query.Each(world, [&](QueryRow&) -> Result<void> { ++rows; return {}; }, &state));
	return rows;
}

std::vector<EntityId> Populate(World& world, size_t count, uint64_t prefix);

void VerifyEntrypointErrors() {
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.EntryPosition");
	World world(context), empty(context);
	(void)Populate(world, 1, 0);
	auto query = MakeQuery(context, position);
	Timeline timeline(context, {.Mode = TimelineMode::Serial});
	Throws([&] { Timeline duplicate(context, {.Mode = TimelineMode::Serial}); });
	const auto invalid = TaskHandle{};
	Require(!invalid && invalid.Status() == TaskStatus::Invalid && !timeline.Wait(invalid), "Expected a default task handle to be invalid");
	bool wrongThread = false, wrongConstructorThread = false;
	std::thread worker([&] {
		const auto result = timeline.Submit(query, world, [](TaskBatch&) -> Result<void> { return {}; });
		wrongThread = !result && result.GetError().Code == ErrorCode::WrongThread;
		try { Timeline other(context, {.Mode = TimelineMode::Serial}); } catch (const std::logic_error&) { wrongConstructorThread = true; }
	});
	worker.join();
	Require(wrongThread && wrongConstructorThread, "Expected Timeline construction and submission on a foreign thread to be rejected");
	EcsContext foreign;
	Register<Position>(foreign, "Timeline.ForeignPosition");
	World foreignWorld(foreign);
	Timeline foreignTimeline(foreign, {.Mode = TimelineMode::Serial});
	Require(!timeline.Submit(query, foreignWorld, [](TaskBatch&) -> Result<void> { return {}; }), "Expected a foreign World Context to be rejected");
	size_t callbacks = 0;
	const auto task = Take(timeline.Submit(query, empty, [&](TaskBatch&) -> Result<void> { ++callbacks; return {}; }));
	Require(task.Status() == TaskStatus::Queued && !foreignTimeline.Wait(task), "Expected a queued handle to retain its originating Timeline identity");
	const auto direct = query.Each(world, [](QueryRow&) -> Result<void> { return {}; });
	Require(context.HasScheduledWork() && !direct && direct.GetError().Code == ErrorCode::Busy,
		"Expected unfinished scheduled work to prevent direct Query execution from bypassing Timeline order");
	Check(timeline.Wait(task));
	Require(task.Status() == TaskStatus::Succeeded && task.IsComplete() && callbacks == 0 && !context.HasScheduledWork(),
		"Expected an empty matching task to complete without callbacks and release its scheduled reservation");
	TaskHandle overwritten;
	overwritten = Take(timeline.Submit(query, world, [&](TaskBatch&) -> Result<void> { overwritten = {}; return {}; }));
	const auto completion = overwritten;
	Check(timeline.Wait(overwritten));
	Require(!overwritten && completion.Status() == TaskStatus::Succeeded,
		"Expected Wait to retain its completion state when a callback overwrites the caller's handle");
}

std::vector<EntityId> Populate(World& world, size_t count, uint64_t prefix) {
	std::vector<EntityId> result;
	for (size_t index = 0; index < count; ++index) {
		const auto id = Take(world.CreateEmpty("timeline row", {prefix, index + 1}));
		Check(world.Emplace<Position>(id, Position{static_cast<int>(index)}));
		result.push_back(id);
	}
	return result;
}

void VerifySubmissionOwnership() {
	static_assert(!std::is_copy_constructible_v<Timeline> && !std::is_move_constructible_v<Timeline>);
	static_assert(!std::is_copy_constructible_v<World> && !std::is_move_constructible_v<World>);
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.OwnershipPosition");
	World world(context);
	const auto entities = Populate(world, 3, 1);
	Timeline timeline(context, TimelineOptions{.Mode = TimelineMode::Serial});
	std::vector<int> trace;
	auto query = std::make_unique<Query>(MakeQuery(context, position, AccessMode::Write, true));
	auto changed = std::make_unique<ChangedState>();
	const auto task = Take(timeline.Submit(*query, world,
		[token = std::make_unique<int>(10), &trace](TaskBatch& batch) -> Result<void> {
			trace.push_back(*token);
			auto values = Take(batch.View().Column<Position>(0));
			for (size_t row = 0; row < values.Size(); ++row) values.At(row).Value += *token;
			return {};
		}, changed.get()));
	Require(!task.IsComplete() && task.Sequence() != 0 && trace.empty(), "Expected Submit to own a deferred move-only callback");
	const auto resetWhileQueued = changed->Reset();
	Require(!resetWhileQueued && resetWhileQueued.GetError().Code == ErrorCode::Busy, "Expected a queued observer reservation to prevent Reset");
	query.reset(); changed.reset();
	Check(timeline.Wait(task));
	Require(task.IsComplete() && trace == std::vector<int>{10}, "Expected queued work to survive destruction of its external Query and ChangedState");
	for (size_t index = 0; index < entities.size(); ++index) Require(static_cast<const World&>(world).TryGet<Position>(entities[index])->Value == static_cast<int>(index + 10),
		"Expected the captured task to update every declared component");
	Check(timeline.Wait(task)); Check(timeline.Finish());
	Require(trace.size() == 1, "Expected repeated Wait and Finish not to replay completed callbacks");
	Require(!timeline.Wait(TaskHandle{}), "Expected an invalid task handle to be rejected");
}

void VerifySerialDataAndChanged() {
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.SerialPosition");
	World world(context, {.ChunkBytes = 256});
	const auto entities = Populate(world, 64, 2);
	auto writer = MakeQuery(context, position, AccessMode::Write);
	auto reader = MakeQuery(context, position, AccessMode::Read, true);
	ChangedState first, second;
	Timeline timeline(context, TimelineOptions{.Mode = TimelineMode::Serial});
	std::vector<uint64_t> trace;
	size_t firstReadRows = 0, duplicateReadRows = 0, secondReadRows = 0, finalReadRows = 0;
	auto write = [&](int add) {
		return Take(timeline.Submit(writer, world, [&, add](TaskBatch& batch) -> Result<void> {
			trace.push_back(batch.Sequence());
			auto values = Take(batch.View().Column<Position>(0));
			for (size_t row = 0; row < values.Size(); ++row) values.At(row).Value += add;
			return {};
		}));
	};
	auto read = [&](ChangedState& state, size_t& visits, int offset) {
		return Take(timeline.Submit(reader, world, [&, offset](TaskBatch& batch) -> Result<void> {
			trace.push_back(batch.Sequence());
			auto values = Take(batch.View().Column<const Position>(0));
			for (size_t row = 0; row < values.Size(); ++row) {
				const auto index = world.Uuid(batch.View().Entity(row)).Low - 1;
				Require(values.At(row).Value == static_cast<int>(index) + offset, "Expected each reader to see preceding serial writes");
			}
			visits += values.Size();
			return {};
		}, &state));
	};
	const auto initialWrite = write(1);
	const auto initialRead = read(first, firstReadRows, 1);
	const auto duplicateRead = read(first, duplicateReadRows, 1);
	const auto independentRead = read(second, secondReadRows, 1);
	const auto finalWrite = write(2);
	const auto finalRead = read(first, finalReadRows, 3);
	Require(trace.empty() && initialWrite.Sequence() < initialRead.Sequence() && initialRead.Sequence() < duplicateRead.Sequence() &&
		duplicateRead.Sequence() < independentRead.Sequence() && independentRead.Sequence() < finalWrite.Sequence() && finalWrite.Sequence() < finalRead.Sequence(),
		"Expected monotonic task sequence numbers and deferred serial execution");
	Check(timeline.Wait(finalRead));
	Require(std::is_sorted(trace.begin(), trace.end()) && firstReadRows == 64 && duplicateReadRows == 0 && secondReadRows == 64 && finalReadRows == 64,
		"Expected serial task order, shared consumer progress and independent Changed observers");
	Require(initialWrite.IsComplete() && initialRead.IsComplete() && finalWrite.IsComplete(), "Expected waiting for a later serial task to settle its predecessors");
	Check(first.Reset()); Check(second.Reset());
	Check(timeline.Finish());
}

void VerifyCommandsAndTemporaryEntities() {
	using Payload = ECSLifecycleFixtures::MoveOnly;
	Require(Payload::Live == 0, "Expected no leaked command payloads before the command test");
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.CommandsPosition");
	const auto payload = Register<Payload>(context, "Timeline.CommandsPayload");
	World world(context, {.ChunkBytes = 256});
	const auto entities = Populate(world, 80, 3);
	auto query = MakeQuery(context, position);
	Timeline timeline(context, TimelineOptions{.Mode = TimelineMode::Serial});
	struct Pending { uint64_t Task; size_t Batch; TemporaryEntity Created; TemporaryEntity Destroyed; int Value; };
	std::vector<Pending> pending;
	std::vector<TaskHandle> tasks;
	std::optional<ColumnView<const Position>> expired;
	std::string expectedName;
	for (int pass = 0; pass < 2; ++pass) {
		tasks.push_back(Take(timeline.Submit(query, world, [&, pass](TaskBatch& batch) -> Result<void> {
			const int value = pass * 1000 + static_cast<int>(batch.BatchIndex());
			const auto created = Take(batch.Commands().CreateEmpty("deferred create", {3, static_cast<uint64_t>(value + 1000)}));
			const auto destroyed = Take(batch.Commands().CreateEmpty("deferred delete"));
			Check(batch.Commands().Set(created, Take(OwnedValue::Construct<Payload>(*context.Types().Find(payload), value))));
			Check(batch.Commands().Destroy(destroyed));
			const std::string prefix = std::to_string(batch.Sequence()) + ":" + std::to_string(batch.BatchIndex());
			Check(batch.Commands().SetName(entities.front(), prefix + ":first"));
			expectedName = prefix + ":last";
			Check(batch.Commands().SetName(entities.front(), expectedName));
			pending.push_back({batch.Sequence(), batch.BatchIndex(), created, destroyed, value});
			expired = Take(batch.View().Column<const Position>(0));
			return {};
		})));
	}
	Check(timeline.Finish());
	Require(pending.size() > 2 && world.EntityCount() == entities.size() && world.Name(entities.front()) == "timeline row",
		"Expected multiple independent batch buffers to remain invisible before CommitCommands");
	Require(expired && !expired->Valid(), "Expected callback column snapshots to expire before structural commit");
	std::set<uint64_t> namespaces;
	std::pair<uint64_t, size_t> previousOrder{};
	for (const auto& entry : pending) {
		const auto order = std::pair{entry.Task, entry.Batch};
		Require(order > previousOrder, "Expected monotonically ordered task and batch command identities");
		previousOrder = order;
		Require(namespaces.insert(entry.Created.Buffer).second && entry.Created.Buffer == entry.Destroyed.Buffer,
			"Expected each batch to own one distinct temporary entity namespace");
		const auto task = std::find_if(tasks.begin(), tasks.end(), [&](const TaskHandle& handle) { return handle.Sequence() == entry.Task; });
		Require(task != tasks.end() && !timeline.Resolve(*task, entry.Created), "Expected temporary entities to remain unresolved before commit");
	}
	Check(timeline.CommitCommands());
	Require(world.EntityCount() == entities.size() + pending.size() && world.Name(entities.front()) == expectedName,
		"Expected command merge order to be task, batch and local command order");
	for (const auto& entry : pending) {
		const auto task = std::find_if(tasks.begin(), tasks.end(), [&](const TaskHandle& handle) { return handle.Sequence() == entry.Task; });
		const auto created = Take(timeline.Resolve(*task, entry.Created));
		const auto destroyed = Take(timeline.Resolve(*task, entry.Destroyed));
		Require(world.IsAlive(created) && !world.IsAlive(destroyed) && *static_cast<const World&>(world).TryGet<Payload>(created)->Value == entry.Value,
			"Expected each committed temporary identity and move-only payload to remain attached to its own batch");
		const auto& wrongTask = tasks[task == tasks.begin() ? 1 : 0];
		Require(!timeline.Resolve(wrongTask, entry.Created), "Expected one task not to resolve another task's temporary entity");
	}
	const auto count = world.EntityCount();
	Check(timeline.CommitCommands()); Check(timeline.Finish());
	Require(world.EntityCount() == count, "Expected repeated commit not to replay consumed commands");
	auto payloadQuery = MakeQuery(context, payload);
	size_t visible = 0;
	Check(payloadQuery.Each(world, [&](QueryRow&) -> Result<void> { ++visible; return {}; }));
	Require(visible == pending.size(), "Expected a new Query execution to observe committed component structure");
	Check(world.Clear());
	Require(Payload::Live == 0, "Expected clearing committed entities to release every move-only command payload");
}
void VerifyScopes() {
	static_assert(std::is_same_v<decltype(std::declval<WorldReadScope&>().Get()), const World&>);
	static_assert(std::is_same_v<decltype(std::declval<WorldEditScope&>().Get()), World&>);
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.ScopePosition");
	const auto velocity = Register<Velocity>(context, "Timeline.ScopeVelocity");
	World main(context), target(context);
	const auto source = Populate(main, 1, 4).front();
	const auto destination = Populate(target, 1, 5).front();
	auto ordinary = MakeQuery(context, position);
	auto observer = MakeQuery(context, position, AccessMode::Read, true);
	ChangedState state;
	Require(Observe(observer, target, state) == 1, "Expected the initial scope observation");
	QuerySpec spec; spec.Columns = {{position}}; spec.RandomAccesses = {{&target, position, AccessMode::Write}};
	auto randomWriter = Take(Query::Create(context, spec));
	Timeline timeline(context, {.Mode = TimelineMode::Serial});
	const auto task = Take(timeline.Submit(randomWriter, main, [&](TaskBatch& batch) -> Result<void> {
		++Take(Take(batch.View().Random<Position>(target)).TryGet(destination))->Value;
		return {};
	}));
	TaskHandle other;
	{
		auto read = Take(WorldReadScope::Acquire(target));
		Require(task.IsComplete() && read.Get().TryGet<Position>(destination)->Value == 1,
			"Expected ReadScope to settle all tasks that declare the World as a random target");
		const auto blocked = timeline.Submit(randomWriter, main, [](TaskBatch&) -> Result<void> { return {}; });
		Require(!blocked && blocked.GetError().Code == ErrorCode::Busy, "Expected a scope to reject new submissions involving its random target World");
		Require(!WorldEditScope::Acquire(target), "Expected an EditScope to reject an existing ReadScope");
		{
			auto nested = Take(WorldReadScope::Acquire(target));
			Require(&nested.Get() == &target, "Expected nested ReadScopes on the same World to be permitted");
		}
		Check(timeline.CommitCommands());
		bool rejectedThread = false;
		std::thread worker([&] {
			try { (void)read.Get(); } catch (const std::logic_error&) { rejectedThread = true; }
		});
		worker.join();
		Require(rejectedThread, "Expected a scope to reject access from a foreign thread");
		Require(!target.CreateEmpty("forbidden") && target.TryGet<Position>(destination) == nullptr,
			"Expected a read scope to reject structural mutation and mutable raw access");
		other = Take(timeline.Submit(ordinary, main, [](TaskBatch&) -> Result<void> { return {}; }));
	}
	Check(timeline.Wait(other));
	Require(Observe(observer, target, state) == 1 && Observe(observer, target, state) == 0,
		"Expected the random write to publish once while read scopes remain clean");
	{
		auto edit = Take(WorldEditScope::Acquire(target));
		auto moved = std::move(edit);
		Throws([&] { (void)edit.Get(); });
		auto* value = moved.Get().TryGet<Position>(destination);
		Require(value != nullptr, "Expected an EditScope to permit mutable component access");
		value->Value += 10; value->Value += 20;
		Check(moved.Get().Emplace<Velocity>(destination, Velocity{99}));
		Require(!WorldReadScope::Acquire(target) && !WorldEditScope::Acquire(target), "Expected EditScope to be exclusive on its World");
		const auto blocked = timeline.Submit(ordinary, target, [](TaskBatch&) -> Result<void> { return {}; });
		Require(!blocked && blocked.GetError().Code == ErrorCode::Busy, "Expected an active EditScope to reject same-World task submission");
	}
	Require(Observe(observer, target, state) == 1 && static_cast<const World&>(target).TryGet<Position>(destination)->Value == 31 && target.Has(destination, velocity),
		"Expected EditScope exit to publish its final writes and preserve structural edits");
	Require(Observe(observer, target, state) == 0, "Expected a following pure read to acknowledge the edit version");
	try {
		auto edit = Take(WorldEditScope::Acquire(target));
		++edit.Get().TryGet<Position>(destination)->Value;
		throw 3;
	} catch (int) {}
	Require(Observe(observer, target, state) == 1, "Expected exceptional EditScope exit to publish writes and release its reservation");
	const auto recovered = Take(timeline.Submit(ordinary, target, [](TaskBatch&) -> Result<void> { return {}; }));
	Check(timeline.Wait(recovered));
	Check(timeline.Finish());
	auto temporaryWorld = std::make_unique<World>(context);
	std::optional<WorldReadScope> staleScope(Take(WorldReadScope::Acquire(*temporaryWorld)));
	temporaryWorld.reset();
	Throws([&] { (void)staleScope->Get(); });
	staleScope.reset();
	(void)source;
}
void VerifyFailurePropagation() {
	using Payload = ECSLifecycleFixtures::MoveOnly;
	for (int failureKind = 0; failureKind < 3; ++failureKind) {
		Require(Payload::Live == 0, "Expected no leaked command payloads before a failure scenario");
		EcsContext context;
		const auto position = Register<Position>(context, "Timeline.FailurePosition");
		const auto velocity = Register<Velocity>(context, "Timeline.FailureVelocity");
		const auto payload = Register<Payload>(context, "Timeline.FailurePayload");
		World world(context), other(context);
		const auto id = Populate(world, 1, 6).front();
		const auto otherId = Populate(other, 1, 7).front();
		Check(world.Emplace<Velocity>(id, Velocity{1}));
		Check(other.Emplace<Velocity>(otherId, Velocity{2}));
		const auto resource = Take(context.Resources().Register(std::make_shared<Counter>()));
		auto observer = MakeQuery(context, position, AccessMode::Read, true);
		auto velocityObserver = MakeQuery(context, velocity, AccessMode::Read, true);
		ChangedState readState, velocityState, sharedState;
		(void)Observe(observer, world, readState); (void)Observe(velocityObserver, world, velocityState);
		QuerySpec failSpec; failSpec.Columns = {{position, Presence::Required, AccessMode::Write}};
		failSpec.ChangedTypes = {position}; failSpec.ResourceAccesses = {{resource, AccessMode::Write}};
		auto failQuery = Take(Query::Create(context, failSpec));
		auto readPosition = MakeQuery(context, position);
		auto readVelocity = MakeQuery(context, velocity);
		auto consumeVelocity = MakeQuery(context, velocity, AccessMode::Write, true);
		auto otherWriter = MakeQuery(context, position, AccessMode::Write);
		QuerySpec randomSpec; randomSpec.Columns = {{velocity}}; randomSpec.RandomAccesses = {{&world, position, AccessMode::Read}};
		auto randomReader = Take(Query::Create(context, randomSpec));
		QuerySpec resourceSpec; resourceSpec.Columns = {{velocity}}; resourceSpec.ResourceAccesses = {{resource, AccessMode::Read}};
		auto resourceReader = Take(Query::Create(context, resourceSpec));
		Timeline timeline(context, {.Mode = TimelineMode::Serial});
		const auto ownerThread = std::this_thread::get_id();
		const auto failed = Take(timeline.Submit(failQuery, world, [&](TaskBatch& batch) -> Result<void> {
			Require(std::this_thread::get_id() == ownerThread, "Expected explicit Serial mode to execute on its owner thread");
			Take(batch.View().Column<Position>(0)).At(0).Value += 7;
			++Take(batch.View().Resource<Counter>(resource)).get().Value;
			const auto temp = Take(batch.Commands().CreateEmpty("discard failed commands", {6, 900}));
			Check(batch.Commands().Set(temp, Take(OwnedValue::Construct<Payload>(*context.Types().Find(payload), 91))));
			if (failureKind == 1) throw std::runtime_error("injected standard task failure");
			if (failureKind == 2) throw 23;
			return Error{ErrorCode::TaskFailed, "TimelineSmoke", "injected task result failure"};
		}, &sharedState));
		size_t forbiddenCallbacks = 0, independentCallbacks = 0;
		auto forbidden = [&](TaskBatch&) -> Result<void> { ++forbiddenCallbacks; return {}; };
		const auto columnDependent = Take(timeline.Submit(readPosition, world, forbidden));
		const auto randomDependent = Take(timeline.Submit(randomReader, other, forbidden));
		const auto resourceDependent = Take(timeline.Submit(resourceReader, other, forbidden));
		const auto independent = Take(timeline.Submit(readVelocity, world, [&](TaskBatch& batch) -> Result<void> {
			++independentCallbacks;
			(void)Take(batch.Commands().CreateEmpty("discard independent commands", {6, 901}));
			return {};
		}));
		const auto consumerDependent = Take(timeline.Submit(consumeVelocity, world, forbidden, &sharedState));
		const auto otherIndependent = Take(timeline.Submit(otherWriter, other, [&](TaskBatch& batch) -> Result<void> {
			++independentCallbacks;
			Take(batch.View().Column<Position>(0)).At(0).Value = 100;
			return {};
		}));
		const auto wait = timeline.Wait(failed);
		Require(!wait && wait.GetError().Task == failed.Sequence() && failed.Status() == TaskStatus::Failed,
			"Expected failed Result and both exception forms to carry the task sequence");
		Require(!timeline.Submit(readVelocity, world, forbidden) && !timeline.ResetError(),
			"Expected failed execution to stop submissions and refuse reset while tasks remain unsettled");
		const auto finish = timeline.Finish();
		Require(!finish && finish.GetError().Task == failed.Sequence() && forbiddenCallbacks == 0 && independentCallbacks == 2,
			"Expected failure to cancel dependent successors while settling independent work");
		Require(columnDependent.Status() == TaskStatus::Cancelled && randomDependent.Status() == TaskStatus::Cancelled &&
			resourceDependent.Status() == TaskStatus::Cancelled && consumerDependent.Status() == TaskStatus::Cancelled &&
			independent.Status() == TaskStatus::Succeeded && otherIndependent.Status() == TaskStatus::Succeeded,
			"Expected column, random, resource and observer dependencies to propagate cancellation precisely");
		Require(static_cast<const World&>(world).TryGet<Position>(id)->Value == 7 && static_cast<const World&>(other).TryGet<Position>(otherId)->Value == 100,
			"Expected completed writes to remain visible without rollback");
		Require(Observe(observer, world, readState) == 1 && Observe(velocityObserver, world, velocityState) == 0,
			"Expected failed started writes to publish and cancelled tasks not to publish fabricated writes");
		Require(sharedState.ObservationCount() == 0 && Payload::Live == 0 && world.EntityCount() == 1 && !world.Find({6, 900}) && !world.Find({6, 901}),
			"Expected failed observation progress and all uncommitted task commands to be discarded");
		Check(sharedState.Reset());
		Require(timeline.IsIdle() && timeline.LastError() && !timeline.Finish() && !timeline.CommitCommands(),
			"Expected the settled first failure to remain reportable until explicit reset");
		Check(timeline.ResetError());
		Require(!timeline.LastError(), "Expected explicit reset to clear the reported error");
		const auto recovery = Take(timeline.Submit(readVelocity, world, [](TaskBatch&) -> Result<void> { return {}; }));
		Check(timeline.Wait(recovery)); Check(timeline.CommitCommands());
		Require(world.EntityCount() == 1 && other.EntityCount() == 1, "Expected recovery not to replay discarded buffers");
	}
}

void VerifyMaskedCaptureIndependence() {
	for (int mode = 0; mode < 3; ++mode) {
		EcsContext context;
		const auto position = Register<Position>(context, "Timeline.MaskedPosition");
		const auto marker = Take(context.Types().Register<GroupMarker>(TypeGuid::FromName("Timeline.MaskedMarker"), "Timeline.MaskedMarker", true));
		World world(context);
		const auto entities = Populate(world, 2, 20);
		Check(world.SetTag(entities.front(), marker));
		if (mode == 1) Check(world.SetEnabled(entities.front(), false));
		if (mode == 2) Check(world.SetComponentEnabled(entities.front(), position, false));
		QuerySpec firstSpec; firstSpec.Columns = {{position, Presence::Required, AccessMode::Write}}; firstSpec.Required = {marker};
		firstSpec.IncludeDisabledEntities = mode == 1;
		firstSpec.IgnoreComponentEnabled = mode == 2;
		QuerySpec secondSpec; secondSpec.Columns = {{position, Presence::Required, AccessMode::Write}};
		if (mode == 0) secondSpec.Exclude = {marker};
		auto firstQuery = Take(Query::Create(context, firstSpec));
		auto secondQuery = Take(Query::Create(context, secondSpec));
		Timeline timeline(context, {.Mode = TimelineMode::Serial});
		const auto failed = Take(timeline.Submit(firstQuery, world, [](TaskBatch& batch) -> Result<void> {
			Take(batch.View().Column<Position>(0)).At(0).Value += 5;
			return Error{ErrorCode::TaskFailed, "MaskedCapture", "injected failure in the excluded chunk"};
		}));
		size_t visited = 0;
		const auto independent = Take(timeline.Submit(secondQuery, world, [&](TaskBatch& batch) -> Result<void> {
			auto values = Take(batch.View().Column<Position>(0));
			for (size_t row = 0; row < values.Size(); ++row) { ++visited; values.At(row).Value += 10; }
			return {};
		}));
		Require(!timeline.Wait(failed) && !timeline.Finish(), "Expected the deliberately failed task to remain reportable");
		Require(independent.Status() == TaskStatus::Succeeded && visited == 1 &&
			static_cast<const World&>(world).TryGet<Position>(entities.back())->Value == 11,
			"Expected Exclude, entity masks and component masks to omit empty-match chunks from captured dependency access");
		Check(timeline.ResetError());
	}
}
void VerifyCommandFailureAndRecovery() {
	using Payload = ECSLifecycleFixtures::MoveOnly;
	using Constructor = ECSLifecycleFixtures::ThrowingDefault;
	using Copy = ECSLifecycleFixtures::ThrowingCopy;
	Require(Payload::Live == 0 && Constructor::Live == 0 && Copy::Live == 0, "Expected balanced lifetimes before commit failure tests");
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.CommitPosition");
	const auto payload = Register<Payload>(context, "Timeline.CommitPayload");
	const auto constructor = Register<Constructor>(context, "Timeline.CommitConstructor");
	Register<Copy>(context, "Timeline.CommitCopy");
	World world(context);
	const auto id = Populate(world, 1, 8).front();
	auto query = MakeQuery(context, position);
	Timeline timeline(context, {.Mode = TimelineMode::Serial});
	TemporaryEntity prefix, suffix, later;
	const auto first = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		prefix = Take(batch.Commands().CreateEmpty("successful prefix", {8, 800}));
		Check(batch.Commands().Set(prefix, Take(OwnedValue::Construct<Payload>(*context.Types().Find(payload), 31))));
		Check(batch.Commands().SetName(id, "committed before failure"));
		Check(batch.Commands().AddDefault(id, constructor));
		suffix = Take(batch.Commands().CreateEmpty("unapplied suffix", {8, 801}));
		Check(batch.Commands().Set(suffix, Take(OwnedValue::Construct<Payload>(*context.Types().Find(payload), 32))));
		return {};
	}));
	const auto second = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		later = Take(batch.Commands().CreateEmpty("unapplied later task", {8, 802}));
		Check(batch.Commands().Set(later, Take(OwnedValue::Construct<Payload>(*context.Types().Find(payload), 33))));
		return {};
	}));
	Check(timeline.Finish());
	Require(Payload::Live == 3 && world.EntityCount() == 1, "Expected all uncommitted move-only values to be owned by their buffers");
	Constructor::Fail = true;
	const auto failed = timeline.CommitCommands();
	Constructor::Fail = false;
	Require(!failed && failed.GetError().Task == first.Sequence() && failed.GetError().Command == 4,
		"Expected commit failure to identify the task and one-based local command position");
	const auto committed = Take(timeline.Resolve(first, prefix));
	Require(world.EntityCount() == 2 && world.Name(id) == "committed before failure" && world.IsAlive(committed) &&
		*static_cast<const World&>(world).TryGet<Payload>(committed)->Value == 31 && !world.Has(id, constructor),
		"Expected the successful command prefix to remain committed and the failing constructor to preserve existing state");
	Require(Payload::Live == 1 && Constructor::Live == 0 && !timeline.Resolve(first, suffix) && !timeline.Resolve(second, later),
		"Expected failing and unexecuted command payloads to be released without fabricated temporary mappings");
	Require(!timeline.CommitCommands() && !timeline.Finish() && world.EntityCount() == 2, "Expected repeated failure reporting not to replay a successful prefix");
	Check(timeline.ResetError());
	const auto recovered = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		Check(batch.Commands().SetName(id, "recovered"));
		return {};
	}));
	Check(timeline.Wait(recovered)); Check(timeline.CommitCommands());
	Require(world.Name(id) == "recovered" && world.EntityCount() == 2 && !world.Find({8, 801}) && !world.Find({8, 802}),
		"Expected a new commit after reset not to execute a previous failure's suffix");
	{
		auto edit = Take(WorldEditScope::Acquire(world));
		Check(edit.Get().Emplace<Copy>(id, "original copy source"));
	}
	TemporaryEntity failedClone;
	const auto cloning = Take(timeline.Submit(query, world, [&](TaskBatch& batch) -> Result<void> {
		failedClone = Take(batch.Commands().Clone(id, "failed clone", {8, 900}));
		return {};
	}));
	Check(timeline.Wait(cloning));
	Copy::Fail = true;
	const auto cloneFailure = timeline.CommitCommands();
	Copy::Fail = false;
	Require(!cloneFailure && cloneFailure.GetError().Task == cloning.Sequence() && cloneFailure.GetError().Command == 1 &&
		!timeline.Resolve(cloning, failedClone) && world.EntityCount() == 2 && static_cast<const World&>(world).TryGet<Copy>(id)->Text == "original copy source",
		"Expected failing entity cloning to preserve its source and leave no temporary entity mapping");
	Check(timeline.ResetError());
	Check(world.Clear());
	Require(Payload::Live == 0 && Constructor::Live == 0 && Copy::Live == 0, "Expected every commit failure payload and source object to be destroyed exactly once");
}

void VerifyShutdownAndWorldLifetime() {
	using Payload = ECSLifecycleFixtures::MoveOnly;
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.ShutdownPosition");
	const auto payload = Register<Payload>(context, "Timeline.ShutdownPayload");
	World world(context);
	const auto id = Populate(world, 1, 9).front();
	auto writer = MakeQuery(context, position, AccessMode::Write);
	TaskHandle retained;
	size_t callbacks = 0;
	{
		Timeline timeline(context, {.Mode = TimelineMode::Serial});
		retained = Take(timeline.Submit(writer, world, [&](TaskBatch& batch) -> Result<void> {
			++callbacks;
			Take(batch.View().Column<Position>(0)).At(0).Value = 55;
			const auto temp = Take(batch.Commands().CreateEmpty("never explicitly committed", {9, 700}));
			Check(batch.Commands().Set(temp, Take(OwnedValue::Construct<Payload>(*context.Types().Find(payload), 44))));
			return {};
		}));
	}
	Require(callbacks == 1 && retained.Status() == TaskStatus::Succeeded && world.EntityCount() == 1 &&
		static_cast<const World&>(world).TryGet<Position>(id)->Value == 55 && Payload::Live == 0 && !world.Find({9, 700}),
		"Expected Timeline destruction to settle deferred writes without implicitly committing structural commands");
	Timeline timeline(context, {.Mode = TimelineMode::Serial});
	Require(!timeline.Wait(retained), "Expected a replacement Timeline to reject a completed handle from an older Timeline in the same Context");
	auto victim = std::make_unique<World>(context);
	(void)Populate(*victim, 1, 10);
	const auto queued = Take(timeline.Submit(writer, *victim, [&](TaskBatch& batch) -> Result<void> {
		++callbacks;
		Take(batch.View().Column<Position>(0)).At(0).Value = 66;
		return {};
	}));
	victim.reset();
	Require(queued.Status() == TaskStatus::Succeeded && callbacks == 2, "Expected World destruction to settle queued tasks before invalidating its native storage");
	auto randomTarget = std::make_unique<World>(context);
	const auto randomId = Populate(*randomTarget, 1, 11).front();
	World* rawTarget = randomTarget.get();
	QuerySpec randomSpec; randomSpec.Columns = {{position}}; randomSpec.RandomAccesses = {{rawTarget, position, AccessMode::Write}};
	auto randomQuery = Take(Query::Create(context, randomSpec));
	const auto random = Take(timeline.Submit(randomQuery, world, [&](TaskBatch& batch) -> Result<void> {
		++callbacks;
		Take(Take(batch.View().Random<Position>(*rawTarget)).TryGet(randomId))->Value = 77;
		return {};
	}));
	randomTarget.reset();
	Require(random.Status() == TaskStatus::Succeeded && callbacks == 3, "Expected World destruction to settle queued tasks that only declare it through Random");
	Check(timeline.Finish());
	victim = std::make_unique<World>(context, WorldOptions{.ChunkBytes = 256});
	(void)Populate(*victim, 32, 12);
	Require(victim->Chunks().size() > 1, "Expected multiple captured batches in the self-destruction task");
	const auto selfDestruct = Take(timeline.Submit(writer, *victim, [&](TaskBatch& batch) -> Result<void> {
		++callbacks;
		auto values = Take(batch.View().Column<Position>(0));
		const int original = values.At(0).Value;
		auto& value = values.At(0);
		victim.reset();
		Require(!batch.View().Valid() && value.Value == original, "Expected callback destruction to invalidate views but retain borrowed native storage until return");
		return {};
	}));
	const auto failed = timeline.Wait(selfDestruct);
	Require(!failed && failed.GetError().Task == selfDestruct.Sequence() && failed.GetError().Code == ErrorCode::InvalidState && callbacks == 4,
		"Expected self-destruction during a callback to fail without self-waiting or continuing later batches");
	Require(!timeline.Finish(), "Expected the lifetime failure to remain reportable after settlement");
	Check(timeline.ResetError());
	Check(timeline.Finish());
}

void VerifyTimelineDestructionDuringCallback() {
	using Capture = ECSLifecycleFixtures::NoDefault;
	using Payload = ECSLifecycleFixtures::MoveOnly;
	Require(Capture::Live == 0 && Payload::Live == 0, "Expected balanced lifetimes before Timeline self-destruction");
	EcsContext context;
	const auto position = Register<Position>(context, "Timeline.SelfDestructionPosition");
	const auto payload = Register<Payload>(context, "Timeline.SelfDestructionPayload");
	World world(context), other(context);
	const auto id = Populate(world, 1, 30).front();
	const auto otherId = Populate(other, 1, 31).front();
	auto writer = MakeQuery(context, position, AccessMode::Write, true);
	auto reader = MakeQuery(context, position);
	auto independentWriter = MakeQuery(context, position, AccessMode::Write);
	ChangedState state;
	auto owner = std::make_unique<Timeline>(context, TimelineOptions{.Mode = TimelineMode::Serial});
	size_t dependentCalls = 0, independentCalls = 0;
	const auto first = Take(owner->Submit(writer, world,
		[token = std::make_unique<Capture>(123), &owner, &context, payload](TaskBatch& batch) -> Result<void> {
			const auto temp = Take(batch.Commands().CreateEmpty("discard closing commands", {30, 900}));
			Check(batch.Commands().Set(temp, Take(OwnedValue::Construct<Payload>(*context.Types().Find(payload), 901))));
			owner.reset();
			Require(Capture::Live == 1 && token->Value == 123, "Expected Timeline destruction not to destroy a currently invoking move-only callback");
			++token->Value;
			Throws([&] { Timeline premature(context, {.Mode = TimelineMode::Serial}); });
			Take(batch.View().Column<Position>(0)).At(0).Value = token->Value;
			return Error{ErrorCode::TaskFailed, "TimelineSelfDestruction", "injected failure after closing the facade"};
		}, &state));
	const auto dependent = Take(owner->Submit(reader, world, [&](TaskBatch&) -> Result<void> { ++dependentCalls; return {}; }));
	const auto independent = Take(owner->Submit(independentWriter, other, [&](TaskBatch& batch) -> Result<void> {
		++independentCalls;
		Take(batch.View().Column<Position>(0)).At(0).Value = 456;
		(void)Take(batch.Commands().CreateEmpty("discard independent closing commands", {31, 900}));
		return {};
	}));
	const auto result = owner->Wait(first);
	Require(!owner && !result && result.GetError().Task == first.Sequence() && first.Status() == TaskStatus::Failed &&
		dependent.Status() == TaskStatus::Cancelled && independent.Status() == TaskStatus::Succeeded,
		"Expected callback Timeline destruction to settle all handles while retaining dependency failure semantics");
	Require(dependentCalls == 0 && independentCalls == 1 && Capture::Live == 0 && Payload::Live == 0 && !context.HasScheduledWork(),
		"Expected deferred shutdown to release callback ownership, commands and scheduled reservations exactly once");
	Require(world.EntityCount() == 1 && other.EntityCount() == 1 && static_cast<const World&>(world).TryGet<Position>(id)->Value == 124 &&
		static_cast<const World&>(other).TryGet<Position>(otherId)->Value == 456,
		"Expected closing to preserve completed writes without committing buffered entity creation");
	Check(state.Reset());
	Timeline replacement(context, {.Mode = TimelineMode::Serial});
	const auto recovery = Take(replacement.Submit(reader, world, [](TaskBatch&) -> Result<void> { return {}; }));
	Check(replacement.Wait(recovery)); Check(replacement.Finish());
}
}

int main() {
	VerifyEntrypointErrors();
	VerifySubmissionOwnership();
	VerifySerialDataAndChanged();
	VerifyCommandsAndTemporaryEntities();
	VerifyScopes();
	VerifyFailurePropagation();
	VerifyMaskedCaptureIndependence();
	VerifyCommandFailureAndRecovery();
	VerifyShutdownAndWorldLifetime();
	VerifyTimelineDestructionDuringCallback();
#if defined(__SANITIZE_ADDRESS__)
	std::cout << "AddressSanitizer instrumentation is enabled\n";
#endif
	std::cout << "ECSTimelineSmoke passed\n";
	return 0;
}
