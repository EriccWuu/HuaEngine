#include <cstdlib>
#include <iostream>
#include <string>

#include "HuaEngine/Project/ProjectContext.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Serialization/Serialization.h"

namespace HE {
	struct RuntimeProviderFixture {
		std::string Name = "default";
		int Value = 7;
	};
}

srefl_class(HE::RuntimeProviderFixture,
	fields(
		field(Name),
		field(Value)
	)
)

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ReflectionRuntimeProviderSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	void VerifyStaticRuntimeFixture() {
		const HE::Refl::RuntimeTypeDescriptor& descriptor =
			HE::Refl::MakeStaticRuntimeTypeDescriptor<HE::RuntimeProviderFixture>(
				"RuntimeProviderFixture",
				"HE::RuntimeProviderFixture",
				"struct");

		Require(descriptor.Fields.size() == 2, "Expected static runtime descriptor fields");
		Require(descriptor.Fields[0].Name == "Name", "Expected static runtime Name field");
		Require(descriptor.Fields[0].Offset == offsetof(HE::RuntimeProviderFixture, Name), "Expected static runtime Name offset");
		Require(descriptor.Fields[1].Name == "Value", "Expected static runtime Value field");

		HE::RuntimeProviderFixture source;
		source.Name = "runtime";
		source.Value = 42;

		HE::Serialization::JsonSerializationBackend writeBackend;
		HE::Refl::SerializeRuntimeObject(descriptor, writeBackend, std::string(descriptor.Name), &source);
		const std::string json = writeBackend.SaveToString();
		Require(json.find("\"Name\"") != std::string::npos, "Expected static runtime serializer to emit Name");
		Require(json.find("\"Value\"") != std::string::npos, "Expected static runtime serializer to emit Value");

		HE::RuntimeProviderFixture loaded;
		HE::Serialization::JsonSerializationBackend readBackend;
		readBackend.LoadFromString(json);
		Require(
			HE::Refl::DeserializeRuntimeObject(descriptor, readBackend, std::string(descriptor.Name), &loaded),
			"Expected static runtime deserialize");
		Require(loaded.Name == "runtime", "Expected Name to round-trip");
		Require(loaded.Value == 42, "Expected Value to round-trip");
	}

	void VerifyProjectDescriptorRuntimeFields() {
		const std::string temporaryName = "ProjectDescriptor";
		const std::string temporaryQualifiedName = "HE::ProjectDescriptor";
		const std::string temporaryKind = "struct";
		const HE::Refl::RuntimeTypeDescriptor& descriptor =
			HE::Refl::MakeStaticRuntimeTypeDescriptor<HE::ProjectDescriptor>(
				temporaryName,
				temporaryQualifiedName,
				temporaryKind);

		Require(descriptor.Name == "ProjectDescriptor", "Expected ProjectDescriptor runtime name to stay stable");
		Require(descriptor.QualifiedName == "HE::ProjectDescriptor", "Expected ProjectDescriptor runtime qualified name to stay stable");
		Require(descriptor.Kind == "struct", "Expected ProjectDescriptor runtime kind to stay stable");
		Require(descriptor.Fields.size() == 3, "Expected ProjectDescriptor runtime fields");
		Require(descriptor.Fields[0].Name == "Name", "Expected ProjectDescriptor Name field");
		Require(descriptor.Fields[1].Name == "SchemaVersion", "Expected ProjectDescriptor SchemaVersion field");
		Require(descriptor.Fields[2].Name == "AssetDirectory", "Expected ProjectDescriptor AssetDirectory field");

		HE::ProjectDescriptor source;
		source.Name = "RuntimeProject";
		source.SchemaVersion = 7;
		source.AssetDirectory = "RuntimeAssets";

		HE::Serialization::JsonSerializationBackend writeBackend;
		HE::Refl::SerializeRuntimeObject(descriptor, writeBackend, std::string(descriptor.Name), &source);
		const std::string json = writeBackend.SaveToString();
		Require(json.find("\"SchemaVersion\"") != std::string::npos, "Expected ProjectDescriptor runtime serializer to emit SchemaVersion");
		Require(json.find("\"SceneDirectory\"") == std::string::npos, "Expected ProjectDescriptor runtime serializer to omit SceneDirectory");

		HE::ProjectDescriptor loaded;
		HE::Serialization::JsonSerializationBackend readBackend;
		readBackend.LoadFromString(json);
		Require(
			HE::Refl::DeserializeRuntimeObject(descriptor, readBackend, std::string(descriptor.Name), &loaded),
			"Expected ProjectDescriptor runtime deserialize");
		Require(loaded.Name == "RuntimeProject", "Expected ProjectDescriptor Name to round-trip");
		Require(loaded.SchemaVersion == 7, "Expected ProjectDescriptor SchemaVersion to round-trip");
		Require(loaded.AssetDirectory == "RuntimeAssets", "Expected ProjectDescriptor AssetDirectory to round-trip");
	}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	HE::Serialization::InitializeSerialization();

	VerifyStaticRuntimeFixture();
	VerifyProjectDescriptorRuntimeFields();

	std::cout << "ReflectionRuntimeProviderSmoke passed" << std::endl;
	return 0;
}
