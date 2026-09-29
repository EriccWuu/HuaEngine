#include "Fixtures/ComponentModule.h"
#include <Test/GeneratedReflection.h>
#include "HuaEngine/ECS/Runtime/Job.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

HE::Ecs::Result<HE::Ecs::TypeId> RegisterPlainFromSecondTranslationUnit(HE::Ecs::EcsContext& context);
void RunMetaGeneratedContracts();

namespace {
    using namespace HE::Ecs;

    void Require(bool condition, const char* message) {
        if (!condition) { std::cerr << "[MetaGeneratedSmoke] " << message << '\n'; std::exit(1); }
    }

    template<typename T> T Take(Result<T> result) {
        if (!result) { std::cerr << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
        return std::move(result).Value();
    }

    void Check(Result<void> result) {
        if (!result) { std::cerr << result.GetError().Operation << ": " << result.GetError().Message << '\n'; std::exit(1); }
    }

    struct IncreasePlain final {
        ComponentView<P6Fixture::PlainComponent> Values;
        int Amount = 0;
        void build(AccessBuilder& access) const { access.write(&IncreasePlain::Values); }
        [[nodiscard]] Result<void> run(TaskContext& context) const {
            for (size_t row = 0; row < context.size(); ++row) {
                Values[row].Value += Amount;
                Values[row].Text += ":" + std::to_string(Amount);
            }
            return {};
        }
    };

    struct PlainSnapshot final {
        HE::EntityId Entity;
        int Value = 0;
        std::string Text;
        bool operator==(const PlainSnapshot&) const = default;
    };

    struct CollectPlain final {
        using Output = PlainSnapshot;
        ComponentView<const P6Fixture::PlainComponent> Values;
        void build(AccessBuilder& access) const { access.read(&CollectPlain::Values); }
        [[nodiscard]] Result<void> run(TaskContext& context, BatchOutput<Output>& output) const {
            for (size_t row = 0; row < context.size(); ++row)
                output.Emit({context.entity(row), Values[row].Value, Values[row].Text});
            return {};
        }
    };

    std::vector<PlainSnapshot> Run(TimelineMode mode) {
        EcsContext context({.WorkerCount = 2});
        Check(HE::Generated::Test::RegisterComponents(context.Types()));
        const auto type = Take(context.Types().Register<P6Fixture::PlainComponent>());
        Require(Take(RegisterPlainFromSecondTranslationUnit(context)) == type,
            "A generated component must keep one identity across translation units");
        const auto* descriptor = context.Types().Find(type);
        Require(descriptor && descriptor->Descriptor.Reflection &&
            descriptor->Descriptor.Guid == TypeGuid{0x6e64049af5f0431bULL, 0xa7d734c4c0000001ULL},
            "Generated registration must keep the declared Guid and runtime reflection");
        const HE::Refl::RuntimeFieldDescriptor* textField = nullptr;
        for (const auto& field : descriptor->Descriptor.Reflection->Fields)
            if (field.Name == "Text") textField = &field;
        Require(textField && HE::Refl::GetRuntimeFieldValueKind(*textField) == HE::Refl::RuntimeFieldValueKind::String &&
            HE::Refl::IsRuntimeFieldEditable(*textField), "A generated string field must be editable");

        World world(context, {.ChunkBytes = 65536});
        std::vector<HE::EntityId> entities;
        for (int index = 0; index < 600; ++index) {
            const auto entity = Take(world.CreateEmpty("component fixture", {1, static_cast<uint64_t>(index + 1)}));
            Check(world.Emplace<P6Fixture::PlainComponent>(entity, P6Fixture::PlainComponent{"item", index}));
            entities.push_back(entity);
        }
        Timeline timeline(context, {.Mode = mode});
        IncreasePlain first;
        first.Amount = 5;
        IncreasePlain second;
        second.Amount = 7;
        auto writerA = Take(timeline.Dispatch(world, first));
        auto writerB = Take(timeline.Dispatch(world, second));
        auto reader = Take(timeline.Dispatch(world, CollectPlain{}));
        auto values = Take(reader.Collect(timeline));
        Check(timeline.Finish());
        Require(writerA.Status() == TaskStatus::Succeeded && writerB.Status() == TaskStatus::Succeeded,
            "Jobs using generated components must complete in Timeline order");
        Require(values.size() == entities.size(), "The Job must visit every registered component");
        for (const auto& item : values) {
            const auto* component = std::as_const(world).TryGet<P6Fixture::PlainComponent>(item.Entity);
            Require(component && item.Value == component->Value && item.Text == component->Text &&
                item.Text == "item:5:7", "A Job must preserve non-POD component values");
        }
        return values;
    }
}

int main() {
#if defined(HUAENGINE_ASAN_REQUIRED)
#if !defined(__SANITIZE_ADDRESS__)
#error The generated module ASan target requires actual compiler instrumentation.
#endif
    std::cout << "AddressSanitizer instrumentation is enabled\n";
#endif
    RunMetaGeneratedContracts();
    Require(Run(TimelineMode::Serial) == Run(TimelineMode::Parallel),
        "Serial and parallel Jobs must produce the same results for generated components");
    std::cout << "MetaGeneratedSmoke passed\n";
    return 0;
}
