#include "ECSTestSupport.h"
#include "HuaEngine/ECS/Runtime/WorldScope.h"
#include "HuaEngine/ECS/Runtime/EcsContext.h"
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "Module/Rendering/RenderingComponent.h"
#include "HuaEngine/Generated/GeneratedReflection.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace {
	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ECSContextSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	template<typename T>
	void VerifyTypedRegistration(HE::Ecs::TypeRegistry& registry, int firstOverload) {
		const auto canonical = HE::Ecs::ComponentTraits<T>::Describe();
		Require(canonical.Reflection != nullptr, "Expected generated traits to retain runtime reflection");
		const auto first = firstOverload == 0 ? registry.Register<T>() :
			(firstOverload == 1 ? registry.Register<T>(canonical.Guid, canonical.Name) : registry.Register<T>(canonical.Name));
		Require(first.HasValue(), "Expected generated typed registration through every public overload");
		const auto* registered = registry.Find(first.Value());
		const auto count = registry.All().size();
		const auto noArguments = registry.Register<T>();
		const auto explicitIdentity = registry.Register<T>(canonical.Guid, canonical.Name);
		const auto explicitName = registry.Register<T>(canonical.Name);
		Require(noArguments.HasValue() && explicitIdentity.HasValue() && explicitName.HasValue() &&
			noArguments.Value() == first.Value() && explicitIdentity.Value() == first.Value() && explicitName.Value() == first.Value(),
			"Expected no-argument, explicit Guid and explicit name registration to be idempotent");
		Require(registry.All().size() == count && registry.Find<T>() == registered &&
			registered->Descriptor.Reflection == canonical.Reflection && registered->Descriptor.QualifiedName == canonical.QualifiedName,
			"Expected every registration path to preserve the canonical descriptor and reflection address");
		const auto wrongGuid = registry.Register<T>(HE::Ecs::TypeGuid::FromName("Fixture.WrongGeneratedIdentity"), canonical.Name);
		const auto wrongName = registry.Register<T>(canonical.Guid, "Fixture.WrongGeneratedName");
		const auto wrongStableName = registry.Register<T>("Fixture.WrongGeneratedName");
		const auto wrongTag = registry.Register<T>(canonical.Guid, canonical.Name, true);
		for (const auto* result : { &wrongGuid, &wrongName, &wrongStableName, &wrongTag }) {
			Require(!result->HasValue() && result->GetError().Code == HE::Ecs::ErrorCode::InvalidType,
				"Expected explicit generated identity mismatch to return InvalidType");
		}
		Require(registry.All().size() == count && registry.Find<T>() == registered,
			"Expected invalid identity attempts to leave canonical registration unchanged");
	}

	void VerifyGeneratedRegistrationOrders() {
		for (int order = 0; order < 3; ++order) {
			HE::Ecs::EcsContext context;
			if (order == 1) {
				HE::Scene registrationScene(context);
			}
			if (order == 2) {
				Require(HE::Generated::RegisterGeneratedComponents(context.Types()).HasValue(), "Expected module registration before typed registration");
			}
			VerifyTypedRegistration<HE::TransformComponent>(context.Types(), order);
			VerifyTypedRegistration<HE::Rendering::MeshComponent>(context.Types(), (order + 1) % 3);
			VerifyTypedRegistration<HE::Rendering::MaterialComponent>(context.Types(), (order + 2) % 3);
			VerifyTypedRegistration<HE::Rendering::CameraComponent>(context.Types(), order);
			const auto count = context.Types().All().size();
			const bool hadRenderer = context.Types().Find<HE::Rendering::RendererComponent>() != nullptr;
			const auto* transform = context.Types().Find<HE::TransformComponent>();
			HE::Scene registrationScene(context);
			Require(HE::Generated::RegisterGeneratedComponents(context.Types()).HasValue(), "Expected module registration after typed registration");
			Require(context.Types().All().size() == count + (hadRenderer ? 0 : 1) && context.Types().Find<HE::TransformComponent>() == transform,
				"Expected World and module registration to preserve existing typed descriptors");
			VerifyTypedRegistration<HE::TransformComponent>(context.Types(), (order + 1) % 3);
		}
		HE::Ecs::EcsContext empty;
		const auto invalid = empty.Types().Register<HE::TransformComponent>(HE::Ecs::TypeGuid::FromName("Fixture.IncorrectTransform"), "TransformComponent");
		Require(!invalid.HasValue() && invalid.GetError().Code == HE::Ecs::ErrorCode::InvalidType && empty.Types().All().empty(),
			"Expected invalid explicit generated identity to fail before publishing any descriptor");

	}

	std::vector<const HE::Refl::RuntimeTypeDescriptor*> ComponentDescriptors() {
		std::vector<const HE::Refl::RuntimeTypeDescriptor*> descriptors;
		for (const auto& descriptor : HE::Refl::GetRuntimeTypes()) {
			if (descriptor.Kind != "component") continue;
			Require(descriptor.MakeEcsType != nullptr, "Expected reflected components to provide the shared ECS descriptor factory");
			descriptors.push_back(&descriptor);
		}
		Require(descriptors.size() >= 4, "Expected the existing reflected scene components");
		return descriptors;
	}

	void RegisterDescriptor(HE::Ecs::EcsContext& context, const HE::Refl::RuntimeTypeDescriptor& descriptor) {
		const auto result = context.Types().Register(descriptor.MakeEcsType());
		Require(result.HasValue(), "Expected reflected component registration in its Context");
	}

	void VerifyContextMetadata(HE::Ecs::EcsContext& context, HE::Ecs::World& world) {
        Require(&world.Context() == &context && &world.Types() == &context.Types(),
            "Expected World to borrow its supplied Context and registry");
        for (const auto* reflected : ComponentDescriptors()) {
            const auto descriptor = reflected->MakeEcsType();
            const auto* local = context.Types().Find(descriptor.Guid);
            Require(local && local == context.Types().FindByName(descriptor.Name) && local->Owner == &context.Types(),
                "Expected stable Guid and name to resolve a Context-owned descriptor");
            Require(local->Descriptor.Reflection == reflected,
                "Expected ECS registration and reflection to share canonical metadata");
        }
    }

	void VerifyEntity(HE::Scene& scene, HE::EntityUuid uuid, const glm::vec3& position) {
		auto entity = scene.GetWorld().Find(uuid);
		Require(scene.GetWorld().IsAlive(entity) && scene.GetWorld().Name(entity) == "Legacy Entity", "Expected entity UUID and name preservation");
		Require(ECSTestSupport::Get<HE::TransformComponent>(scene.GetWorld(), entity).Position == position, "Expected transform values from the correct Context");
		Require(ECSTestSupport::Get<HE::TransformComponent>(scene.GetWorld(), entity).Scale == glm::vec3(2.0f, 3.0f, 4.0f), "Expected transform scale preservation");
		Require(ECSTestSupport::Get<HE::Rendering::MeshComponent>(scene.GetWorld(), entity).Mesh.Reference.Guid == "fixture-mesh-guid",
			"Expected mesh resource reference preservation");
		const auto& material = ECSTestSupport::Get<HE::Rendering::MaterialComponent>(scene.GetWorld(), entity);
		Require(material.Material.Reference.Guid == "fixture-material-guid", "Expected material resource reference preservation");
		Require(material.BlendMode == HE::Rendering::MaterialBlendMode::Transparent, "Expected reflected enum preservation");
		const auto parameter = material.Overrides.Parameters.find("u_BaseColor");
		Require(parameter != material.Overrides.Parameters.end() && std::holds_alternative<glm::vec4>(parameter->second),
			"Expected material override payload preservation");
		Require(std::get<glm::vec4>(parameter->second) == glm::vec4(1.0f, 0.5f, 0.25f, 1.0f), "Expected material override values");
		const auto texture = material.Overrides.TextureParameters.find("u_Texture");
		Require(texture != material.Overrides.TextureParameters.end() && texture->second == "fixture-texture-guid",
			"Expected nested texture resource reference preservation");
		Require(ECSTestSupport::Get<HE::Rendering::CameraComponent>(scene.GetWorld(), entity).VerticalFovDegrees == 61.0f, "Expected camera values");
	}

	void EditPositionThroughReflection(HE::Scene& scene, HE::EntityUuid uuid, const glm::vec3& position) {
		auto& context = scene.GetWorld().Context();
		const auto* metadata = context.Types().Find<HE::TransformComponent>();
		Require(metadata != nullptr && metadata->Owner == &context.Types(),
			"Expected editor-style metadata lookup in the selected World's Context");
		const HE::Refl::RuntimeFieldDescriptor* positionField = nullptr;
		for (const auto& candidate : metadata->Descriptor.Reflection->Fields) {
			if (candidate.Name == "Position") positionField = &candidate;
		}
		Require(positionField != nullptr && HE::Refl::IsRuntimeFieldEditable(*positionField), "Expected editable reflected Position");
		auto entity = scene.GetWorld().Find(uuid);
		auto edit = ECSTestSupport::Take(HE::Ecs::WorldEditScope::Acquire(scene.GetWorld()));
		void* component = edit.Get().TryGet(entity, metadata->Id);
		Require(component != nullptr && HE::Refl::SetRuntimeFieldValue(*positionField, component, position),
			"Expected reflection editing to use the component resolved through the selected World");
	}
}

int main() {
	HE::Serialization::InitializeSerialization();
	VerifyGeneratedRegistrationOrders();
	HE::Ecs::EcsContext forward;
	HE::Ecs::EcsContext reverse;
	const auto descriptors = ComponentDescriptors();
	for (const auto* descriptor : descriptors) RegisterDescriptor(forward, *descriptor);
	for (auto iterator = descriptors.rbegin(); iterator != descriptors.rend(); ++iterator) RegisterDescriptor(reverse, **iterator);
	bool observedDifferentLocalIds = false;
	for (const auto* descriptor : descriptors) {
		const auto stable = descriptor->MakeEcsType();
		const auto* first = forward.Types().Find(stable.Guid);
		const auto* second = reverse.Types().Find(stable.Guid);
		Require(first != nullptr && second != nullptr && first->Descriptor.Name == second->Descriptor.Name,
			"Expected stable Guid and TypeName identity across registration orders");
		Require(first != second && !reverse.Types().Owns(*first), "Expected Context ownership of registered handles");
		if (first->Id != second->Id) {
			observedDifferentLocalIds = true;
			Require(reverse.Types().Find(first->Id)->Descriptor.Guid != first->Descriptor.Guid,
				"Expected equal local numeric IDs to be allowed to denote different Context types");
		}
	}
	Require(observedDifferentLocalIds, "Expected the test to exercise different local registration orders");

	HE::Scene firstScene(forward, "Forward Context");
	HE::Scene secondScene(reverse, "Reverse Context");
	HE::Ecs::World additionalWorld(forward);
	VerifyContextMetadata(forward, firstScene.GetWorld());
	VerifyContextMetadata(reverse, secondScene.GetWorld());
	Require(&additionalWorld.Types() == &firstScene.GetWorld().Types(),
		"Expected multiple Worlds in one Context to share one TypeRegistry");

	const auto fixturePath = std::filesystem::temp_directory_path() / "context-v3.scene";
	const auto savedPath = std::filesystem::temp_directory_path() / "context-edited.scene";
	{
		std::ofstream fixture(fixturePath);
		Require(fixture.good(), "Expected isolated legacy scene fixture creation");
		fixture << R"(name: Context Fixture
version: 3
entities:
  - uuid: 18e04a19e5df4e9dbe3986e5e5294091
    name: Legacy Entity
    components:
      TransformComponent:
        Position: {x: 1, y: 2, z: 3}
        Rotation: {x: 0, y: 0, z: 0}
        Scale: {x: 2, y: 3, z: 4}
      MeshComponent:
        Mesh: {guid: fixture-mesh-guid}
      MaterialComponent:
        Material: {guid: fixture-material-guid}
        Overrides:
          parameters:
            u_BaseColor:
              type: vec4
              value: [1, 0.5, 0.25, 1]
          textures:
            u_Texture: {guid: fixture-texture-guid}
        BlendMode: Transparent
      CameraComponent:
        Primary: true
        FixedAspectRatio: false
        VerticalFovDegrees: 61
        NearClip: 0.1
        FarClip: 100
        AspectRatio: 1.5
)";
	}
	const auto uuid = HE::EntityUuid::FromString("18e04a19e5df4e9dbe3986e5e5294091");
	Require(HE::Serialization::LoadScene(fixturePath.string(), firstScene), "Expected legacy scene load in forward Context");
	Require(HE::Serialization::LoadScene(fixturePath.string(), secondScene), "Expected legacy scene load in reverse Context");
	VerifyEntity(firstScene, uuid, {1.0f, 2.0f, 3.0f});
	VerifyEntity(secondScene, uuid, {1.0f, 2.0f, 3.0f});
	EditPositionThroughReflection(secondScene, uuid, {17.0f, 19.0f, 23.0f});
	VerifyEntity(firstScene, uuid, {1.0f, 2.0f, 3.0f});
	Require(HE::Serialization::SaveScene(secondScene, savedPath.string()), "Expected edited scene save through its Context");
	std::ifstream saved(savedPath);
	const std::string text((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
	Require(text.find("version: 3") != std::string::npos && text.find("TransformComponent:") != std::string::npos &&
		text.find("fixture-texture-guid") != std::string::npos, "Expected unchanged version 3 names and resource representation");
	HE::Scene reloaded(forward);
	Require(HE::Serialization::LoadScene(savedPath.string(), reloaded), "Expected edited scene reload in the opposite Context");
	VerifyEntity(reloaded, uuid, {17.0f, 19.0f, 23.0f});
	Require(reloaded.GetName() == "Context Fixture" && reloaded.GetWorld().EntityCount() == 1,
		"Expected scene identity and entity count preservation");
	std::cout << "ECSContextSmoke passed: opposite Context registration orders preserve version 3 scenes" << std::endl;
	return 0;
}
