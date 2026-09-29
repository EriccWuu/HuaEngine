#pragma once

#include "HuaEngine/Reflection/ReflectionMarkers.h"
#include <string>
#include <memory>

namespace P6Fixture {
    HE_REFLECT_COMPONENT(Guid="6e64049af5f0431ba7d734c4c0000001", DisplayName="Plain fixture", Category="Tests")
    struct PlainComponent {
        HE_REFLECT_FIELD()
        std::string Text;
        HE_REFLECT_FIELD()
        int Value = 0;
    };

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
}
