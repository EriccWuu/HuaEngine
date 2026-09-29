#pragma once

#include "HuaEngine/Reflection/ReflectionMarkers.h"
#include "FixtureEnums.h"
#include <string>
#include <memory>

namespace P6Fixture {
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000010"; reflect=@marked; attrs=["DisplayName=Plain reflected","Category=Tests"])]] PlainReflected {
        [[sattr()]]
        std::string Label;
        [[sattr()]]
        SharedMode Mode = SharedMode::Idle;
    };

    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000001"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Plain fixture","Category=Tests","Script.Visible=true"])]] PlainComponent {
        [[sattr(attrs=["Inspector.Label=Text"])]]
        std::string Text;
        [[sattr(flags=["ScriptVisible"]; attrs=["Serialization.Alias=Value"])]]
        int Value = 0;
    };

    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000002"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Read","Category=Tests"])]] ReadComponent { [[sattr()]] int Value = 0; };
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000003"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Optional read","Category=Tests"])]] OptionalRead { [[sattr()]] int Value = 0; };
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000004"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Optional write","Category=Tests"])]] OptionalWrite { [[sattr()]] int Value = 0; };
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000005"; reflect=@marked; flags=["Component","Tag"]; attrs=["DisplayName=Active","Category=Tests"])]] Active {};
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000006"; reflect=@marked; flags=["Component","Tag"]; attrs=["DisplayName=Excluded","Category=Tests"])]] Excluded {};
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000007"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Shared","Category=Tests"])]] SharedComponent { [[sattr()]] int Value = 0; };
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000008"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=No default","Category=Tests"])]] NoDefault {
        explicit NoDefault(int value) : Value(value) {}
        [[sattr()]] int Value;
    };
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000009"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Move only","Category=Tests"])]] MoveOnly {
        std::unique_ptr<int> Owner = std::make_unique<int>(42);
        [[sattr()]] int Value = 0;
    };
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c000000a"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Immovable","Category=Tests"])]] Immovable {
        explicit Immovable(int value) : Value(value) {}
        Immovable(const Immovable&) = delete;
        Immovable(Immovable&&) = delete;
        [[sattr()]] int Value;
    };
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c000000b"; reflect=@marked; flags=["Component"]; attrs=["DisplayName=Linked","Category=Tests"])]] LinkedComponent { [[sattr()]] SharedMode Mode = SharedMode::Idle; };
}
