#pragma once

#include "HuaEngine/ECS/Runtime/World.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace ECSTestSupport {
inline void Require(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "[ECS test] " << message << '\n'; std::exit(1); }
}

template<typename T>
T Take(HE::Ecs::Result<T> result) {
    Require(result.HasValue(), result ? "" : result.GetError().Operation + ": " + result.GetError().Message);
    return std::move(result).Value();
}

inline void Check(HE::Ecs::Result<void> result) {
    Require(result.HasValue(), result ? "" : result.GetError().Operation + ": " + result.GetError().Message);
}

template<typename T, typename... Args>
T& Add(HE::Ecs::World& world, HE::EntityId id, Args&&... args) {
    Check(world.Emplace<T>(id, std::forward<Args>(args)...));
    auto* value = world.TryGet<T>(id);
    Require(value != nullptr, "Expected inserted component lookup");
    return *value;
}

template<typename T>
T& Get(HE::Ecs::World& world, HE::EntityId id) {
    auto* value = world.TryGet<T>(id);
    Require(value != nullptr, "Expected required component lookup");
    return *value;
}

template<typename T>
void Remove(HE::Ecs::World& world, HE::EntityId id) {
    const auto* type = world.Types().Find<T>();
    Require(type != nullptr, "Expected removal of a registered component");
    Check(world.Remove(id, type->Id));
}
}
