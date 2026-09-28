#pragma once

#include "HuaEngine/Reflection/ReflectionMarkers.h"
#include "HuaEngine/ECS/Runtime/GeneratedQuery.h"
#include "HuaEngine/ECS/Runtime/Commands.h"

#include <string>
#include <memory>
#include <atomic>
#include <stdexcept>

namespace P6Fixture {
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000001", DisplayName="Plain fixture", Category="Tests")
    struct PlainComponent {
        HE_REFLECT_FIELD()
        std::string Text;
        HE_REFLECT_FIELD()
        int Value = 0;
    };

    HE_ECS_QUERY()
    void AddValue(PlainComponent& component, HE::Ecs::Value<int> amount);

    HE_ECS_QUERY() /* A comment must not detach the declaration from its marker. */
    void OwnedValues(PlainComponent& component, HE::Ecs::Value<std::string> text,
        HE::Ecs::Value<std::unique_ptr<int>> number);

    struct DestructionControl {
        std::unique_ptr<HE::Ecs::World>* Target = nullptr;
        std::atomic<int> Calls{0};
    };
    HE_ECS_QUERY()
    void DestroyWorldAfterFirst(PlainComponent& component,
        HE::Ecs::Value<std::shared_ptr<DestructionControl>> control);

    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000002", DisplayName="Read", Category="Tests")
    struct ReadComponent { HE_REFLECT_FIELD() int Value = 0; };
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000003", DisplayName="Optional read", Category="Tests")
    struct OptionalRead { HE_REFLECT_FIELD() int Value = 0; };
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000004", DisplayName="Optional write", Category="Tests")
    struct OptionalWrite { HE_REFLECT_FIELD() int Value = 0; };
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000005", DisplayName="Active", Category="Tests", Tag=true)
    struct Active {};
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000006", DisplayName="Excluded", Category="Tests", Tag=true)
    struct Excluded {};
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000007", DisplayName="Shared", Category="Tests")
    struct SharedComponent { HE_REFLECT_FIELD() int Value = 0; };
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000008", DisplayName="No default", Category="Tests")
    struct NoDefault {
        explicit NoDefault(int value) : Value(value) {}
        HE_REFLECT_FIELD() int Value;
    };
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000009", DisplayName="Move only", Category="Tests")
    struct MoveOnly {
        std::unique_ptr<int> Owner = std::make_unique<int>(42);
        HE_REFLECT_FIELD() int Value = 0;
    };
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c000000a", DisplayName="Immovable", Category="Tests")
    struct Immovable {
        explicit Immovable(int value) : Value(value) {}
        Immovable(const Immovable&) = delete;
        Immovable(Immovable&&) = delete;
        HE_REFLECT_FIELD() int Value;
    };
    using RequiredAlias = ReadComponent;
    HE_ECS_QUERY(Required(RequiredAlias), Tag(Active), Without(Excluded), Changed(ReadComponent), Shared(SharedComponent))
    void AllArguments(PlainComponent&, const ReadComponent&, const OptionalRead*, OptionalWrite*,
        HE::Ecs::Value<HE::EntityId>, HE::Ecs::RandomRead<ReadComponent>, HE::Ecs::RandomWrite<OptionalWrite>,
        HE::Ecs::ResourceRead<int>, HE::Ecs::ResourceWrite<int>, HE::Ecs::CommandBuffer&);

    HE_ECS_QUERY(IncludeDisabled, IgnoreComponentEnabled)
    void IncludeAll(PlainComponent&);

    struct OutputRecord {
        HE::EntityId Entity;
        std::unique_ptr<int> Number;
        std::string Text;
    };
    struct OutputControl {
        std::atomic<bool> Release{false};
        std::atomic<int> Active{0};
        std::atomic<int> Peak{0};
    };
    using EntityAlias = HE::EntityId;
    HE_ECS_QUERY()
    void ExtractOutput(EntityAlias, const PlainComponent&, HE::Ecs::Value<int>,
        HE::Ecs::Value<std::shared_ptr<OutputControl>>, HE::Ecs::BatchOutput<OutputRecord>);
    HE_ECS_QUERY()
    void FailOutput(HE::EntityId, PlainComponent&, HE::Ecs::Value<std::shared_ptr<OutputControl>>,
        HE::Ecs::BatchOutput<OutputRecord>);
    struct ThrowingOutput {
        inline static std::atomic<bool> ThrowOnMove{false};
        inline static std::atomic<int> Live{0};
        int Number;
        explicit ThrowingOutput(int number) : Number(number) { ++Live; }
        ThrowingOutput(const ThrowingOutput&) = delete;
        ThrowingOutput(ThrowingOutput&& other) : Number(other.Number) {
            if (ThrowOnMove.load()) throw std::runtime_error("intentional output move failure");
            ++Live;
        }
        ~ThrowingOutput() { --Live; }
    };
    HE_ECS_QUERY()
    void ExtractThrowing(const PlainComponent&, HE::Ecs::BatchOutput<ThrowingOutput>);
}
