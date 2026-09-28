#include "Fixtures/ComponentModule.h"
#include <Test/GeneratedReflection.h>
#include <Test/GeneratedQueries.h>

#include "HuaEngine/ECS/Runtime/Timeline.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <tuple>
#include <type_traits>

HE::Ecs::Result<HE::Ecs::TypeId> RegisterPlainFromSecondTranslationUnit(HE::Ecs::EcsContext& context);
void RunGeneratedContracts();

namespace {
    using namespace HE::Ecs;
    void Require(bool condition, const char* message) {
        if (!condition) { std::cerr << "[GeneratedQuerySmoke] " << message << '\n'; std::exit(1); }
    }
    template<typename T> T Take(Result<T> result) {
        if (!result) { std::cerr << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
        return std::move(result).Value();
    }
    void Check(Result<void> result) {
        if (!result) { std::cerr << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
    }
    std::vector<std::pair<int, std::string>> Run(TimelineMode mode) {
        EcsContext context({.WorkerCount = 2});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        const auto type = Take(context.Types().Register<P6Fixture::PlainComponent>());
        Require(Take(RegisterPlainFromSecondTranslationUnit(context)) == type, "Expected canonical generated traits in every translation unit");
        const auto* descriptor = context.Types().Find(type);
        Require(descriptor && descriptor->Descriptor.Reflection && descriptor->Descriptor.Guid == TypeGuid{0x6e64049af5f0431bULL, 0xa7d734c4c0000001ULL},
            "Expected the explicitly declared Guid and generated runtime reflection");
        const HE::Refl::RuntimeFieldDescriptor* textField = nullptr;
        for (const auto& field : descriptor->Descriptor.Reflection->Fields) if (field.Name == "Text") textField = &field;
        Require(textField && HE::Refl::GetRuntimeFieldValueKind(*textField) == HE::Refl::RuntimeFieldValueKind::String &&
            HE::Refl::IsRuntimeFieldEditable(*textField), "Generated std::string fields must preserve editable runtime semantics");
        World world(context, {.ChunkBytes = 65536});
        std::vector<HE::EntityId> entities;
        for (int index = 0; index < 600; ++index) {
            const auto entity = Take(world.CreateEmpty("generated fixture", {1, static_cast<uint64_t>(index + 1)}));
            Check(world.Emplace<P6Fixture::PlainComponent>(entity, P6Fixture::PlainComponent{"item", index}));
            entities.push_back(entity);
        }
        Timeline timeline(context, {.Mode = mode});
        const auto first = Take(HE::Generated::Test::Submit_P6Fixture__AddValue(timeline, world, 5));
        int amount = 7;
        const auto second = Take(HE::Generated::Test::Submit_P6Fixture__AddValue(timeline, world, amount));
        amount = 999;
        std::string text = ":owned";
        auto number = std::make_unique<int>(3);
        const auto third = Take(HE::Generated::Test::Submit_P6Fixture__OwnedValues(timeline, world, std::move(text), std::move(number)));
        text = "changed";
        Require(!number, "Expected move-only Value ownership to transfer at submission");
        Check(timeline.Finish());
        Require(first.Status() == TaskStatus::Succeeded && second.Status() == TaskStatus::Succeeded && third.Status() == TaskStatus::Succeeded, "Expected generated tasks to finish in Timeline order");
        std::vector<std::pair<int, std::string>> result;
        for (size_t index = 0; index < entities.size(); ++index) {
            const auto* component = static_cast<const World&>(world).TryGet<P6Fixture::PlainComponent>(entities[index]);
            Require(component && component->Value == static_cast<int>(index + 15) && component->Text == "item:5:7:owned",
                "Expected non-POD data and owned Value payloads to survive deferred execution");
            result.emplace_back(component->Value, component->Text);
        }
        return result;
    }

    void RunWorldDestruction(TimelineMode mode, int rows) {
        EcsContext context({.WorkerCount = 2});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        auto world = std::make_unique<World>(context, WorldOptions{.ChunkBytes = 65536});
        for (int index = 0; index < rows; ++index) {
            const auto entity = Take(world->CreateEmpty("destroy during generated query"));
            Check(world->Emplace<P6Fixture::PlainComponent>(entity, P6Fixture::PlainComponent{"row", index}));
        }
        Timeline timeline(context, {.Mode = mode});
        auto control = std::make_shared<P6Fixture::DestructionControl>();
        control->Target = &world;
        const auto task = Take(HE::Generated::Test::Submit_P6Fixture__DestroyWorldAfterFirst(timeline, *world, control));
        const auto finished = timeline.Finish();
        Require(!finished && finished.GetError().Code == ErrorCode::InvalidState,
            "Destroying a World during a generated batch must report invalid state");
        Require(!world && control->Calls.load() == 1 && task.Status() == TaskStatus::Failed,
            "Generated execution must stop before accessing another row after World destruction");
        Check(timeline.ResetError());
    }

    using OutputSnapshot = std::vector<std::tuple<HE::EntityId, int, std::string>>;
    OutputSnapshot RunOutput(TimelineMode mode) {
        static_assert(!std::is_copy_constructible_v<BatchOutput<P6Fixture::OutputRecord>>);
        static_assert(!std::is_move_constructible_v<BatchOutput<P6Fixture::OutputRecord>>);
        EcsContext context({.WorkerCount = 3});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        World world(context, {.ChunkBytes = 65536});
        std::vector<HE::EntityId> entities;
        for (int index = 0; index < 700; ++index) {
            auto id = Take(world.CreateEmpty("output"));
            Check(world.Emplace<P6Fixture::PlainComponent>(id, P6Fixture::PlainComponent{"row-" + std::to_string(index), index}));
            entities.push_back(id);
        }
        const auto stale = entities[5];
        Check(world.Destroy(stale));
        const auto replacement = Take(world.CreateEmpty("replacement"));
        Require(replacement.Index == stale.Index && replacement.Generation != stale.Generation,
            "Output fixture must exercise a reused slot with a different generation");
        Check(world.Emplace<P6Fixture::PlainComponent>(replacement, P6Fixture::PlainComponent{"replacement", 3000}));
        const auto plainType = context.Types().Find<P6Fixture::PlainComponent>()->Id;
        for (size_t index = 19; index < entities.size(); index += 19) Check(world.SetEnabled(entities[index], false));
        for (size_t index = 23; index < entities.size(); index += 23) Check(world.SetComponentEnabled(entities[index], plainType, false));
        Timeline timeline(context, {.Mode = mode});
        auto control = std::make_shared<P6Fixture::OutputControl>();
        auto pending = Take(HE::Generated::Test::Submit_P6Fixture__ExtractOutput(timeline, world, 10, control));
        auto second = Take(HE::Generated::Test::Submit_P6Fixture__ExtractOutput(timeline, world, 20, nullptr));
        auto moved = std::move(pending);
        Require(!pending.Collect(timeline), "Moved-from output task must reject collection");
        // Drop a return object before execution; the callback must own its collector.
        { auto ignored = Take(HE::Generated::Test::Submit_P6Fixture__ExtractOutput(timeline, world, 30, nullptr)); }
        Check(timeline.Finish());
        auto output = Take(moved.Collect(timeline));
        auto secondOutput = Take(second.Collect(timeline));
        Require(!moved.Collect(timeline), "Output collection must be single-use");
        Require(output.size() == secondOutput.size() && !output.empty(), "Every submission must own an independent result");
        if (mode == TimelineMode::Parallel)
            Require(control->Peak.load() > 1, "BatchOutput must allow actual concurrent worker batches");
        else Require(control->Peak.load() == 1, "Serial output execution must remain serial");
        size_t expected = 0;
        for (const auto id : world.Entities()) {
            const auto* component = std::as_const(world).TryGet<P6Fixture::PlainComponent>(id);
            if (!world.IsEnabled(id) || !world.IsComponentEnabled(id, plainType) || component->Value < 256) continue;
            expected += 1 + (component->Value % 13 == 0 ? 1 : 0);
        }
        Require(output.size() == expected, "Output must preserve exact matching rows and explicit duplicate emits");
        OutputSnapshot snapshot;
        bool sawReplacement = false;
        for (size_t index = 0; index < output.size(); ++index) {
            const auto& item = output[index];
            const auto* component = std::as_const(world).TryGet<P6Fixture::PlainComponent>(item.Entity);
            Require(component && item.Number && item.Entity != stale, "Injected EntityId must preserve actual generation");
            Require(item.Entity == secondOutput[index].Entity, "Submission outputs must use deterministic batch and row order");
            const bool duplicate = item.Text == "duplicate";
            Require(*item.Number == (duplicate ? -component->Value : component->Value + 10), "Move-only output value mismatch");
            Require(*secondOutput[index].Number == (duplicate ? -component->Value : component->Value + 20), "Collector payload crossed submissions");
            sawReplacement |= item.Entity == replacement;
            snapshot.emplace_back(item.Entity, *item.Number, item.Text);
        }
        Require(sawReplacement, "Expected injected replacement entity identity");

        World empty(context);
        auto emptyTask = Take(HE::Generated::Test::Submit_P6Fixture__ExtractOutput(timeline, empty, 0, nullptr));
        Require(Take(emptyTask.Collect(timeline)).empty(), "A successful zero-chunk task must collect an empty vector");
        auto noEmission = Take(empty.CreateEmpty());
        Check(empty.Emplace<P6Fixture::PlainComponent>(noEmission, P6Fixture::PlainComponent{"none", 1}));
        auto noOutput = Take(HE::Generated::Test::Submit_P6Fixture__ExtractOutput(timeline, empty, 0, nullptr));
        Require(Take(noOutput.Collect(timeline)).empty(), "A successful batch may intentionally emit no values");

        auto throwing = Take(HE::Generated::Test::Submit_P6Fixture__ExtractThrowing(timeline, empty));
        Check(timeline.Finish());
        Require(P6Fixture::ThrowingOutput::Live.load() == 1, "A completed collector must own its output until consumed");
        P6Fixture::ThrowingOutput::ThrowOnMove = true;
        auto moveFailure = throwing.Collect(timeline);
        P6Fixture::ThrowingOutput::ThrowOnMove = false;
        Require(!moveFailure && moveFailure.GetError().Code == ErrorCode::ConstructionFailed && P6Fixture::ThrowingOutput::Live.load() == 0,
            "Throwing output moves must return an error and release the owned values");
        Require(!throwing.Collect(timeline), "A failed collection consumes the output exactly once");

        auto failureControl = std::make_shared<P6Fixture::OutputControl>();
        auto failed = Take(HE::Generated::Test::Submit_P6Fixture__FailOutput(timeline, world, failureControl));
        auto cancelled = Take(HE::Generated::Test::Submit_P6Fixture__ExtractOutput(timeline, world, 0, nullptr));
        failureControl->Release = true;
        Require(!timeline.Finish(), "An output callback exception must fail the timeline");
        auto failedResult = failed.Collect(timeline);
        auto cancelledResult = cancelled.Collect(timeline);
        Require(!failedResult && !cancelledResult, "Failed or dependency-cancelled tasks must never deliver partial output");
        Require(failed.Task().Status() == TaskStatus::Failed && cancelled.Task().Status() == TaskStatus::Cancelled,
            "Generated output tasks must expose actual failure and cancellation status");
        Check(timeline.ResetError());
        return snapshot;
    }
}

int main() {
#if defined(HUAENGINE_ASAN_REQUIRED)
#if !defined(__SANITIZE_ADDRESS__)
#error The generated module ASan target requires actual compiler instrumentation.
#endif
    std::cout << "AddressSanitizer instrumentation is enabled\n";
#endif
    RunGeneratedContracts();
    Require(Run(TimelineMode::Serial) == Run(TimelineMode::Parallel), "Expected generated Serial and Parallel Query results to match");
    RunWorldDestruction(TimelineMode::Serial, 1);
    RunWorldDestruction(TimelineMode::Serial, 2);
    RunWorldDestruction(TimelineMode::Parallel, 1);
    RunWorldDestruction(TimelineMode::Parallel, 2);
    Require(RunOutput(TimelineMode::Serial) == RunOutput(TimelineMode::Parallel),
        "Entity injection and move-only output order must match in Serial and Parallel modes");
    std::cout << "GeneratedQuerySmoke passed: Clang/Python/MSVC module, two-TU traits, non-POD and owned Value\n";
    return 0;
}
