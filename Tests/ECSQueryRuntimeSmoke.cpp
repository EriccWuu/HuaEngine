#include "ECSLifecycleFixtures.h"
#include "HuaEngine/ECS/Runtime/World.h"
#include "HuaEngine/ECS/Runtime/Query.h"
#include "HuaEngine/ECS/Runtime/Commands.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <type_traits>

#if defined(HUAENGINE_ASAN_REQUIRED) && !defined(__SANITIZE_ADDRESS__)
#error The standalone query target must actually enable AddressSanitizer.
#endif

namespace {
using namespace HE::Ecs;
using HE::EntityId;
using HE::EntityUuid;

struct Position { int Value = 0; };
struct Velocity { int Value = 0; };
struct Label { std::string Text; };
struct Marker {};
struct Counter { int Value = 0; };

void Require(bool condition, const char* message) {
	if (!condition) { std::cerr << "[ECSQueryRuntimeSmoke] " << message << '\n'; std::exit(1); }
}

template<typename T>
T Take(Result<T> result) {
	if (!result) {
		std::cerr << "[ECSQueryRuntimeSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n';
		std::exit(1);
	}
	return std::move(result).Value();
}

void Check(Result<void> result) {
	if (!result) {
		std::cerr << "[ECSQueryRuntimeSmoke] " << result.GetError().Operation << ": " << result.GetError().Message << '\n';
		std::exit(1);
	}
}

template<typename T>
TypeId Register(EcsContext& context, const char* name, bool tag = false) {
	return Take(context.Types().Register<T>(TypeGuid::FromName(name), name, tag));
}

std::set<uint64_t> Matching(Query& query, World& world, ChangedState* changed = nullptr) {
	std::set<uint64_t> result;
	Check(query.Each(world, [&](QueryRow& row) -> Result<void> {
		Require(row.Valid() && world.IsAlive(row.Entity()), "Expected a live entity bound to every query row");
		Require(result.insert(world.Uuid(row.Entity()).Low).second, "Expected each entity to occur at most once per execution");
		return {};
	}, changed));
	return result;
}

template<typename Action>
void ThrowsLogicError(Action&& action) {
	bool rejected = false;
	try { action(); }
	catch (const std::logic_error&) { rejected = true; }
	Require(rejected, "Expected invalid column access to throw a defined logic_error");
}

void VerifyFilteringAndDeclarations() {
	EcsContext context;
	const auto position = Register<Position>(context, "Query.FilterPosition");
	const auto velocity = Register<Velocity>(context, "Query.FilterVelocity");
	const auto marker = Register<Marker>(context, "Query.FilterMarker", true);
	World world(context);
	struct Model { bool Entity; bool Position; bool Velocity; bool Marker; bool PositionEnabled; bool VelocityEnabled; bool MarkerEnabled; };
	std::vector<Model> model;
	for (uint64_t index = 0; index < 32; ++index) {
		const auto id = Take(world.CreateEmpty("filter row", {1, index + 1}));
		const Model value{index % 7 != 0, (index & 1) != 0, (index & 2) != 0, (index & 4) != 0,
			(index & 8) == 0, (index & 16) == 0, index % 3 != 0};
		if (value.Position) { Check(world.Emplace<Position>(id, Position{static_cast<int>(index)})); Check(world.SetComponentEnabled(id, position, value.PositionEnabled)); }
		if (value.Velocity) { Check(world.Emplace<Velocity>(id, Velocity{static_cast<int>(100 + index)})); Check(world.SetComponentEnabled(id, velocity, value.VelocityEnabled)); }
		if (value.Marker) { Check(world.SetTag(id, marker)); Check(world.SetComponentEnabled(id, marker, value.MarkerEnabled)); }
		Check(world.SetEnabled(id, value.Entity));
		model.push_back(value);
	}
	auto expected = [&](bool includeDisabled, bool ignoreComponents, auto predicate) {
		std::set<uint64_t> result;
		for (size_t index = 0; index < model.size(); ++index) {
			const auto& row = model[index];
			if (!row.Entity && !includeDisabled) continue;
			if (predicate(row.Position && (ignoreComponents || row.PositionEnabled), row.Velocity && (ignoreComponents || row.VelocityEnabled),
				row.Marker && (ignoreComponents || row.MarkerEnabled))) result.insert(index + 1);
		}
		return result;
	};
	for (bool includeDisabled : {false, true}) {
		for (bool ignoreComponents : {false, true}) {
			QuerySpec spec;
			spec.Columns = {{position, Presence::Required, AccessMode::Read}, {velocity, Presence::Optional, AccessMode::Read}};
			spec.Required = {marker, marker};
			spec.IncludeDisabledEntities = includeDisabled;
			spec.IgnoreComponentEnabled = ignoreComponents;
			auto query = Take(Query::Create(context, spec));
			Require(Matching(query, world) == expected(includeDisabled, ignoreComponents, [](bool p, bool, bool tag) { return p && tag; }),
				"Expected Required, Optional and Tag filters to respect entity and component masks independently");
			Check(query.Each(world, [&](QueryRow& row) -> Result<void> {
				const auto index = static_cast<size_t>(world.Uuid(row.Entity()).Low - 1);
				Require(Take(row.Get<const Position>(0)).get().Value == static_cast<int>(index), "Expected argument order to bind the required column");
				const auto* optional = Take(row.Optional<const Velocity>(1));
				const bool present = model[index].Velocity && (ignoreComponents || model[index].VelocityEnabled);
				Require((optional != nullptr) == present, "Expected a disabled optional component to appear absent");
				if (optional) Require(optional->Value == static_cast<int>(100 + index), "Expected optional values from their own row");
				Require(!row.Get<Position>(0) && !row.Get<const Velocity>(0) && !row.Get<const Position>(99),
					"Expected write-through-read, wrong native type and wrong argument index to fail");
				return {};
			}));
		}
	}
	QuerySpec excluded;
	excluded.Columns = {{position, Presence::Optional, AccessMode::Read}};
	excluded.Exclude = {position};
	auto absent = Take(Query::Create(context, excluded));
	Require(Matching(absent, world) == expected(false, false, [](bool p, bool, bool) { return !p; }),
		"Expected Optional intersect Exclude to remain legal, including physically present disabled components");
	Check(absent.Each(world, [](QueryRow& row) -> Result<void> {
		Require(Take(row.Optional<const Position>(0)) == nullptr, "Expected an excluded optional component to be absent on every matched row");
		return {};
	}));
	QuerySpec reordered;
	reordered.Columns = {{velocity, Presence::Optional, AccessMode::Read}, {position, Presence::Required, AccessMode::Read}, {position, Presence::Optional, AccessMode::Read}};
	auto ordered = Take(Query::Create(context, reordered));
	Check(ordered.Each(world, [](QueryRow& row) -> Result<void> {
		Require(&Take(row.Get<const Position>(1)).get() == &Take(row.Get<const Position>(2)).get(),
			"Expected duplicate terms to preserve parameter order while binding the same component");
		return {};
	}));
	QuerySpec empty;
	auto everything = Take(Query::Create(context, empty));
	Require(Matching(everything, world) == expected(false, false, [](bool, bool, bool) { return true; }), "Expected an empty Query to select all enabled entities");
	World noEntities(context);
	Require(Matching(everything, noEntities).empty(), "Expected an empty World to produce no callbacks");
	for (const auto invalid : {TypeId{0}, std::numeric_limits<TypeId>::max(), marker}) {
		QuerySpec bad; bad.Columns = {{invalid}};
		Require(!Query::Create(context, bad), "Expected unknown types and Tag columns to be rejected");
	}
	QuerySpec contradictory; contradictory.Columns = {{position}}; contradictory.Exclude = {position};
	Require(!Query::Create(context, contradictory), "Expected implicit Required intersect Exclude to be rejected");
	EcsContext foreign;
	World foreignWorld(foreign);
	Require(!everything.Each(foreignWorld, [](QueryRow&) -> Result<void> { return {}; }), "Expected execution with a foreign Context to be rejected");
}

void VerifyColumnsAndExpiredViews() {
	static_assert(std::is_same_v<decltype(std::declval<ColumnView<const Position>>().At(0)), const Position&>);
	EcsContext context;
	const auto position = Register<Position>(context, "Query.ColumnPosition");
	const auto label = Register<Label>(context, "Query.ColumnLabel");
	const auto indirect = Register<ECSLifecycleFixtures::IndirectAligned>(context, "Query.ColumnIndirect");
	World world(context);
	std::vector<EntityId> entities;
	for (int index = 0; index < 8; ++index) {
		const auto id = Take(world.CreateEmpty("column", {2, static_cast<uint64_t>(index + 1)}));
		Check(world.Emplace<Position>(id, Position{index}));
		Check(world.Emplace<Label>(id, Label{"label-" + std::to_string(index)}));
		Check(world.Emplace<ECSLifecycleFixtures::IndirectAligned>(id, index));
		entities.push_back(id);
	}
	QuerySpec spec; spec.Columns = {{position}, {label}, {indirect}};
	auto query = Take(Query::Create(context, spec));
	std::set<int> eachValues;
	std::optional<QueryRow> retainedRow;
	Check(query.Each(world, [&](QueryRow& row) -> Result<void> {
		eachValues.insert(Take(row.Get<const Position>(0)).get().Value);
		retainedRow = row;
		return {};
	}));
	std::set<int> batchValues;
	std::optional<ColumnView<const Position>> retainedColumn;
	QueryBatch retainedBatch;
	Check(query.Batches(world, [&](QueryBatch& batch) -> Result<void> {
		auto positions = Take(batch.Column<const Position>(0));
		auto labels = Take(batch.Column<const Label>(1));
		auto indirects = Take(batch.Column<const ECSLifecycleFixtures::IndirectAligned>(2));
		Require(positions.Size() == batch.Size() && positions.TryAsSpan() && labels.TryAsSpan() && !indirects.TryAsSpan(),
			"Expected spans only for contiguous direct columns, including non-POD direct objects");
		for (size_t row = 0; row < batch.Size(); ++row) {
			const int value = positions.At(row).Value;
			batchValues.insert(value);
			Require(labels.At(row).Text == "label-" + std::to_string(value) && indirects.At(row).Value == value,
				"Expected all logical column rows to refer to the same entity");
		}
		Require(!positions.TryGet(batch.Size()), "Expected out-of-range column access to return null");
		ThrowsLogicError([&] { (void)positions.At(batch.Size()); });
		retainedColumn = positions; retainedBatch = batch;
		return {};
	}));
	Require(eachValues == batchValues && eachValues.size() == 8, "Expected Each and Batches to visit the same values");
	Require(retainedRow && !retainedRow->Valid() && !retainedRow->Get<const Position>(0), "Expected retained row access to reject an expired execution");
	Require(retainedColumn && !retainedColumn->Valid() && retainedColumn->Size() == 0 && !retainedColumn->TryGet(0) && !retainedColumn->TryAsSpan(),
		"Expected retained column views to fail safely after execution");
	ThrowsLogicError([&] { (void)retainedColumn->At(0); });
	Require(!retainedBatch.Valid() && !retainedBatch.Column<const Position>(0), "Expected retained batch bindings to expire");
	Check(world.SetEnabled(entities[1], false));
	Check(world.SetEnabled(entities[3], false));
	Check(query.Batches(world, [&](QueryBatch& batch) -> Result<void> {
		auto positions = Take(batch.Column<const Position>(0));
		Require(batch.Size() == 6 && !positions.TryAsSpan(), "Expected filtered physical-row holes to prevent direct span exposure");
		for (size_t row = 0; row < batch.Size(); ++row) Require(positions.At(row).Value != 1 && positions.At(row).Value != 3,
			"Expected logical row mapping to omit disabled physical rows");
		return {};
	}));
	Check(world.SetEnabled(entities[1], true)); Check(world.SetEnabled(entities[3], true));
	Check(world.SetComponentEnabled(entities[2], label, false));
	QuerySpec optional; optional.Columns = {{position}, {label, Presence::Optional, AccessMode::Read}};
	auto optionalQuery = Take(Query::Create(context, optional));
	Check(optionalQuery.Batches(world, [](QueryBatch& batch) -> Result<void> {
		auto positions = Take(batch.Column<const Position>(0));
		auto labels = Take(batch.Column<const Label>(1));
		Require(!labels.TryAsSpan(), "Expected any absent optional row to prevent a span");
		for (size_t row = 0; row < batch.Size(); ++row) {
			Require(labels.Has(row) == (positions.At(row).Value != 2), "Expected optional presence to follow logical entity rows");
			if (!labels.Has(row)) ThrowsLogicError([&] { (void)labels.At(row); });
		}
		return {};
	}));
}

void VerifyChangedObservers() {
	EcsContext context;
	const auto position = Register<Position>(context, "Query.ChangedPosition");
	const auto velocity = Register<Velocity>(context, "Query.ChangedVelocity");
	const auto marker = Register<Marker>(context, "Query.ChangedMarker", true);
	World world(context, { .ChunkBytes = 512 });
	std::vector<EntityId> entities;
	for (uint64_t index = 0; index < 96; ++index) {
		const auto id = Take(world.CreateEmpty("changed", {3, index + 1}));
		Check(world.Emplace<Position>(id, Position{static_cast<int>(index)}));
		Check(world.Emplace<Velocity>(id, Velocity{static_cast<int>(index)}));
		entities.push_back(id);
	}
	QuerySpec batchesSpec; batchesSpec.Columns = {{position}};
	auto batchQuery = Take(Query::Create(context, batchesSpec));
	std::optional<ColumnView<const Position>> previousBatch;
	size_t batchCount = 0;
	Check(batchQuery.Batches(world, [&](QueryBatch& batch) -> Result<void> {
		if (previousBatch) Require(!previousBatch->Valid() && !previousBatch->TryGet(0),
			"Expected a preceding batch's column view to expire before the next callback");
		previousBatch = Take(batch.Column<const Position>(0));
		++batchCount;
		return {};
	}));
	Require(batchCount > 1, "Expected the Changed workload to exercise multiple independent chunks");
	QuerySpec spec; spec.Columns = {{position}}; spec.ChangedTypes = {position};
	auto query = Take(Query::Create(context, spec));
	ChangedState first, second;
	Require(first.ConsumerId() != second.ConsumerId() && first.ConsumerId() != 0, "Expected stable distinct Changed consumer identities");
	Require(Matching(query, world, &first).size() == 96 && Matching(query, world, &first).empty(),
		"Expected first observation to consume all matching rows and a pure read not to mark them dirty");
	Require(Matching(query, world, &second).size() == 96, "Expected another observer not to consume the first observer's progress");
	Require(!query.Each(world, [](QueryRow&) -> Result<void> { return {}; }), "Expected Changed execution without an observer to fail explicitly");
	const auto location = world.Location(entities.front());
	const auto changedChunkRows = Take(world.InspectChunk({location.Chunk, location.ChunkGeneration})).Entities.size();
	world.TryGet<Position>(entities.front())->Value = 901;
	Check(world.PublishWrite(entities.front(), position));
	Require(Matching(query, world, &first).size() == changedChunkRows && Matching(query, world, &second).size() == changedChunkRows,
		"Expected a published write to expose exactly its column's chunk to both observers");
	Require(Matching(query, world, &first).empty(), "Expected successful acknowledgement to suppress an unchanged column");
	QuerySpec unionSpec; unionSpec.Columns = {{position}, {velocity, Presence::Optional, AccessMode::Read}};
	unionSpec.ChangedTypes = {velocity, position, velocity};
	auto either = Take(Query::Create(context, unionSpec));
	ChangedState unionState;
	Require(Matching(either, world, &unionState).size() == 96, "Expected initial observation of multiple Changed columns");
	world.TryGet<Velocity>(entities.front())->Value = 902;
	Check(world.PublishWrite(entities.front(), velocity));
	Require(Matching(either, world, &unionState).size() == changedChunkRows && Matching(query, world, &first).empty(),
		"Expected Changed columns to use OR while a distinct untouched column remains clean");
	QuerySpec reordered = unionSpec;
	reordered.Columns = {{velocity, Presence::Optional, AccessMode::Read}, {position, Presence::Required, AccessMode::Write}};
	reordered.ChangedTypes = {position, velocity};
	auto equivalent = Take(Query::Create(context, reordered));
	Require(equivalent.FilterIdentity() == either.FilterIdentity() && Matching(equivalent, world, &unionState).empty(),
		"Expected normalized filters to share observation progress across parameter order and access declarations");
	QuerySpec distinct = unionSpec; distinct.Exclude = {marker};
	auto isolated = Take(Query::Create(context, distinct));
	Require(isolated.FilterIdentity() != either.FilterIdentity() && Matching(isolated, world, &unionState).size() == 96,
		"Expected different filter identities to keep independent observation progress within one state");

	Check(first.Reset());
	size_t callbacks = 0;
	const auto failure = query.Batches(world, [&](QueryBatch&) -> Result<void> {
		if (++callbacks == 2) return Error{ErrorCode::TaskFailed, "ChangedSmoke", "injected second-batch failure"};
		return {};
	}, &first);
	Require(!failure && callbacks == 2 && first.ObservationCount() == 0, "Expected failure after a successful batch not to acknowledge any of the consumer round");
	Require(Matching(query, world, &first).size() == 96 && Matching(query, world, &first).empty(),
		"Expected retry to revisit the entire failed round, including its successful prefix");
	QuerySpec writes = spec; writes.Columns.front().Access = AccessMode::Write;
	auto writer = Take(Query::Create(context, writes));
	ChangedState writerState;
	const auto writeFailure = writer.Batches(world, [&](QueryBatch& batch) -> Result<void> {
		auto values = Take(batch.Column<Position>(0));
		++values.At(0).Value;
		return Error{ErrorCode::TaskFailed, "ChangedSmoke", "injected failure after a real write"};
	}, &writerState);
	Require(!writeFailure && writerState.ObservationCount() == 0 && !Matching(query, world, &first).empty(),
		"Expected failed callbacks to publish started writes while leaving their own observations unacknowledged");
	Check(writerState.Reset());
	Check(writer.Each(world, [](QueryRow& row) -> Result<void> { ++Take(row.Get<Position>(0)).get().Value; return {}; }, &writerState));
	Require(Matching(query, world, &writerState).size() == 96,
		"Expected observation to record entry versions rather than hiding writes made by its own callback");
	Require(Matching(query, world, &writerState).empty(), "Expected the following pure reader to acknowledge the published versions");
	Check(writerState.Reset());
	const auto thrown = writer.Each(world, [&](QueryRow& row) -> Result<void> {
		++Take(row.Get<Position>(0)).get().Value;
		throw 17;
	}, &writerState);
	Require(!thrown && thrown.GetError().Code == ErrorCode::TaskFailed && writerState.ObservationCount() == 0,
		"Expected a non-standard callback exception to fail without acknowledging any Changed progress");
	Require(!Matching(query, world, &first).empty(), "Expected writes preceding a thrown callback to remain observable");
	Check(writerState.Reset());
	Check(query.Each(world, [&](QueryRow&) -> Result<void> {
		const auto reset = writerState.Reset();
		const auto nested = query.Each(world, [](QueryRow&) -> Result<void> { return {}; }, &second);
		Require(!reset && reset.GetError().Code == ErrorCode::Busy && !nested && nested.GetError().Code == ErrorCode::Busy,
			"Expected active Changed reset and same-Context query re-entry to reject unfinished observation");
		return {};
	}, &writerState));
	std::vector<EntityId> chunkHeads;
	Check(batchQuery.Batches(world, [&](QueryBatch& batch) -> Result<void> { chunkHeads.push_back(batch.Entity(0)); return {}; }));
	ChangedState crossChunkState;
	(void)Matching(query, world, &crossChunkState);
	Check(world.PublishWrite(chunkHeads.front(), position));
	QuerySpec crossChunkSpec = spec; crossChunkSpec.RandomAccesses = {{&world, position, AccessMode::Write}};
	auto crossChunk = Take(Query::Create(context, crossChunkSpec));
	size_t changedBatches = 0;
	Check(crossChunk.Batches(world, [&](QueryBatch& batch) -> Result<void> {
		++changedBatches;
		if (batch.Entity(0) == chunkHeads.front()) {
			Take(Take(batch.Random<Position>(world)).TryGet(chunkHeads.back()))->Value = 1937;
		} else {
			Require(batch.Entity(0) == chunkHeads.back() && Take(batch.Column<const Position>(0)).At(0).Value == 1937,
				"Expected a later Changed batch to inspect versions published by an earlier batch's random write");
		}
		return {};
	}, &crossChunkState));
	Require(changedBatches == 2 && Matching(query, world, &crossChunkState).empty(),
		"Expected only the initially dirty chunk and the subsequently written chunk to run and acknowledge current entry versions");

	Check(first.Reset());
	(void)Matching(query, world, &first);
	const auto observed = first.ObservationCount();
	for (const auto id : entities) Check(world.SetEnabled(id, false));
	Require(Matching(query, world, &first).empty() && first.ObservationCount() == observed, "Expected chunks with no matching rows not to acknowledge new mask versions");
	Check(world.SetEnabled(entities.front(), true));
	Require(Matching(query, world, &first) == std::set<uint64_t>{1} && Matching(query, world, &first).empty(),
		"Expected a re-enabled row to remain observable after an empty masked execution");
	Check(world.SetEnabled(entities.back(), true));
	Require(Matching(query, world, &first) == std::set<uint64_t>{96}, "Expected re-enabling another chunk not to consume a different chunk's progress");
	Check(world.SetComponentEnabled(entities.front(), position, false));
	Require(Matching(query, world, &first).empty(), "Expected a disabled Required component to remove its row from observation");
	Check(world.SetComponentEnabled(entities.front(), position, true));
	Require(Matching(query, world, &first) == std::set<uint64_t>{1}, "Expected component re-enable to invalidate a previous mask observation");
	Check(world.SetTag(entities.front(), marker));
	QuerySpec tagged = spec; tagged.Required = {marker};
	auto taggedQuery = Take(Query::Create(context, tagged));
	Require(Matching(taggedQuery, world, &first) == std::set<uint64_t>{1}, "Expected an independent tagged observation");
	Check(world.SetComponentEnabled(entities.front(), marker, false));
	Require(Matching(taggedQuery, world, &first).empty(), "Expected disabled tags to behave as absent filters");
	Check(world.SetComponentEnabled(entities.front(), marker, true));
	Require(Matching(taggedQuery, world, &first) == std::set<uint64_t>{1}, "Expected tag re-enable to remain visible to Changed");
	Check(world.Clear());
	const auto replacement = Take(world.CreateEmpty("new generation", {3, 500}));
	Check(world.Emplace<Position>(replacement, Position{500}));
	Require(Matching(query, world, &first) == std::set<uint64_t>{500}, "Expected a new chunk after Clear not to reuse old Changed observations");
}

void VerifyQueryCacheWork() {
	EcsContext context;
	const auto position = Register<Position>(context, "Query.CachePosition");
	const auto velocity = Register<Velocity>(context, "Query.CacheVelocity");
	const auto marker = Register<Marker>(context, "Query.CacheMarker", true);
	World world(context);
	std::vector<EntityId> entities;
	for (uint64_t index = 1; index <= 300; ++index) {
		const auto id = Take(world.CreateEmpty("cache row", {4, index}));
		Check(world.Emplace<Position>(id, Position{static_cast<int>(index)}));
		entities.push_back(id);
	}
	QuerySpec spec; spec.Columns = {{position}};
	auto query = Take(Query::Create(context, spec));
	const auto cold = Matching(query, world);
	const auto afterCold = query.Stats();
	Require(afterCold.GroupMatchCount > 0 && afterCold.LayoutBindCount > 0 && cold.size() == 300, "Expected a cold query to perform observable matching and binding work");
	Require(Matching(query, world) == cold, "Expected warm query results to equal cold results");
	const auto warm = query.Stats();
	Require(warm.GroupMatchCount == afterCold.GroupMatchCount && warm.LayoutBindCount == afterCold.LayoutBindCount && warm.CacheHits > afterCold.CacheHits &&
		warm.VisitedRows - afterCold.VisitedRows == 300, "Expected hot execution to reuse type matching and binding while visiting current rows");
	const auto added = Take(world.CreateEmpty("same group", {4, 301}));
	Check(world.Emplace<Position>(added));
	const auto groupRevision = world.GroupRevision();
	Require(Matching(query, world).size() == 301 && query.Stats().GroupMatchCount == warm.GroupMatchCount && query.Stats().LayoutBindCount == warm.LayoutBindCount,
		"Expected entity count changes in existing groups not to repeat type matching or column binding");
	Check(world.SetTag(added, marker));
	Require(world.GroupRevision() > groupRevision, "Expected a new tag group to publish incremental group change");
	(void)Matching(query, world);
	const auto tagged = query.Stats();
	Require(tagged.GroupMatchCount > warm.GroupMatchCount && tagged.LayoutBindCount == warm.LayoutBindCount,
		"Expected a new Group with an existing layout to match once while reusing its column binding");
	Check(world.Emplace<Velocity>(added));
	(void)Matching(query, world);
	Require(query.Stats().LayoutBindCount > tagged.LayoutBindCount, "Expected a genuinely new layout to bind its column indices");
	std::mt19937 random(0x31a5ca7e);
	for (size_t step = 0; step < 160; ++step) {
		const auto id = entities[random() % entities.size()];
		if (!world.IsAlive(id)) continue;
		switch (random() % 5) {
		case 0: Check(world.SetEnabled(id, !world.IsEnabled(id))); break;
		case 1: Check(world.SetComponentEnabled(id, position, !world.IsComponentEnabled(id, position))); break;
		case 2: Check(world.SetTag(id, marker, !world.Has(id, marker))); break;
		case 3: Check(world.Emplace<Velocity>(id, Velocity{static_cast<int>(step)})); break;
		case 4: Check(world.Remove(id, velocity)); break;
		}
		auto fresh = Take(Query::Create(context, spec));
		Require(Matching(query, world) == Matching(fresh, world), "Expected cached and fresh queries to agree after fixed-seed mask and structural changes");
	}
	Check(world.Destroy(entities.front()));
	auto fresh = Take(Query::Create(context, spec));
	Require(Matching(query, world) == Matching(fresh, world), "Expected tail-fill after destruction not to reuse stale row bindings");
	World second(context);
	const auto other = Take(second.CreateEmpty("other world", {5, 900}));
	Check(second.Emplace<Position>(other, Position{900}));
	Require(Matching(query, second) == std::set<uint64_t>{900}, "Expected one query to isolate cache state by World identity");
	Check(world.Clear());
	Require(Matching(query, world).empty(), "Expected Clear to invalidate cached groups and chunks");
	const auto recreated = Take(world.CreateEmpty("recreated", {4, 901}));
	Check(world.Emplace<Position>(recreated));
	Require(Matching(query, world) == std::set<uint64_t>{901} && Matching(query, second) == std::set<uint64_t>{900},
		"Expected old and new World cache entries to remain independent after Clear");
}

void VerifyRandomResourcesAndShared() {
	EcsContext context;
	const auto position = Register<Position>(context, "Query.AccessPosition");
	const auto velocity = Register<Velocity>(context, "Query.AccessVelocity");
	const auto counterType = Register<Counter>(context, "Query.AccessCounter");
	World main(context), target(context), undeclared(context);
	const auto source = Take(main.CreateEmpty("source", {6, 1}));
	Check(main.Emplace<Position>(source, Position{1}));
	const auto destination = Take(target.CreateEmpty("target", {6, 2}));
	Check(target.Emplace<Position>(destination, Position{42}));
	const auto object = std::make_shared<Counter>(Counter{10});
	const auto equalObject = std::make_shared<Counter>(Counter{10});
	const auto handle = Take(context.Resources().Register(object));
	const auto differentHandle = Take(context.Resources().Register(equalObject));
	Require(handle != differentHandle, "Expected equal-valued resources to retain distinct identities");
	Check(main.SetShared(source, {counterType, handle}));
	const auto excluded = Take(main.CreateEmpty("equal shared value", {6, 3}));
	Check(main.Emplace<Position>(excluded, Position{3}));
	Check(main.SetShared(excluded, {counterType, differentHandle}));
	QuerySpec spec; spec.Columns = {{position}}; spec.SharedBindings = {{counterType, handle}};
	spec.RandomAccesses = {{&target, position, AccessMode::Read}};
	spec.ResourceAccesses = {{handle, AccessMode::Read}};
	auto reader = Take(Query::Create(context, spec));
	Require(Matching(reader, main) == std::set<uint64_t>{1}, "Expected shared filtering by object identity rather than equal values");
	RandomView<const Position> retainedRandom;
	QueryBatch retainedBatch;
	Check(reader.Batches(main, [&](QueryBatch& batch) -> Result<void> {
		const auto random = Take(batch.Random<const Position>(target));
		Require(Take(random.TryGet(destination))->Value == 42, "Expected declared random access to target the requested World");
		Require(!batch.Random<Position>(target) && !batch.Random<const Velocity>(target) && !batch.Random<const Position>(undeclared),
			"Expected random access to reject missing write permission, missing type and undeclared World");
		Require(Take(batch.Resource<const Counter>(handle)).get().Value == 10 && !batch.Resource<Counter>(handle) &&
			!batch.Resource<const Position>(handle) && !batch.Resource<const Counter>(differentHandle),
			"Expected resource access to validate permission, native type and declared object identity");
		Require(!target.Destroy(destination), "Expected execution to borrow every declared random target World");
		retainedRandom = random; retainedBatch = batch;
		return {};
	}));
	Require(!retainedRandom.TryGet(destination) && !retainedBatch.Resource<const Counter>(handle),
		"Expected random and resource access paths to reject expired execution tokens");
	EcsContext foreign;
	World foreignWorld(foreign);
	const auto foreignHandle = Take(foreign.Resources().Register(std::make_shared<Counter>()));
	QuerySpec badRandom; badRandom.RandomAccesses = {{&foreignWorld, position, AccessMode::Read}};
	QuerySpec badResource; badResource.ResourceAccesses = {{foreignHandle, AccessMode::Read}};
	QuerySpec badShared; badShared.SharedBindings = {{counterType, foreignHandle}};
	Require(!Query::Create(context, badRandom) && !Query::Create(context, badResource) && !Query::Create(context, badShared),
		"Expected foreign Context World and resource declarations to be rejected at query creation");
	QuerySpec observeSpec; observeSpec.Columns = {{position}}; observeSpec.ChangedTypes = {position};
	auto observer = Take(Query::Create(context, observeSpec));
	ChangedState state;
	(void)Matching(observer, target, &state);
	QuerySpec writeSpec = spec;
	writeSpec.RandomAccesses.front().Access = AccessMode::Write;
	writeSpec.ResourceAccesses.front().Access = AccessMode::Write;
	auto writer = Take(Query::Create(context, writeSpec));
	Check(writer.Each(main, [&](QueryRow& row) -> Result<void> {
		++Take(Take(row.Random<Position>(target)).TryGet(destination))->Value;
		++Take(row.Resource<Counter>(handle)).get().Value;
		return {};
	}));
	Require(static_cast<const World&>(target).TryGet<Position>(destination)->Value == 43 && object->Value == 11 &&
		Matching(observer, target, &state) == std::set<uint64_t>{2}, "Expected actual random writes to publish target column versions and resource writes to reach the shared object");
	Check(target.SetShared(destination, {counterType, handle}));
	QuerySpec resourceOnly; resourceOnly.Columns = {{position}}; resourceOnly.ResourceAccesses = {{handle, AccessMode::Read}};
	auto crossWorld = Take(Query::Create(context, resourceOnly));
	Check(crossWorld.Each(target, [&](QueryRow& row) -> Result<void> {
		Require(&Take(row.Resource<const Counter>(handle)).get() == object.get(), "Expected the same resource handle to refer to one object across Worlds");
		return {};
	}));
	Require(writer.Spec().RandomAccesses.front().Target == &target && writer.Spec().RandomAccesses.front().Type == position &&
		writer.Spec().RandomAccesses.front().TargetId == target.Id() &&
		writer.Spec().ResourceAccesses.front().Resource.Id == crossWorld.Spec().ResourceAccesses.front().Resource.Id,
		"Expected access metadata to retain target World, whole component type and shared ResourceId");
	QuerySpec sharedOnly; sharedOnly.Columns = {{position}}; sharedOnly.SharedBindings = {{counterType, handle}};
	auto noResourcePermission = Take(Query::Create(context, sharedOnly));
	Check(noResourcePermission.Each(main, [&](QueryRow& row) -> Result<void> {
		Require(!row.Resource<const Counter>(handle), "Expected shared filtering not to grant undeclared resource content access");
		return {};
	}));
	auto ephemeral = std::make_unique<World>(context);
	QuerySpec staleSpec; staleSpec.Columns = {{position}}; staleSpec.RandomAccesses = {{ephemeral.get(), position, AccessMode::Read}};
	auto staleQuery = Take(Query::Create(context, staleSpec));
	ephemeral.reset();
	const auto staleExecution = staleQuery.Each(main, [](QueryRow&) -> Result<void> { return {}; });
	Require(!staleExecution && staleExecution.GetError().Code == ErrorCode::InvalidState,
		"Expected a destroyed declared random target to fail without dereferencing a dangling World pointer");
}

void VerifyMaskCommands() {
	EcsContext context;
	const auto position = Register<Position>(context, "Query.CommandPosition");
	const auto marker = Register<Marker>(context, "Query.CommandMarker", true);
	World world(context);
	const auto id = Take(world.CreateEmpty("command mask", {7, 1}));
	Check(world.Emplace<Position>(id)); Check(world.SetTag(id, marker));
	QuerySpec spec; spec.Columns = {{position}}; spec.Required = {marker};
	auto query = Take(Query::Create(context, spec));
	CommandBuffer disabled(context);
	Check(disabled.SetEnabled(id, false));
	Require(Matching(query, world) == std::set<uint64_t>{1}, "Expected unplayed enable commands not to change query visibility");
	Check(disabled.Playback(world));
	Require(Matching(query, world).empty(), "Expected committed entity disable to remove the row");
	CommandBuffer component(context);
	Check(component.SetEnabled(id, true));
	Check(component.SetComponentEnabled(id, marker, false));
	Check(component.Playback(world));
	Require(Matching(query, world).empty(), "Expected a disabled required tag after buffered playback");
	CommandBuffer enabled(context);
	Check(enabled.SetComponentEnabled(id, marker, true));
	Check(enabled.Playback(world));
	Require(Matching(query, world) == std::set<uint64_t>{1}, "Expected re-enabled tag visibility after commit");
}

void VerifyHistoryRollover() {
	EcsContext context;
	const auto position = Register<Position>(context, "Query.HistoryPosition");
	const auto velocity = Register<Velocity>(context, "Query.HistoryVelocity");
	World world(context, {.ChunkBytes = 256});
	const auto original = Take(world.CreateEmpty("history original", {8, 1}));
	Check(world.Emplace<Position>(original));
	QuerySpec spec; spec.Columns = {{position}, {velocity, Presence::Optional, AccessMode::Read}};
	auto cached = Take(Query::Create(context, spec));
	Require(Matching(cached, world) == std::set<uint64_t>{1}, "Expected an initial cache entry before history rollover");
	const auto oldRevision = world.GroupRevision();
	uint64_t lastUuid = 0;
	for (uint64_t index = 0; index < 4096; ++index) {
		if ((index % 64) == 0 && world.ChangesSince(oldRevision).Reset) break;
		Check(world.Clear());
		lastUuid = index + 2;
		const auto id = Take(world.CreateEmpty("history replacement", {8, lastUuid}));
		if ((index & 1) != 0) Check(world.Emplace<Velocity>(id, Velocity{17}));
		Check(world.Emplace<Position>(id, Position{static_cast<int>(index)}));
	}
	Require(world.ChangesSince(oldRevision).Reset, "Expected bounded Group history to signal a full snapshot after rollover");
	auto fresh = Take(Query::Create(context, spec));
	Require(Matching(cached, world) == Matching(fresh, world) && Matching(cached, world) == std::set<uint64_t>{lastUuid},
		"Expected a stale cache to rebuild after history truncation without reusing another generation's column binding");
	Check(cached.Each(world, [](QueryRow& row) -> Result<void> {
		(void)Take(row.Get<const Position>(0));
		if (const auto* value = Take(row.Optional<const Velocity>(1))) Require(value->Value == 17, "Expected optional columns to bind the current layout after rollover");
		return {};
	}));
}

void VerifyWorldDestructionDuringExecution() {
	using Payload = ECSLifecycleFixtures::NoDefault;
	Require(Payload::Live == 0, "Expected no pre-existing lifecycle payloads");
	EcsContext context;
	const auto payload = Register<Payload>(context, "Query.DestructionPayload");
	auto world = std::make_unique<World>(context, WorldOptions{.ChunkBytes = 256});
	for (uint64_t index = 1; index <= 64; ++index) {
		const auto id = Take(world->CreateEmpty("borrowed payload", {9, index}));
		Check(world->Emplace<Payload>(id, static_cast<int>(index)));
	}
	Require(world->Chunks().size() > 1 && Payload::Live == 64, "Expected multiple chunks with owned lifecycle payloads");
	QuerySpec spec; spec.Columns = {{payload, Presence::Required, AccessMode::Write}}; spec.ChangedTypes = {payload};
	auto query = Take(Query::Create(context, spec));
	ChangedState state;
	size_t calls = 0;
	std::optional<ColumnView<Payload>> retained;
	const auto destroyed = query.Batches(*world, [&](QueryBatch& batch) -> Result<void> {
		++calls;
		retained = Take(batch.Column<Payload>(0));
		auto& object = retained->At(0);
		const int originalValue = object.Value;
		world.reset();
		Require(Payload::Live == 64 && object.Value == originalValue, "Expected an active borrow to preserve native object storage after World facade destruction");
		Require(!batch.Valid() && !retained->Valid() && !retained->TryGet(0), "Expected World destruction to invalidate all public batch access immediately");
		return {};
	}, &state);
	Require(!destroyed && destroyed.GetError().Code == ErrorCode::InvalidState && calls == 1 && state.ObservationCount() == 0,
		"Expected callback World destruction to stop subsequent chunks and leave Changed unacknowledged");
	Require(Payload::Live == 0 && retained && !retained->Valid(), "Expected borrowed native objects to be destroyed when execution releases storage");

	auto target = std::make_unique<World>(context);
	const auto destination = Take(target->CreateEmpty("random target", {9, 200}));
	Check(target->Emplace<Payload>(destination, 200));
	World main(context);
	const auto source = Take(main.CreateEmpty("source", {9, 100}));
	Check(main.Emplace<Payload>(source, 100));
	QuerySpec observationSpec; observationSpec.Columns = {{payload}}; observationSpec.ChangedTypes = {payload};
	auto observer = Take(Query::Create(context, observationSpec));
	ChangedState survivingState;
	(void)Matching(observer, main, &survivingState);
	QuerySpec randomSpec; randomSpec.Columns = {{payload}};
	randomSpec.RandomAccesses = {{target.get(), payload, AccessMode::Write}, {&main, payload, AccessMode::Write}};
	auto randomQuery = Take(Query::Create(context, randomSpec));
	RandomView<Payload> randomView;
	const auto randomDestroyed = randomQuery.Each(main, [&](QueryRow& row) -> Result<void> {
		randomView = Take(row.Random<Payload>(*target));
		auto* object = Take(randomView.TryGet(destination));
		++Take(Take(row.Random<Payload>(main)).TryGet(source))->Value;
		target.reset();
		Require(Payload::Live == 2 && object->Value == 200, "Expected a random target borrow to retain native object storage through the callback");
		Require(!randomView.TryGet(destination) && !row.Valid() && !row.Get<const Payload>(0),
			"Expected any destroyed borrowed World to invalidate every access path immediately");
		return {};
	});
	Require(!randomDestroyed && randomDestroyed.GetError().Code == ErrorCode::InvalidState && Payload::Live == 1,
		"Expected random target destruction to fail the execution and release only the target storage");
	Require(Matching(observer, main, &survivingState) == std::set<uint64_t>{100} && static_cast<const World&>(main).TryGet<Payload>(source)->Value == 101,
		"Expected a destroyed earlier World not to prevent publishing completed writes in another surviving World");
	Check(main.Clear());
	Require(Payload::Live == 0, "Expected no leaked native objects after destruction callbacks");
}

void VerifyWorldMasksAndCache() {
	EcsContext context;
	const auto position = Register<Position>(context, "Query.Position");
	const auto velocity = Register<Velocity>(context, "Query.Velocity");
	const auto marker = Register<Marker>(context, "Query.Marker", true);
	World world(context);
	std::vector<EntityId> entities;
	struct Masks { bool Entity; bool Position; bool Marker; };
	std::map<std::pair<uint32_t, uint32_t>, Masks> expected;
	auto key = [](EntityId id) { return std::pair{id.Index, id.Generation}; };
	for (size_t index = 0; index < 300; ++index) {
		const auto id = Take(world.CreateEmpty("mask row", {0x3a5c, index + 1}));
		Check(world.Emplace<Position>(id, Position{static_cast<int>(index)}));
		Check(world.SetTag(id, marker));
		Require(world.IsEnabled(id) && world.IsComponentEnabled(id, position) && world.IsComponentEnabled(id, marker),
			"Expected new entities, ordinary components and tags to be enabled");
		entities.push_back(id);
		expected.emplace(key(id), Masks{index % 3 != 0, index % 5 != 0, index % 7 != 0});
	}
	const auto revision = world.GroupRevision();
	for (const auto id : entities) {
		const auto masks = expected.at(key(id));
		Check(world.SetEnabled(id, masks.Entity));
		Check(world.SetComponentEnabled(id, position, masks.Position));
		Check(world.SetComponentEnabled(id, marker, masks.Marker));
	}
	Require(world.GroupRevision() == revision, "Expected mask writes not to create or rematch Group identities");
	auto verify = [&] {
		for (const auto& chunk : world.Chunks()) {
			for (size_t row = 0; row < chunk.Entities.size(); ++row) {
				const auto id = chunk.Entities[row];
				const auto masks = expected.at(key(id));
				Require(chunk.Enabled(row) == masks.Entity && world.IsEnabled(id) == masks.Entity,
					"Expected entity masks to follow logical entities across tail-fill and migration");
				for (const auto& component : chunk.ComponentEnabled) {
					if (component.Type == position) Require(component.Enabled(row) == masks.Position, "Expected physical ordinary component mask mapping");
					if (component.Type == marker) Require(component.Enabled(row) == masks.Marker, "Expected physical tag mask mapping");
				}
				Require(world.IsComponentEnabled(id, position) == masks.Position && world.IsComponentEnabled(id, marker) == masks.Marker,
					"Expected logical component masks to preserve their owners");
				Require(Take(world.InspectChunk(chunk.Handle())).Entities[row] == id &&
					Take(world.InspectGroup(chunk.GroupIdentity)).Layout == chunk.Layout,
					"Expected public chunk and group handles to resolve the current generation");
			}
		}
	};
	verify();
	for (size_t index : {0u, 63u, 64u, 127u, 128u}) {
		Check(world.Destroy(entities[index]));
		expected.erase(key(entities[index]));
	}
	for (size_t index = 1; index < entities.size(); index += 17) {
		if (!world.IsAlive(entities[index])) continue;
		Check(world.Emplace<Velocity>(entities[index], Velocity{static_cast<int>(index)}));
		Require(world.IsComponentEnabled(entities[index], velocity), "Expected newly added ordinary components to start enabled");
	}
	verify();
	const auto disabled = entities[105];
	const auto cloned = Take(world.Clone(disabled, "disabled clone"));
	expected.emplace(key(cloned), expected.at(key(disabled)));
	verify();
	Require(!world.IsEnabled(cloned) && !world.IsComponentEnabled(cloned, position) && !world.IsComponentEnabled(cloned, marker),
		"Expected clone to copy entity, ordinary component and tag masks");
	const auto selected = entities[2];
	Check(world.Emplace<Velocity>(selected, Velocity{1}));
	Check(world.Remove(selected, velocity));
	const auto warm = world.Stats();
	for (int iteration = 0; iteration < 12; ++iteration) {
		Check(world.Emplace<Velocity>(selected, Velocity{iteration}));
		Check(world.Remove(selected, velocity));
	}
	const auto repeated = world.Stats();
	Require(repeated.MigrationPlanBuilds == warm.MigrationPlanBuilds && repeated.MigrationCacheHits >= warm.MigrationCacheHits + 24,
		"Expected repeated layout transitions to reuse cached migration plans");
	{
		auto borrow = Take(world.AcquireBorrow());
		Require(borrow.Valid(), "Expected a live World borrow");
		const auto create = world.CreateEmpty("forbidden structural write");
		const auto remove = world.Remove(selected, position);
		const auto entityMask = world.SetEnabled(selected, false);
		const auto componentMask = world.SetComponentEnabled(selected, marker, false);
		Require(!create && !remove && !entityMask && !componentMask && create.GetError().Code == ErrorCode::Busy && remove.GetError().Code == ErrorCode::Busy &&
			entityMask.GetError().Code == ErrorCode::Busy && componentMask.GetError().Code == ErrorCode::Busy,
			"Expected a live execution borrow to reject structural mutations");
	}
	Check(world.Remove(selected, velocity));
	const auto chunks = world.Chunks();
	Require(!chunks.empty(), "Expected identified chunks after the mask workload");
	Require(!world.InspectChunk({chunks.front().Id, chunks.front().Generation + 1}) &&
		!world.InspectGroup({chunks.front().GroupIdentity.Id, chunks.front().GroupIdentity.Generation + 1}),
		"Expected stale chunk and group generations to be rejected");
	const auto changes = world.ChangesSince(0);
	Require(changes.Revision == world.GroupRevision() && !changes.Added.empty(), "Expected a complete initial Group change feed");
	Require(world.ChangesSince(changes.Revision).Added.empty(), "Expected no duplicate Group additions at a current revision");
	const auto oldChunk = chunks.front().Handle();
	const auto oldGroup = chunks.front().GroupIdentity;
	Check(world.Clear());
	Require(!world.InspectChunk(oldChunk) && !world.InspectGroup(oldGroup) && !world.ChangesSince(changes.Revision).Removed.empty(),
		"Expected Clear to retire Group and Chunk handles and publish removals to incremental caches");
	const auto afterClear = Take(world.CreateEmpty("enabled after Clear"));
	Check(world.Emplace<Position>(afterClear));
	Check(world.SetTag(afterClear, marker));
	Require(world.IsEnabled(afterClear) && world.IsComponentEnabled(afterClear, position) && world.IsComponentEnabled(afterClear, marker),
		"Expected reused storage after Clear not to retain earlier mask bits");
}
}

int main() {
	VerifyWorldMasksAndCache();
	VerifyFilteringAndDeclarations();
	VerifyColumnsAndExpiredViews();
	VerifyChangedObservers();
	VerifyQueryCacheWork();
	VerifyRandomResourcesAndShared();
	VerifyMaskCommands();
	VerifyHistoryRollover();
	VerifyWorldDestructionDuringExecution();
#if defined(__SANITIZE_ADDRESS__)
	std::cout << "AddressSanitizer instrumentation is enabled\n";
#endif
	std::cout << "ECSQueryRuntimeSmoke passed\n";
	return 0;
}
