#include "ECSTestSupport.h"
#include "HuaEngine/ECS/Runtime/Job.h"

#include <vector>

namespace {
using namespace ECSTestSupport;
using namespace HE::Ecs;

struct Number { int Value = 0; };
struct Offset { int Value = 0; };
struct Bonus { int Value = 0; };
struct Hidden {};
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
