#include "ECSTestSupport.h"
#include "HuaEngine/ECS/Runtime/Job.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace ECSTestSupport;
using namespace HE::Ecs;

struct Number { int Value = 0; };
struct Offset { int Value = 0; };
struct Bonus { int Value = 0; };
struct Link { HE::EntityId Target; };
struct SharedCounter { int Value = 0; };
struct SharedMarker { int Value = 0; };
struct Hidden {};
struct Active {};
struct Unregistered { int Value = 0; };
struct Snapshot { HE::EntityId Entity; int Value = 0; };

struct AddOffset final {
    ComponentView<Number> numbers;
    ComponentView<const Offset> offsets;
    void build(AccessBuilder& access) const { access.write(&AddOffset::numbers).read(&AddOffset::offsets); }
    [[nodiscard]] Result<void> run(TaskContext& context) const {
        for (size_t row = 0; row < context.size(); ++row)
            numbers[row].Value += offsets[row].Value;
        return {};
    }
};

struct CollectNumbers final {
    using Output = Snapshot;
    ComponentView<const Number> numbers;
    void build(AccessBuilder& access) const { access.read(&CollectNumbers::numbers); }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row)
            output.Emit({context.entity(row), numbers[row].Value});
        return {};
    }
};

struct InvalidJob final {
    ComponentView<const Unregistered> missing;
    void build(AccessBuilder& access) const { access.read(&InvalidJob::missing); }
    [[nodiscard]] Result<void> run(TaskContext&) const { return {}; }
};

struct ForeignMemberJob final {
    void build(AccessBuilder& access) const { access.read(&CollectNumbers::numbers); }
    [[nodiscard]] Result<void> run(TaskContext&) const { return {}; }
};

struct ReadWriteSame final {
    ComponentView<const Number> before;
    ComponentView<Number> after;
    void build(AccessBuilder& access) const {
        access.read(&ReadWriteSame::before).write(&ReadWriteSame::after);
    }
    [[nodiscard]] Result<void> run(TaskContext& context) const {
        for (size_t row = 0; row < context.size(); ++row)
            after[row].Value = before[row].Value + 1;
        return {};
    }
};

struct CollectOptional final {
    using Output = Snapshot;
    ComponentView<const Number> numbers;
    ComponentView<const Bonus> bonuses;
    void build(AccessBuilder& access) const {
        access.read(&CollectOptional::numbers)
            .optional_read(&CollectOptional::bonuses)
            .none<Hidden>();
    }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row) {
            const auto* bonus = bonuses.TryGet(row);
            output.Emit({context.entity(row), numbers[row].Value + (bonus ? bonus->Value : 0)});
        }
        return {};
    }
};

struct FollowLinks final {
    using Output = Snapshot;
    ComponentView<const Link> links;
    RandomView<const Number> targets;
    World* Target = nullptr;
    void build(AccessBuilder& access) const {
        access.read(&FollowLinks::links).access(&FollowLinks::targets, *Target);
    }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row) {
            auto target = targets.TryGet(links[row].Target);
            if (!target) return target.GetError();
            output.Emit({context.entity(row), target.Value() ? target.Value()->Value : -1});
        }
        return {};
    }
};

struct IncrementTargets final {
    ComponentView<const Link> links;
    RandomView<Number> targets;
    World* Target = nullptr;
    void build(AccessBuilder& access) const {
        access.read(&IncrementTargets::links).access(&IncrementTargets::targets, *Target);
    }
    [[nodiscard]] Result<void> run(TaskContext& context) const {
        for (size_t row = 0; row < context.size(); ++row) {
            auto target = targets.TryGet(links[row].Target);
            if (!target) return target.GetError();
            if (target.Value()) target.Value()->Value += 2;
        }
        return {};
    }
};

struct ReadOwnWorld final {
    using Output = Snapshot;
    ComponentView<const Number> numbers;
    RandomView<const Number> random;
    void build(AccessBuilder& access) const {
        access.read(&ReadOwnWorld::numbers).access(&ReadOwnWorld::random);
    }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row) {
            auto value = random.TryGet(context.entity(row));
            if (!value) return value.GetError();
            output.Emit({context.entity(row), value.Value() ? value.Value()->Value : -1});
        }
        return {};
    }
};

struct WriteResource final {
    ComponentView<const Link> links;
    ResourceView<SharedCounter> counter;
    ResourceHandle Handle;
    void build(AccessBuilder& access) const {
        access.read(&WriteResource::links).access(&WriteResource::counter, Handle);
    }
    [[nodiscard]] Result<void> run(TaskContext& context) const {
        counter->Value += static_cast<int>(context.size());
        return {};
    }
};

struct ReadResource final {
    using Output = Snapshot;
    ComponentView<const Link> links;
    ResourceView<const SharedCounter> counter;
    ResourceHandle Handle;
    void build(AccessBuilder& access) const {
        access.read(&ReadResource::links).access(&ReadResource::counter, Handle);
    }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row)
            output.Emit({context.entity(row), counter->Value});
        return {};
    }
};

struct ObserveNumbers final {
    using Output = Snapshot;
    ComponentView<const Number> numbers;
    ChangedState* Observer = nullptr;
    void build(AccessBuilder& access) const {
        access.read(&ObserveNumbers::numbers).changed<Number>(*Observer);
    }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row)
            output.Emit({context.entity(row), numbers[row].Value});
        return {};
    }
};

struct ConflictingObservers final {
    ChangedState* First = nullptr;
    ChangedState* Second = nullptr;
    void build(AccessBuilder& access) const {
        access.changed<Number>(*First).changed<Bonus>(*Second);
    }
    [[nodiscard]] Result<void> run(TaskContext&) const { return {}; }
};

struct SelectShared final {
    using Output = Snapshot;
    ComponentView<const Number> numbers;
    ResourceHandle Shared;
    void build(AccessBuilder& access) const {
        access.read(&SelectShared::numbers).has<Active>().none<Hidden>().shared<SharedMarker>(Shared);
    }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row) {
            output.Emit({context.entity(row), numbers[row].Value});
            auto created = context.commands().CreateEmpty("job command");
            if (!created) return created.GetError();
        }
        return {};
    }
};

struct IncludeMasked final {
    ComponentView<Number> numbers;
    ResourceHandle Shared;
    void build(AccessBuilder& access) const {
        access.write(&IncludeMasked::numbers).has<Active>().none<Hidden>()
            .shared<SharedMarker>(Shared).include_disabled().ignore_component_enabled();
    }
    [[nodiscard]] Result<void> run(TaskContext& context) const {
        for (size_t row = 0; row < context.size(); ++row) numbers[row].Value += 100;
        return {};
    }
};

struct OwnedSnapshot final {
    HE::EntityId Entity;
    std::unique_ptr<int> Value;
};

struct CollectOwned final {
    using Output = OwnedSnapshot;
    ComponentView<const Number> numbers;
    void build(AccessBuilder& access) const { access.read(&CollectOwned::numbers); }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        for (size_t row = 0; row < context.size(); ++row)
            output.Emit({context.entity(row), std::make_unique<int>(numbers[row].Value)});
        return {};
    }
};

struct FailOwned final {
    using Output = OwnedSnapshot;
    ComponentView<Number> numbers;
    std::shared_ptr<std::atomic<bool>> Release;
    void build(AccessBuilder& access) const { access.write(&FailOwned::numbers); }
    [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
        output.Emit({context.entity(0), std::make_unique<int>(numbers[0].Value)});
        while (!Release->load(std::memory_order_acquire)) std::this_thread::yield();
        throw std::runtime_error("intentional Job output failure");
    }
};

struct DestroyFromJob final {
    ComponentView<Number> numbers;
    std::unique_ptr<World>* Target = nullptr;
    std::shared_ptr<std::atomic<int>> Calls;
    void build(AccessBuilder& access) const { access.write(&DestroyFromJob::numbers); }
    [[nodiscard]] Result<void> run(TaskContext& context) const {
        if (!context.valid())
            return Error{ErrorCode::InvalidState, "DestroyFromJob", "The World is unavailable"};
        numbers[0].Value += 1;
        Calls->fetch_add(1, std::memory_order_relaxed);
        Target->reset();
        return {};
    }
};

void RunWorldDestruction(TimelineMode mode) {
    EcsContext context({.WorkerCount = 2});
    Take(context.Types().Register<Number>("Job.DestroyNumber"));
    auto world = std::make_unique<World>(context);
    const auto entity = Take(world->CreateEmpty());
    Add<Number>(*world, entity, Number{1});
    Timeline timeline(context, {.Mode = mode});
    DestroyFromJob job;
    job.Target = &world;
    job.Calls = std::make_shared<std::atomic<int>>(0);
    auto task = Take(timeline.Dispatch(*world, job));
    const auto finished = timeline.Finish();
    Require(!finished && finished.GetError().Code == ErrorCode::InvalidState,
        "Destroying a World in a Job must fail the task without accessing invalid rows");
    Require(!world && job.Calls->load(std::memory_order_relaxed) == 1 && task.Status() == TaskStatus::Failed,
        "A Job must observe World destruction after its current batch returns");
    Check(timeline.ResetError());
}

void RunOutputOwnership(TimelineMode mode) {
    EcsContext context({.WorkerCount = 2});
    Take(context.Types().Register<Number>("Job.OutputNumber"));
    World world(context);
    const auto entity = Take(world.CreateEmpty());
    Add<Number>(world, entity, Number{27});
    Timeline timeline(context, {.Mode = mode});
    auto original = Take(timeline.Dispatch(world, CollectOwned{}));
    auto moved = std::move(original);
    Require(!original.Collect(timeline), "A moved-from Job output cannot be collected");
    auto values = Take(moved.Collect(timeline));
    Require(values.size() == 1 && values.front().Entity == entity && *values.front().Value == 27,
        "A Job must retain move-only output until collection");
    Require(!moved.Collect(timeline), "Job output collection must consume results once");

    auto release = std::make_shared<std::atomic<bool>>(false);
    FailOwned failing;
    failing.Release = release;
    auto failed = Take(timeline.Dispatch(world, failing));
    auto cancelled = Take(timeline.Dispatch(world, CollectOwned{}));
    release->store(true, std::memory_order_release);
    Require(!timeline.Finish(), "A failing Job must fail its Timeline");
    Require(!failed.Collect(timeline) && !cancelled.Collect(timeline),
        "Failed and dependent Jobs cannot return partial output");
    Require(failed.Task().Status() == TaskStatus::Failed && cancelled.Task().Status() == TaskStatus::Cancelled,
        "Job output tasks must expose failure and dependency cancellation");
    Check(timeline.ResetError());
}

void RunSharedAndMasks(TimelineMode mode) {
    EcsContext context({.WorkerCount = 2});
    Take(context.Types().Register<Number>("Job.SharedNumber"));
    const auto activeType = Take(context.Types().Register<Active>(TypeGuid::FromName("Job.Active"), "Job.Active", true));
    const auto hiddenType = Take(context.Types().Register<Hidden>(TypeGuid::FromName("Job.SharedHidden"), "Job.SharedHidden", true));
    const auto sharedType = Take(context.Types().Register<SharedMarker>("Job.SharedMarker"));
    const auto shared = Take(context.Resources().Register(std::make_shared<SharedMarker>(SharedMarker{1})));
    const auto otherShared = Take(context.Resources().Register(std::make_shared<SharedMarker>(SharedMarker{1})));
    World world(context);
    const auto normal = Take(world.CreateEmpty());
    const auto disabled = Take(world.CreateEmpty());
    const auto componentDisabled = Take(world.CreateEmpty());
    const auto excluded = Take(world.CreateEmpty());
    const auto other = Take(world.CreateEmpty());
    for (const auto entity : {normal, disabled, componentDisabled, excluded, other}) {
        Add<Number>(world, entity, Number{1});
        Check(world.SetTag(entity, activeType));
        Check(world.SetShared(entity, {sharedType, entity == other ? otherShared : shared}));
    }
    Check(world.SetTag(excluded, hiddenType));
    Check(world.SetEnabled(disabled, false));
    Check(world.SetComponentEnabled(componentDisabled, context.Types().Find<Number>()->Id, false));

    Timeline timeline(context, {.Mode = mode});
    SelectShared select;
    select.Shared = shared;
    auto selected = Take(timeline.Dispatch(world, select));
    const auto values = Take(selected.Collect(timeline));
    Require(values.size() == 1 && values.front().Entity == normal,
        "A Job must match shared identity, tags, exclusions and enabled masks");
    Check(timeline.CommitCommands());
    Require(world.EntityCount() == 6, "A Job command must become visible after commit");

    IncludeMasked include;
    include.Shared = shared;
    Take(timeline.Dispatch(world, include));
    Check(timeline.Finish());
    Require(world.TryGet<Number>(normal)->Value == 101 && world.TryGet<Number>(disabled)->Value == 101 &&
        world.TryGet<Number>(componentDisabled)->Value == 101 && world.TryGet<Number>(excluded)->Value == 1 &&
        world.TryGet<Number>(other)->Value == 1,
        "Job mask overrides must affect only matching shared and tag groups");

    EcsContext foreignContext({.WorkerCount = 1});
    const auto foreign = Take(foreignContext.Resources().Register(std::make_shared<SharedMarker>()));
    select.Shared = foreign;
    Require(!timeline.Dispatch(world, select), "A Job must reject a shared object from another Context");
}

void RunDeclaredAccesses(TimelineMode mode) {
    EcsContext context({.WorkerCount = 3});
    Take(context.Types().Register<Number>("Job.AccessNumber"));
    Take(context.Types().Register<Link>("Job.AccessLink"));
    Take(context.Types().Register<Bonus>("Job.AccessBonus"));
    World source(context);
    World target(context);
    const auto firstTarget = Take(target.CreateEmpty());
    const auto secondTarget = Take(target.CreateEmpty());
    Add<Number>(target, firstTarget, Number{10});
    Add<Number>(target, secondTarget, Number{20});
    const auto first = Take(source.CreateEmpty());
    const auto second = Take(source.CreateEmpty());
    Add<Link>(source, first, Link{firstTarget});
    Add<Link>(source, second, Link{secondTarget});
    auto counter = std::make_shared<SharedCounter>();
    const auto resource = Take(context.Resources().Register(counter));
    Timeline timeline(context, {.Mode = mode});

    IncrementTargets increment;
    increment.Target = &target;
    Take(timeline.Dispatch(source, increment));
    FollowLinks follow;
    follow.Target = &target;
    auto random = Take(timeline.Dispatch(source, follow));
    auto randomValues = Take(random.Collect(timeline));
    Require(randomValues.size() == 2, "Random Job access must visit both source entities");
    for (const auto& value : randomValues)
        Require((value.Entity == first && value.Value == 12) ||
            (value.Entity == second && value.Value == 22),
            "A random reader must observe preceding cross-World writes");

    WriteResource write;
    write.Handle = resource;
    Take(timeline.Dispatch(source, write));
    ReadResource read;
    read.Handle = resource;
    auto resourceTask = Take(timeline.Dispatch(source, read));
    auto resourceValues = Take(resourceTask.Collect(timeline));
    Require(counter->Value == 2 && resourceValues.size() == 2,
        "Resource reads must observe the preceding declared write");
    for (const auto& value : resourceValues)
        Require(value.Value == 2, "Each resource view must bind the registered object");

    auto sameWorld = Take(timeline.Dispatch(target, ReadOwnWorld{}));
    auto sameWorldValues = Take(sameWorld.Collect(timeline));
    Require(sameWorldValues.size() == 2, "Default random access must target the submitted World");
    for (const auto& value : sameWorldValues)
        Require((value.Entity == firstTarget && value.Value == 12) ||
            (value.Entity == secondTarget && value.Value == 22),
            "A default random reader must bind the submitted World");

    ChangedState observer;
    ObserveNumbers observed;
    observed.Observer = &observer;
    auto firstChange = Take(timeline.Dispatch(target, observed));
    Require(Take(firstChange.Collect(timeline)).size() == 2,
        "A Changed observer must initially visit each matching row");
    auto unchanged = Take(timeline.Dispatch(target, observed));
    Require(Take(unchanged.Collect(timeline)).empty(),
        "A Changed observer must skip unchanged columns");
    Take(timeline.Dispatch(source, increment));
    auto changed = Take(timeline.Dispatch(target, observed));
    auto changedValues = Take(changed.Collect(timeline));
    Require(changedValues.size() == 2,
        "A Changed observer must see completed random writes in another World");

    ChangedState otherObserver;
    ConflictingObservers invalid;
    invalid.First = &observer;
    invalid.Second = &otherObserver;
    auto conflict = timeline.Dispatch(target, invalid);
    Require(!conflict && conflict.GetError().Code == ErrorCode::InvalidArgument,
        "A Job must reject multiple Changed observers");
    EcsContext foreignContext({.WorkerCount = 1});
    World foreignWorld(foreignContext);
    follow.Target = &foreignWorld;
    auto foreignRandom = timeline.Dispatch(source, follow);
    Require(!foreignRandom && foreignRandom.GetError().Code == ErrorCode::InvalidArgument,
        "Random access to a foreign Context must fail before submission");
    auto foreignObject = std::make_shared<SharedCounter>();
    read.Handle = Take(foreignContext.Resources().Register(foreignObject));
    auto foreignResource = timeline.Dispatch(source, read);
    Require(!foreignResource && foreignResource.GetError().Code == ErrorCode::InvalidType,
        "Resource access to a foreign Context must fail before submission");
    Check(timeline.Finish());
}

void RunOptional(TimelineMode mode) {
    EcsContext context({.WorkerCount = 2});
    Take(context.Types().Register<Number>("Job.OptionalNumber"));
    Take(context.Types().Register<Bonus>("Job.OptionalBonus"));
    const auto hiddenType = Take(context.Types().Register<Hidden>(
        TypeGuid::FromName("Job.Hidden"), "Job.Hidden", true));
    World world(context);
    const auto first = Take(world.CreateEmpty());
    const auto second = Take(world.CreateEmpty());
    const auto excluded = Take(world.CreateEmpty());
    Add<Number>(world, first, Number{1});
    Add<Number>(world, second, Number{2});
    Add<Number>(world, excluded, Number{3});
    Add<Bonus>(world, first, Bonus{10});
    Check(world.SetTag(excluded, hiddenType));
    Timeline timeline(context, {.Mode = mode});
    auto task = Take(timeline.Dispatch(world, CollectOptional{}));
    auto output = Take(task.Collect(timeline));
    Require(output.size() == 2, "An excluded Job row must not appear");
    bool sawFirst = false;
    bool sawSecond = false;
    for (const auto& item : output) {
        if (item.Entity == first) sawFirst = item.Value == 11;
        else if (item.Entity == second) sawSecond = item.Value == 2;
        else Require(false, "An unexpected entity passed the Job filter");
    }
    Require(sawFirst && sawSecond, "Optional Job views must bind both present and absent components");
    Check(timeline.Finish());
}

std::vector<Snapshot> Run(TimelineMode mode) {
    EcsContext context({.WorkerCount = 3});
    Take(context.Types().Register<Number>("Job.Number"));
    Take(context.Types().Register<Offset>("Job.Offset"));
    World world(context);
    std::vector<HE::EntityId> entities;
    for (int index = 0; index < 730; ++index) {
        auto entity = Take(world.CreateEmpty());
        Add<Number>(world, entity, Number{index});
        Add<Offset>(world, entity, Offset{5});
        entities.push_back(entity);
    }
    Timeline timeline(context, {.Mode = mode});
    auto invalid = timeline.Dispatch(world, InvalidJob{});
    Require(!invalid && invalid.GetError().Code == ErrorCode::InvalidType,
        "An unregistered Job component must fail before submission");
    auto foreign = timeline.Dispatch(world, ForeignMemberJob{});
    Require(!foreign && foreign.GetError().Code == ErrorCode::InvalidArgument,
        "A Job must not bind a view from another Job type");
    auto writer = Take(timeline.Dispatch(world, AddOffset{}));
    auto reader = Take(timeline.Dispatch(world, CollectNumbers{}));
    auto values = Take(reader.Collect(timeline));
    Require(writer.Status() == TaskStatus::Succeeded && reader.Task().Status() == TaskStatus::Succeeded,
        "A Job reader must observe the preceding Job writer");
    Require(!reader.Collect(timeline), "Job output can only be collected once");
    Require(values.size() == entities.size(), "Every matching row must produce one output");
    for (const auto& value : values)
        Require(value.Value == static_cast<int>(value.Entity.Index) + 5,
            "The output must contain the completed write");
    auto combined = Take(timeline.Dispatch(world, ReadWriteSame{}));
    Check(timeline.Wait(combined));
    Require(world.TryGet<Number>(entities.front())->Value == 6,
        "Read and write views of one component must bind independently");
    Check(timeline.Finish());
    return values;
}
}

int main() {
    RunWorldDestruction(TimelineMode::Serial);
    RunWorldDestruction(TimelineMode::Parallel);
    RunOutputOwnership(TimelineMode::Serial);
    RunOutputOwnership(TimelineMode::Parallel);
    RunSharedAndMasks(TimelineMode::Serial);
    RunSharedAndMasks(TimelineMode::Parallel);
    RunDeclaredAccesses(TimelineMode::Serial);
    RunDeclaredAccesses(TimelineMode::Parallel);
    RunOptional(TimelineMode::Serial);
    RunOptional(TimelineMode::Parallel);
    auto serial = Run(TimelineMode::Serial);
    auto parallel = Run(TimelineMode::Parallel);
    Require(serial.size() == parallel.size(), "Serial and parallel Jobs must produce the same number of values");
    for (size_t row = 0; row < serial.size(); ++row)
        Require(serial[row].Entity == parallel[row].Entity && serial[row].Value == parallel[row].Value,
            "Serial and parallel Jobs must preserve output order and values");
    std::cout << "ECSJobSmoke passed\n";
}
