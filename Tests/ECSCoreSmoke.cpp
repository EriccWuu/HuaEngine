#include "ECSTestSupport.h"
#include "HuaEngine/ECS/EntityId.h"
#include "HuaEngine/ECS/Runtime/TypeRegistry.h"
#include <unordered_set>

namespace {
struct Position { float X = 0; float Y = 0; };
struct Velocity { float X = 0; };
}

int main() {
    using namespace ECSTestSupport;
    using namespace HE::Ecs;
    Require(!HE::EntityId{} && HE::EntityId{} == HE::EntityId{0, 0}, "Expected default ID to be invalid");
    Require(static_cast<bool>(HE::EntityId{7, 2}) && HE::EntityId{7, 2} != HE::EntityId{7, 3},
        "Expected generations to participate in entity identity");
    std::unordered_set<HE::EntityId> ids{{42, 5}};
    Require(ids.contains({42, 5}), "Expected hashed entity lookup");
    const HE::EntityUuid uuid{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    Require(HE::ToString(uuid) == "0123456789abcdeffedcba9876543210", "Expected canonical UUID text");
    Require(HE::EntityUuid::FromString(HE::ToString(uuid)) == uuid &&
        HE::EntityUuid::FromString("0123456789ABCDEFFEDCBA9876543210") == uuid &&
        HE::EntityUuid::FromString("not-a-uuid") == HE::EntityUuid{}, "Expected UUID roundtrip and strict parsing");
    TypeRegistry registry;
    const auto guid = TypeGuid{0x8137ef5f1fae4583ULL, 0xb189eb7ff45cb08eULL};
    const auto position = Take(registry.Register<Position>(guid, "Tests.SmokePosition"));
    const auto* metadata = registry.Find<Position>();
    Require(metadata && metadata == registry.Find(position) && metadata == registry.Find(guid) &&
        metadata == registry.FindByName("Tests.SmokePosition") && metadata->Owner == &registry,
        "Expected all lookup paths to resolve the same Context-owned descriptor");
    Require(metadata->Descriptor.Size == sizeof(Position) && metadata->Descriptor.Alignment == alignof(Position) &&
        metadata->Descriptor.Storage == StorageKind::Direct, "Expected native layout metadata");
    auto value = Take(OwnedValue::Default(*metadata));
    *static_cast<Position*>(value.Data()) = {42, 24};
    auto copy = Take(value.Clone());
    Require(static_cast<const Position*>(copy.Data())->X == 42 && static_cast<const Position*>(copy.Data())->Y == 24,
        "Expected descriptor-driven owned copying to retain values");
    Require(Take(registry.Register<Position>(guid, "Tests.SmokePosition")) == position && registry.All().size() == 1,
        "Expected exact duplicate registration to be idempotent");
    Require(!registry.Register<Position>(guid, "Tests.RenamedPosition"), "Expected conflicting native identity to fail");
    Require(!registry.Register<Velocity>(TypeGuid::FromName("Tests.Velocity"), "Tests.SmokePosition"),
        "Expected duplicate stable names to fail");
    Require(registry.All().size() == 1, "Expected rejected registrations to leave no entries");
    std::cout << "ECSCoreSmoke passed\n";
}
