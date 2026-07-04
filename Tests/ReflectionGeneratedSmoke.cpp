#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

#include "HuaEngine/ECS/ComponentRegistry.h"
#include "HuaEngine/Generated/GeneratedReflection.h"
#include "HuaEngine/Reflection/Reflection.h"

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionGeneratedSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	bool HasField(std::span<const HE::Generated::ReflectedFieldInfo> fields, std::string_view name) {
		for (const HE::Generated::ReflectedFieldInfo& field : fields) {
			if (field.Name == name) {
				return true;
			}
		}

		return false;
	}

	bool HasField(std::span<const HE::Refl::RuntimeFieldDescriptor> fields, std::string_view name) {
		for (const HE::Refl::RuntimeFieldDescriptor& field : fields) {
			if (field.Name == name) {
				return true;
			}
		}

		return false;
	}
}

int main() {
	const std::span<const HE::Generated::ReflectedTypeInfo> types = HE::Generated::GetReflectedTypes();
	Require(types.size() == 5, "Expected five reflected component types");

	const HE::Generated::ReflectedTypeInfo* transform = HE::Generated::FindReflectedType("HE::TransformComponent");
	Require(transform != nullptr, "Expected to find HE::TransformComponent");
	Require(HasField(transform->Fields, "Position"), "Expected TransformComponent Position field");
	Require(HasField(transform->Fields, "Rotation"), "Expected TransformComponent Rotation field");
	Require(HasField(transform->Fields, "Scale"), "Expected TransformComponent Scale field");

	const auto* transformRuntime = HE::Refl::FindRuntimeType("HE::TransformComponent");
	Require(transformRuntime != nullptr, "Expected runtime reflection descriptor for TransformComponent");
	Require(transformRuntime->Fields.size() == 3, "Expected TransformComponent runtime fields");
	Require(HasField(transformRuntime->Fields, "Position"), "Expected TransformComponent runtime Position field");
	Require(HasField(transformRuntime->Fields, "Rotation"), "Expected TransformComponent runtime Rotation field");
	Require(HasField(transformRuntime->Fields, "Scale"), "Expected TransformComponent runtime Scale field");

	const HE::Generated::ReflectedTypeInfo* mesh = HE::Generated::FindReflectedType("HE::Rendering::MeshComponent");
	Require(mesh != nullptr, "Expected to find HE::Rendering::MeshComponent");
	Require(HasField(mesh->Fields, "MeshAssetName"), "Expected MeshComponent MeshAssetName field");
	Require(!HasField(mesh->Fields, "m_CachedVertexArray"), "Expected MeshComponent cache field to be omitted");

	HE::ComponentRegistry registry;
	HE::Generated::RegisterGeneratedComponents(registry);

	constexpr std::string_view expectedNames[] = {
		"NameComponent",
		"TransformComponent",
		"CameraComponent",
		"MeshComponent",
		"MaterialComponent",
	};
	for (const std::string_view name : expectedNames) {
		Require(registry.FindByName(name) != nullptr, "Expected registered component: " + std::string(name));
	}

	std::cout << "ReflectionGeneratedSmoke passed" << std::endl;
	return 0;
}
