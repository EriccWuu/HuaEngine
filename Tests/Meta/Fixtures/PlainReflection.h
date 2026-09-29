#pragma once

#include "HuaEngine/Reflection/ReflectionMarkers.h"
#include "FixtureEnums.h"

#include <string>

namespace P6Fixture {
    struct [[sattr(guid="6e64049af5f0431ba7d734c4c0000011"; reflect=@marked; attrs=["DisplayName=Independent value","Category=Tests"])]] IndependentValue {
        [[sattr()]]
        std::string Label;
        [[sattr()]]
        SharedMode Mode = SharedMode::Idle;
    };
}
