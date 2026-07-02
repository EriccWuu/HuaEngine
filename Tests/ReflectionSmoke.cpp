#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Reflection/Reflection.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionSmoke] " << message << std::endl;
			std::exit(1);
		}
	}
}

int main() {
	HE::TransformComponent transform;
	transform.Position = { 1.0f, 2.0f, 3.0f };
	transform.Rotation = { 4.0f, 5.0f, 6.0f };
	transform.Scale = { 7.0f, 8.0f, 9.0f };

	auto typeInfo = HE::Refl::reflect<HE::TransformComponent>();

	uint32_t fieldCount = 0;
	typeInfo.visit_member_variables([&](auto&& field) {
		++fieldCount;
		const std::string fieldName(field.name());
		if (fieldName == "Position") {
			Require(field.GetValue(&transform).x == 1.0f, "Expected reflected Position to read original value");
			field.SetValue(&transform, glm::vec3{ 10.0f, 11.0f, 12.0f });
		}
		else if (fieldName == "Rotation") {
			Require(field.GetValue(&transform).y == 5.0f, "Expected reflected Rotation to read original value");
			field.SetValue(&transform, glm::vec3{ 13.0f, 14.0f, 15.0f });
		}
		else if (fieldName == "Scale") {
			Require(field.GetValue(&transform).z == 9.0f, "Expected reflected Scale to read original value");
			field.SetValue(&transform, glm::vec3{ 16.0f, 17.0f, 18.0f });
		}
		else {
			Require(false, "Unexpected reflected TransformComponent field: " + fieldName);
		}
	});

	Require(fieldCount == 3, "Expected TransformComponent to expose three reflected fields");
	Require(transform.Position == glm::vec3(10.0f, 11.0f, 12.0f), "Expected reflected Position write to update component");
	Require(transform.Rotation == glm::vec3(13.0f, 14.0f, 15.0f), "Expected reflected Rotation write to update component");
	Require(transform.Scale == glm::vec3(16.0f, 17.0f, 18.0f), "Expected reflected Scale write to update component");

	std::cout << "ReflectionSmoke passed" << std::endl;
	return 0;
}
