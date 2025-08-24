#pragma once

#include "HuaEngine/Math/Math.h"
#include "HuaEngine/Serialization/Serialization.h"
#include "HuaEngine/Scene/SceneSerializer.h"
#include "HuaEngine/ECS/Components.h"
#include <iostream>

namespace HE {
    struct PlayerComponent {
        TransformComponent transform = TransformComponent();
        std::string name = "Player";
        int level = 1;
        float health = 100.0f;
        glm::vec3 spawnPoint = { 0.0f, 0.0f, 0.0f };
        std::vector<std::string> inventory;
    };
}

srefl_class(PlayerComponent,
    fields(
        field(transform),
        field(name),
        field(level),
        field(health),
        field(spawnPoint),
        field(inventory)
    )
)


namespace HE::Test {

    // Test usage of the serialization system
    void SerializationTest() {
        // Initialize serialization system
        InitializeSerialization();

        // Test 1: Serialize a simple component
        {
            PlayerComponent player;
            player.name = "Hero";
            player.level = 10;
            player.health = 85.5f;
            player.spawnPoint = {10.0f, 20.0f, 30.0f};
            player.inventory = {"sword", "shield", "potion"};

            // Serialize to JSON string
            std::string jsonStr = ToJson(player);
            std::cout << "Serialized Player Component:\n" << jsonStr << std::endl;

            // Deserialize from JSON string
            PlayerComponent loadedPlayer;
            if (FromJson(jsonStr, loadedPlayer)) {
                std::cout << "Successfully deserialized player: " << loadedPlayer.name 
                         << " Level: " << loadedPlayer.level << std::endl;
            }

            // Save to file
            SaveAsJson(player, "player.json");

            // Load from file
            PlayerComponent filePlayer;
            if (LoadFromJson("player.json", filePlayer)) {
                std::cout << "Loaded player from file: " << filePlayer.name << std::endl;
                std::cout << "Loaded player from file: " << filePlayer.transform.Position << std::endl;
            }
        }

        // Test 2: Serialize a transform component
        {
            TransformComponent transform;
            transform.Position = {1.0f, 2.0f, 3.0f};
            transform.Rotation = {0.0f, 45.0f, 0.0f};
            transform.Scale = {2.0f, 2.0f, 2.0f};

            std::string transformJson = ToJson(transform);
            std::cout << "Transform Component JSON:\n" << transformJson << std::endl;
        }

        // Test 3: Serialize a vector of components
        {
            std::vector<TransformComponent> transforms;
            transforms.resize(3);
            
            transforms[0].Position = {1.0f, 0.0f, 0.0f};
            transforms[1].Position = {0.0f, 1.0f, 0.0f};
            transforms[2].Position = {0.0f, 0.0f, 1.0f};

            std::string transformsJson = ToJson(transforms);
            std::cout << "Transform Array JSON:\n" << transformsJson << std::endl;

            // Deserialize back
            std::vector<TransformComponent> loadedTransforms;
            if (FromJson(transformsJson, loadedTransforms)) {
                std::cout << "Loaded " << loadedTransforms.size() << " transforms" << std::endl;
            }
        }

        // Test 4: Scene serialization
        {
            Scene scene;
            auto& entityManager = scene.GetEntityManager();
            auto& registry = entityManager.GetRegistry();

            // Create some entities with components
            for (int i = 0; i < 3; ++i) {
                auto entity = registry.create();
                
                TransformComponent transform;
                transform.Position = {static_cast<float>(i), 0.0f, 0.0f};
                registry.emplace<TransformComponent>(entity, transform);
            }

            // Save the scene
            if (SaveScene(&scene, "test_scene.json")) {
                std::cout << "Scene saved successfully!" << std::endl;
            }

            // Load the scene
            Scene loadedScene;
            if (LoadScene(&loadedScene, "test_scene.json")) {
                std::cout << "Scene loaded successfully!" << std::endl;
                
                // Count entities in loaded scene
                auto& loadedRegistry = loadedScene.GetEntityManager().GetRegistry();
                size_t entityCount = 0;
                for (auto entity : loadedRegistry.storage<entt::entity>()) {
                    ++entityCount;
                }
                std::cout << "Loaded scene has " << entityCount << " entities" << std::endl;
            }
        }
    }

    // Test case for extending serialization for custom types
    void CustomSerializationTest() {
        // This would be implemented in a separate file where PlayerComponent is defined
        
        /*
        // Custom serializer for PlayerComponent
        template<>
        struct Serializer<PlayerComponent> {
            static void Serialize(Serialization::SerializationBackend& backend, const std::string& name, const PlayerComponent& obj) {
                backend.BeginObject(name);
                backend.Serialize("name", obj.name);
                backend.Serialize("level", obj.level);
                backend.Serialize("health", obj.health);
                Serialization::SerializeValue(backend, "spawnPoint", obj.spawnPoint);
                Serialization::SerializeArray(backend, "inventory", obj.inventory);
                backend.EndObject();
            }

            static bool Deserialize(Serialization::SerializationBackend& backend, const std::string& name, PlayerComponent& obj) {
                if (!backend.HasField(name)) return false;
                
                backend.BeginObject(name);
                backend.Deserialize("name", obj.name);
                backend.Deserialize("level", obj.level);
                backend.Deserialize("health", obj.health);
                Serialization::DeserializeValue(backend, "spawnPoint", obj.spawnPoint);
                Serialization::DeserializeArray(backend, "inventory", obj.inventory);
                backend.EndObject();
                return true;
            }
        };
        */
    }

}
