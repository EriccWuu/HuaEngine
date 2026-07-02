#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_set>

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/EntityId.h"

namespace {
	struct PositionComponent {
		float X = 0.0f;
	};

	struct VelocityComponent {
		float X = 0.0f;
	};

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ECSCoreSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	const HE::EntityId invalidEntity;
	Require(!invalidEntity, "Expected default EntityId to be invalid");
	Require(invalidEntity == HE::EntityId{0, 0}, "Expected default EntityId to be {0, 0}");
	Require(static_cast<bool>(HE::EntityId{7, 2}), "Expected non-zero generation EntityId to be valid");
	Require(HE::EntityId{7, 2} == HE::EntityId{7, 2}, "Expected identical EntityId values to compare equal");
	Require(HE::EntityId{7, 2} != HE::EntityId{7, 3}, "Expected different EntityId generations to compare unequal");

	std::unordered_set<HE::EntityId> entitySet;
	entitySet.insert(HE::EntityId{42, 5});
	Require(entitySet.find(HE::EntityId{42, 5}) != entitySet.end(), "Expected EntityId hash lookup to find equivalent id");

	const HE::EntityUuid uuid{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
	const auto uuidText = HE::ToString(uuid);
	Require(uuidText == "0123456789abcdeffedcba9876543210", "Expected UUID string to be 32 lowercase hex digits");
	Require(HE::EntityUuid::FromString(uuidText) == uuid, "Expected UUID to round-trip through string conversion");
	Require(HE::EntityUuid::FromString("0123456789ABCDEFFEDCBA9876543210") == uuid, "Expected UUID parser to accept uppercase hex");
	Require(HE::EntityUuid::FromString("not-a-uuid") == HE::EntityUuid{}, "Expected invalid UUID strings to return default UUID");

	const HE::ComponentTypeId positionTypeId = HE::ComponentTypeIdOf<PositionComponent>();
	const HE::ComponentTypeId velocityTypeId = HE::ComponentTypeIdOf<VelocityComponent>();
	Require(positionTypeId != HE::InvalidComponentTypeId, "Expected concrete component type id to be valid");
	Require(positionTypeId == HE::ComponentTypeIdOf<PositionComponent>(), "Expected component type id to be stable per type");
	Require(positionTypeId == HE::ComponentTypeIdOf<const PositionComponent&>(), "Expected cv/ref qualifiers to share component type id");
	Require(positionTypeId != velocityTypeId, "Expected different component types to have different ids");

	static_assert(std::is_same_v<HE::QueryTermTraits<PositionComponent>::ComponentType, PositionComponent>);
	static_assert(!HE::QueryTermTraits<PositionComponent>::IsReadOnly);
	static_assert(std::is_same_v<HE::QueryTermTraits<HE::Read<PositionComponent>>::ComponentType, PositionComponent>);
	static_assert(HE::QueryTermTraits<HE::Read<PositionComponent>>::IsReadOnly);
	static_assert(!HE::IsReadTerm<PositionComponent>::Value);
	static_assert(HE::IsReadTerm<HE::Read<PositionComponent>>::Value);

	std::cout << "ECSCoreSmoke passed" << std::endl;
	return 0;
}
