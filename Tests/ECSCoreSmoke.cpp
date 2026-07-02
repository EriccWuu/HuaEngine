#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <unordered_set>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/EntityId.h"

namespace {
	struct SmokePosition {
		float X = 0.0f;
		float Y = 0.0f;
	};

	struct SmokeVelocity {
		float X = 0.0f;
		float Y = 0.0f;
	};

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

	HE::ComponentRegistry registry;
	HE::ComponentRegistration smokePositionRegistration;
	smokePositionRegistration.TypeName = "Tests.SmokePosition";
	smokePositionRegistration.DisplayName = "Smoke Position";
	smokePositionRegistration.Category = "Tests";

	Require(registry.Register<SmokePosition>(smokePositionRegistration), "Expected SmokePosition registration to succeed");
	const HE::ComponentMetadata* smokePositionByType = registry.FindByType<SmokePosition>();
	Require(smokePositionByType != nullptr, "Expected SmokePosition lookup by type to succeed");
	Require(smokePositionByType->TypeId == HE::ComponentTypeIdOf<SmokePosition>(), "Expected SmokePosition metadata type id to match");
	Require(smokePositionByType->TypeName == "Tests.SmokePosition", "Expected SmokePosition metadata type name to match");
	Require(smokePositionByType->DisplayName == "Smoke Position", "Expected SmokePosition metadata display name to match");
	Require(smokePositionByType->Category == "Tests", "Expected SmokePosition metadata category to match");
	Require(smokePositionByType->Size == sizeof(SmokePosition), "Expected SmokePosition metadata size to match");
	Require(!smokePositionByType->AllowMultiple, "Expected SmokePosition registration to disallow multiple by default");

	const HE::ComponentMetadata* smokePositionByName = registry.FindByName(smokePositionRegistration.TypeName);
	Require(smokePositionByName == smokePositionByType, "Expected SmokePosition lookup by name to return the same metadata");
	Require(registry.FindByTypeId(HE::ComponentTypeIdOf<SmokePosition>()) == smokePositionByType, "Expected SmokePosition lookup by type id to return the same metadata");
	Require(registry.GetAll().size() == 1, "Expected registry to contain one component registration");

	void* constructedSmokePosition = smokePositionByType->ConstructDefault();
	Require(constructedSmokePosition != nullptr, "Expected SmokePosition ConstructDefault to allocate an instance");
	static_cast<SmokePosition*>(constructedSmokePosition)->X = 42.0f;
	static_cast<SmokePosition*>(constructedSmokePosition)->Y = 24.0f;
	void* copiedSmokePosition = smokePositionByType->Copy(constructedSmokePosition);
	Require(copiedSmokePosition != nullptr, "Expected SmokePosition Copy to allocate an instance");
	Require(static_cast<SmokePosition*>(copiedSmokePosition)->X == 42.0f, "Expected SmokePosition Copy to preserve X");
	Require(static_cast<SmokePosition*>(copiedSmokePosition)->Y == 24.0f, "Expected SmokePosition Copy to preserve Y");
	smokePositionByType->Destroy(copiedSmokePosition);
	smokePositionByType->Destroy(constructedSmokePosition);

	HE::ComponentRegistration duplicateTypeRegistration;
	duplicateTypeRegistration.TypeName = "Tests.SmokePositionDuplicateType";
	duplicateTypeRegistration.DisplayName = "Smoke Position Duplicate Type";
	duplicateTypeRegistration.Category = "Tests";
	Require(!registry.Register<SmokePosition>(duplicateTypeRegistration), "Expected duplicate SmokePosition type registration to fail");

	HE::ComponentRegistration duplicateNameRegistration;
	duplicateNameRegistration.TypeName = "Tests.SmokePosition";
	duplicateNameRegistration.DisplayName = "Smoke Velocity";
	duplicateNameRegistration.Category = "Tests";
	Require(!registry.Register<SmokeVelocity>(duplicateNameRegistration), "Expected duplicate SmokePosition type name registration to fail");

	std::cout << "ECSCoreSmoke passed" << std::endl;
	return 0;
}
