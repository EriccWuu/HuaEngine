#include "HuaEngine/Core/Core.h"
#include "HuaEngine/Core/Assert.h"
#include "HuaEngine/ECS/Runtime/World.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#include <unordered_map>

namespace {
	void Require(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "[ECSBackendSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void VerifyEntityHashDistribution(uint32_t indexBase, uint32_t indexStep,
		uint32_t generationBase, uint32_t generationStep) {
		constexpr uint32_t KeyCount = 4096;
		std::unordered_map<HE::EntityId, uint32_t> values;
		values.reserve(KeyCount);
		for (uint32_t index = 0; index < KeyCount; ++index) {
			const HE::EntityId id{indexBase + index * indexStep, generationBase + index * generationStep};
			values.emplace(id, index);
		}
		Require(values.size() == KeyCount, "Expected all distinct EntityIds to remain addressable");
		size_t largestBucket = 0;
		for (size_t bucket = 0; bucket < values.bucket_count(); ++bucket) {
			const size_t bucketSize = values.bucket_size(bucket);
			if (bucketSize > largestBucket) {
				largestBucket = bucketSize;
			}
		}
		// Allow implementation differences while rejecting concentrated hash chains.
		Require(largestBucket < KeyCount / 16, "Expected EntityId hashes to distribute across unordered_map buckets");
		for (uint32_t index = 0; index < KeyCount; ++index) {
			const HE::EntityId id{indexBase + index * indexStep, generationBase + index * generationStep};
			Require(values.at(id) == index, "Expected EntityId lookup to preserve the associated value");
		}
		std::cout << "EntityId hash largest bucket: " << largestBucket << std::endl;
	}
}

struct BackendPosition {
	float X = 0.0f;
};

int main(int argc, char** argv) {
	if (argc == 2 && std::string_view(argv[1]) == "--verify-require") {
		Require(false, "Require guard is active");
	}
	bool engineAssertConditionEvaluated = false;
	HE_CORE_ASSERT((engineAssertConditionEvaluated = true), "Expected a true assertion condition");
#if defined(_DEBUG)
	Require(engineAssertConditionEvaluated, "Expected Debug engine assertions to evaluate their conditions");
	std::cout << "Engine assertion expressions are enabled" << std::endl;
#else
	Require(!engineAssertConditionEvaluated, "Expected non-Debug engine assertions to omit their conditions");
	std::cout << "Engine assertion expressions are disabled" << std::endl;
#endif
	VerifyEntityHashDistribution(0, 1, 1, 0);
	VerifyEntityHashDistribution(17, 0, 1, 1);
	VerifyEntityHashDistribution(0, uint32_t{1} << 20, 1, 0);
	VerifyEntityHashDistribution(17, 0, 1, uint32_t{1} << 20);
	VerifyEntityHashDistribution(std::numeric_limits<uint32_t>::max() - 4095, 1,
		std::numeric_limits<uint32_t>::max(), 0);
	HE::Ecs::EcsContext context;
	Require(context.Types().Register<BackendPosition>("Tests.BackendPosition").HasValue(), "Expected Context-local registration");
	HE::Ecs::World world(context);
	const auto entity = world.CreateEmpty("Backend Entity");
	Require(entity.HasValue(), "Expected runtime entity creation");
	Require(world.Emplace<BackendPosition>(entity.Value(), BackendPosition{8.0f}).HasValue(), "Expected runtime component storage");
	Require(world.TryGet<BackendPosition>(entity.Value())->X == 8.0f, "Expected chunk storage to retain the component value");

	return 0;
}
