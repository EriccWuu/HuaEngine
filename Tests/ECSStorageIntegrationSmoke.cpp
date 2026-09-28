#include "HuaEngine/ECS/Components.h"
#include <array>
#include <algorithm>
#include <map>
#include <optional>
#include "HuaEngine/Scene/Scene.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "Module/Rendering/RenderingComponent.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

namespace {
using HE::EntityId;
using HE::EntityUuid;
using HE::Ecs::EcsContext;
using HE::Ecs::Result;

void Require(bool condition, const std::string& message) {
	if (!condition) { std::cerr << "[ECSStorageIntegrationSmoke] " << message << '\n'; std::exit(1); }
}

template<typename T>
T Take(Result<T> result) {
	Require(result.HasValue(), result ? "" : result.GetError().Operation + ": " + result.GetError().Message);
	return std::move(result).Value();
}

void Check(Result<void> result) {
	Require(result.HasValue(), result ? "" : result.GetError().Operation + ": " + result.GetError().Message);
}

struct Value {
    int Number = 0;
    std::string Text;
    std::vector<int> Items;
    bool operator==(const Value&) const = default;
};
struct ModelEntity {
    std::string Name;
    std::optional<HE::TransformComponent> Transform;
    std::optional<Value> Data;
};
void VerifyDifferential(uint32_t seed) {
    EcsContext context;
    HE::Scene scene(context);
    auto& world = scene.GetWorld();
    const auto valueType = Take(context.Types().Register<Value>("StorageIntegration.Value"));
    const auto transformType = context.Types().Find<HE::TransformComponent>()->Id;
    std::map<std::pair<uint64_t, uint64_t>, ModelEntity> expected;
    uint64_t nextUuid = 1;
    std::mt19937 random(seed);
    auto key = [](EntityUuid uuid) { return std::pair{uuid.High, uuid.Low}; };
    auto create = [&](std::string name) {
        const EntityUuid uuid{0xd1ff000000000000ULL | seed, nextUuid++};
        const auto id = Take(scene.CreateEntity(name, uuid));
        expected.emplace(key(uuid), ModelEntity{name.empty() ? "Entity" : name, HE::TransformComponent{}, {}});
        Require(world.Has<HE::TransformComponent>(id), "Expected scene creation to add Transform");
        return uuid;
    };
    auto compare = [&](size_t step) {
        Require(world.EntityCount() == expected.size(), "Model entity count mismatch at step " + std::to_string(step));
        for (const auto& [identity, model] : expected) {
            const auto id = world.Find({identity.first, identity.second});
            Require(world.IsAlive(id) && world.Name(id) == model.Name,
                "Model identity/name mismatch at step " + std::to_string(step));
            const auto* transform = world.TryGet<HE::TransformComponent>(id);
            Require(bool(transform) == model.Transform.has_value(), "Model Transform presence mismatch");
            if (transform) Require(transform->Position == model.Transform->Position && transform->Rotation == model.Transform->Rotation &&
                transform->Scale == model.Transform->Scale, "Model Transform values mismatch");
            const auto* data = world.TryGet<Value>(id);
            Require(bool(data) == model.Data.has_value() && (!data || *data == *model.Data), "Model owned values mismatch");
        }
    };
    for (size_t index = 0; index < 240; ++index) {
        const auto uuid = create(index % 7 == 0 ? "" : "initial");
        Value value{int(index), "value-" + std::to_string(index), {1, 2, int(index)}};
        Check(world.Emplace<Value>(world.Find(uuid), value));
        expected.at(key(uuid)).Data = value;
    }
    compare(0);
    for (size_t step = 1; step <= 1200; ++step) {
        if (step % 307 == 0) { Check(world.Clear()); expected.clear(); compare(step); continue; }
        const auto operation = random() % 10;
        if (expected.empty() || operation == 0) { create("created-" + std::to_string(step)); compare(step); continue; }
        auto selected = expected.begin();
        std::advance(selected, random() % expected.size());
        const EntityUuid uuid{selected->first.first, selected->first.second};
        const auto id = world.Find(uuid);
        switch (operation) {
        case 1: case 9: {
            Value value{int(step), "replacement-" + std::to_string(step), {int(seed), int(step), -17}};
            Check(world.Emplace<Value>(id, value)); selected->second.Data = value; break;
        }
        case 2: {
            HE::TransformComponent transform;
            transform.Position = {float(step), -3, 5}; transform.Rotation = {11, 13, 17}; transform.Scale = {2, 3, 4};
            Check(world.Emplace<HE::TransformComponent>(id, transform)); selected->second.Transform = transform; break;
        }
        case 3: Check(world.Remove(id, valueType)); selected->second.Data.reset(); break;
        case 4: Check(world.Remove(id, transformType)); selected->second.Transform.reset(); break;
        case 5: {
            const auto name = step % 5 == 0 ? std::string{} : "renamed-" + std::to_string(step);
            Check(world.SetName(id, name)); selected->second.Name = name.empty() ? "Entity" : name; break;
        }
        case 6: Check(world.Destroy(id)); expected.erase(selected); break;
        case 7: {
            const EntityUuid clone{uuid.High, nextUuid++};
            (void)Take(world.Clone(id, "clone", clone));
            auto copied = selected->second; copied.Name = "clone"; expected.emplace(key(clone), std::move(copied)); break;
        }
        case 8:
            Require(Take(scene.CreateEntity("must not overwrite", uuid)) == id, "Expected duplicate UUID identity preservation"); break;
        }
        compare(step);
    }
    const auto source = create("clone collision source");
    const auto occupied = create("clone collision target");
    Require(!world.Clone(world.Find(source), "collision", occupied), "Expected clone UUID collision rejection");
    Check(world.Destroy({}));
    compare(1201);
    std::cout << "Independent value-model seed " << seed << ": 240 initial and 1200 mutations passed\n";
}

struct SceneValue {
	EntityUuid Uuid;
	std::string Name;
	std::array<std::string, 4> Components;
	bool operator==(const SceneValue&) const = default;
};

void SortSceneValues(std::vector<SceneValue>& values) {
	std::sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
		return std::tie(left.Uuid.High, left.Uuid.Low) < std::tie(right.Uuid.High, right.Uuid.Low);
	});
}

template<typename T>
std::string ComponentText(const T* value, const HE::Ecs::TypeRegistry& types) {
	return value ? HE::Serialization::ToJson(*value, types) : std::string{};
}

std::vector<SceneValue> SnapshotScene(HE::Scene& scene) {
    std::vector<SceneValue> values;
    const auto& world = scene.GetWorld();
    for (const auto id : world.Entities()) {
        values.push_back({world.Uuid(id), std::string(world.Name(id)), {
            ComponentText(world.TryGet<HE::TransformComponent>(id), world.Types()),
            ComponentText(world.TryGet<HE::Rendering::MeshComponent>(id), world.Types()),
            ComponentText(world.TryGet<HE::Rendering::MaterialComponent>(id), world.Types()),
            ComponentText(world.TryGet<HE::Rendering::CameraComponent>(id), world.Types())}});
    }
    SortSceneValues(values);
    return values;
}

std::vector<SceneValue> SnapshotRuntime(const HE::Ecs::World& world) {
	std::vector<SceneValue> values;
	for (const auto id : world.Entities()) {
		values.push_back({world.Uuid(id), std::string(world.Name(id)), {
			ComponentText(world.TryGet<HE::TransformComponent>(id), world.Types()),
			ComponentText(world.TryGet<HE::Rendering::MeshComponent>(id), world.Types()),
			ComponentText(world.TryGet<HE::Rendering::MaterialComponent>(id), world.Types()),
			ComponentText(world.TryGet<HE::Rendering::CameraComponent>(id), world.Types())}});
	}
	SortSceneValues(values);
	return values;
}

void VerifySceneStorageBridge() {
	HE::Serialization::InitializeSerialization();
	const auto fixturePath = std::filesystem::temp_directory_path() / "storage-v3.scene";
	const auto outputPath = std::filesystem::temp_directory_path() / "storage-v3-roundtrip.scene";
	{
		std::ofstream fixture(fixturePath);
		Require(fixture.good(), "Expected isolated scene fixture creation");
		fixture << R"(name: Storage Bridge
version: 3
entities:
  - uuid: 11223344556677889900112233445566
    name: Stored Entity
    components:
      TransformComponent:
        Position: {x: 1, y: 2, z: 3}
        Rotation: {x: 4, y: 5, z: 6}
        Scale: {x: 2, y: 3, z: 4}
      MeshComponent:
        Mesh: {guid: stored-mesh}
      MaterialComponent:
        Material: {guid: stored-material}
        Overrides:
          parameters:
            u_Color:
              type: vec4
              value: [1, 0.5, 0.25, 1]
          textures:
            u_Texture: {guid: stored-texture}
        BlendMode: Transparent
      CameraComponent:
        Primary: true
        FixedAspectRatio: true
        VerticalFovDegrees: 67
        NearClip: 0.25
        FarClip: 120
        AspectRatio: 1.5
)";
	}
	EcsContext context;
	HE::Scene loaded(context);
	Require(HE::Serialization::LoadScene(fixturePath.string(), loaded), "Expected the existing serializer to read a format 3 scene");
	HE::Ecs::World runtime(context);
	for (const auto source : loaded.GetWorld().Entities()) {
        const auto& sourceWorld = loaded.GetWorld();
        const auto id = Take(runtime.CreateEmpty(sourceWorld.Name(source), sourceWorld.Uuid(source)));
        Check(runtime.Emplace<HE::TransformComponent>(id, *sourceWorld.TryGet<HE::TransformComponent>(source)));
        Check(runtime.Emplace<HE::Rendering::MeshComponent>(id, *sourceWorld.TryGet<HE::Rendering::MeshComponent>(source)));
        Check(runtime.Emplace<HE::Rendering::MaterialComponent>(id, *sourceWorld.TryGet<HE::Rendering::MaterialComponent>(source)));
        Check(runtime.Emplace<HE::Rendering::CameraComponent>(id, *sourceWorld.TryGet<HE::Rendering::CameraComponent>(source)));
    }
	const auto original = SnapshotScene(loaded);
	Require(SnapshotRuntime(runtime) == original, "Expected transfer into real chunk storage to preserve every reflected field and resource reference");
	const auto id = runtime.Entities().front();
	Check(runtime.Remove(id, context.Types().Find<HE::Rendering::CameraComponent>()->Id));
	Check(runtime.Emplace<HE::Rendering::CameraComponent>(id,
		*loaded.GetWorld().TryGet<HE::Rendering::CameraComponent>(loaded.GetWorld().Find(original.front().Uuid))));
	Require(SnapshotRuntime(runtime) == original, "Expected layout migration to retain complete scene semantics");
	runtime.TryGet<HE::TransformComponent>(id)->Position.x = 99.0f;
	(void)Take(runtime.Clone(id, "Storage Clone", {0x1122334455667788ULL, 0x9900112233445567ULL}));
	const auto expected = SnapshotRuntime(runtime);

	// Exercise the same production serializer after runtime storage migration.
	HE::Scene saving(context, "Storage Bridge");
	for (const auto stored : runtime.Entities()) {
		const auto entity = Take(saving.CreateEntity(runtime.Name(stored), runtime.Uuid(stored)));
		Check(saving.GetWorld().Emplace<HE::TransformComponent>(entity, *runtime.TryGet<HE::TransformComponent>(stored)));
		Check(saving.GetWorld().Emplace<HE::Rendering::MeshComponent>(entity, *runtime.TryGet<HE::Rendering::MeshComponent>(stored)));
		Check(saving.GetWorld().Emplace<HE::Rendering::MaterialComponent>(entity, *runtime.TryGet<HE::Rendering::MaterialComponent>(stored)));
		Check(saving.GetWorld().Emplace<HE::Rendering::CameraComponent>(entity, *runtime.TryGet<HE::Rendering::CameraComponent>(stored)));
	}
	Require(HE::Serialization::SaveScene(saving, outputPath.string()), "Expected the scene storage bridge to save through the real serializer");
	std::ifstream saved(outputPath);
	const std::string text((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
	Require(text.find("version: 3") != std::string::npos && text.find("stored-mesh") != std::string::npos &&
		text.find("stored-material") != std::string::npos && text.find("stored-texture") != std::string::npos,
		"Expected stable version 3 representation and resource GUIDs");
	EcsContext destination;
	HE::Scene reloaded(destination);
	Require(HE::Serialization::LoadScene(outputPath.string(), reloaded), "Expected bridged format 3 scene reload in another Context");
	Require(SnapshotScene(reloaded) == expected, "Expected scene reload to preserve new storage UUIDs, names, components and resources");
	std::cout << "Format 3 serializer -> runtime storage -> scene serializer bridge passed\n";
}
}

int main() {
	HE::Log::Init({ .EnableConsoleOutput = false });
	VerifyDifferential(20260928);
	VerifyDifferential(0x5eed1234);
	VerifySceneStorageBridge();
	std::cout << "ECSStorageIntegrationSmoke passed\n";
	return 0;
}
