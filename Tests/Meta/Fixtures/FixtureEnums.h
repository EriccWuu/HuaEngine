#pragma once

#include "HuaEngine/Reflection/ReflectionMarkers.h"

namespace P6Fixture {
    enum class [[sattr(guid="6e64049af5f0431ba7d734c4c0000012"; reflect=@full; attrs=["DisplayName=Shared Mode"])]] SharedMode {
        Idle [[sattr(attrs=["DisplayName=Idle state"])]],
        Active
    };
}
