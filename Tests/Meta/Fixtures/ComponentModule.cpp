#include "ComponentModule.h"
#include "HuaEngine/ECS/Runtime/Commands.h"
#include <stdexcept>
#include <thread>
#include <chrono>

namespace P6Fixture {
    void AddValue(PlainComponent& component, HE::Ecs::Value<int> amount) {
        component.Value += amount.Get();
        component.Text += ":" + std::to_string(amount.Get());
    }
}

void P6Fixture::OwnedValues(PlainComponent& component, HE::Ecs::Value<std::string> text,
    HE::Ecs::Value<std::unique_ptr<int>> number) {
    component.Text += text.Get();
    component.Value += *number.Get();
}

void P6Fixture::DestroyWorldAfterFirst(PlainComponent& component,
    HE::Ecs::Value<std::shared_ptr<DestructionControl>> control) {
    component.Value += 1;
    control.Get()->Calls.fetch_add(1, std::memory_order_relaxed);
    control.Get()->Target->reset();
}

void P6Fixture::AllArguments(PlainComponent& component, const ReadComponent& required,
    const OptionalRead* optionalRead, OptionalWrite* optionalWrite, HE::Ecs::Value<HE::EntityId> target,
    HE::Ecs::RandomRead<ReadComponent> randomRead, HE::Ecs::RandomWrite<OptionalWrite> randomWrite,
    HE::Ecs::ResourceRead<int> amount, HE::Ecs::ResourceWrite<int> total, HE::Ecs::CommandBuffer& commands) {
    auto read = randomRead.TryGet(target.Get());
    auto write = randomWrite.TryGet(target.Get());
    if (!read || !read.Value() || !write || !write.Value()) throw std::logic_error("Missing declared random target");
    component.Value += required.Value + (optionalRead ? optionalRead->Value : 0) + read.Value()->Value + amount.Get();
    if (optionalWrite) optionalWrite->Value += 1;
    write.Value()->Value += 2;
    total.Get() += 1;
    if (!commands.CreateEmpty("generated-command")) throw std::logic_error("Cannot record generated command");
}

void P6Fixture::IncludeAll(PlainComponent& component) { component.Value += 100; }

void P6Fixture::ExtractOutput(EntityAlias entity, const PlainComponent& component, HE::Ecs::Value<int> offset,
    HE::Ecs::Value<std::shared_ptr<OutputControl>> control, HE::Ecs::BatchOutput<OutputRecord> output) {
    if (control.Get()) {
        const int active = control.Get()->Active.fetch_add(1) + 1;
        int previous = control.Get()->Peak.load();
        while (previous < active && !control.Get()->Peak.compare_exchange_weak(previous, active)) {}
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        control.Get()->Active.fetch_sub(1);
    }
    // Some batches deliberately emit no rows, while others emit multiple rows.
    if (component.Value < 256) return;
    output.Emit({entity, std::make_unique<int>(component.Value + offset.Get()), component.Text});
    if (component.Value % 13 == 0)
        output.Emit({entity, std::make_unique<int>(-component.Value), "duplicate"});
}

void P6Fixture::FailOutput(HE::EntityId entity, PlainComponent& component,
    HE::Ecs::Value<std::shared_ptr<OutputControl>> control, HE::Ecs::BatchOutput<OutputRecord> output) {
    output.Emit({entity, std::make_unique<int>(component.Value), component.Text});
    if (component.Value == 0) {
        while (!control.Get()->Release.load()) std::this_thread::yield();
        throw std::runtime_error("intentional generated output failure");
    }
}

void P6Fixture::ExtractThrowing(const PlainComponent& component, HE::Ecs::BatchOutput<ThrowingOutput> output) {
    output.Emit(ThrowingOutput(component.Value));
}
