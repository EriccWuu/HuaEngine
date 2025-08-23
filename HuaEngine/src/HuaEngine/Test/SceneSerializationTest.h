#pragma once

#include "HuaEngine/Serialization/SceneSerializer.h"
#include "HuaEngine/Serialization/SerializationCore.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Scene/Scene.h"

namespace HE {

    // Create a simple scene to test serialization functionality
    class SceneSerializationTest {
    public:
        static void RunTest() {
            HE_CORE_INFO("========== Scene Serialization Test ==========");

            // Create scene
            Scene scene;
            auto& entityManager = scene.GetEntityManager();
            auto& registry = entityManager.GetRegistry();

            // Create entity1 - Transform component
            auto entity1 = registry.create();
            TransformComponent transform1;
            transform1.Position = {1.0f, 2.0f, 3.0f};
            transform1.Rotation = {0.0f, 45.0f, 0.0f};
            transform1.Scale = {2.0f, 1.5f, 1.0f};
            registry.emplace<TransformComponent>(entity1, transform1);

            // Create entity2 - Different Transform
            auto entity2 = registry.create();
            TransformComponent transform2;
            transform2.Position = {-5.0f, 0.0f, 10.0f};
            transform2.Rotation = {90.0f, 0.0f, 0.0f};
            transform2.Scale = {0.5f, 0.5f, 0.5f};
            registry.emplace<TransformComponent>(entity2, transform2);

            // Create entity3 - Default Transform
            auto entity3 = registry.create();
            TransformComponent transform3; // Use default values
            registry.emplace<TransformComponent>(entity3, transform3);

            HE_CORE_INFO("Created scene with 3 entities:");
            HE_CORE_INFO("- Entity 1: Position({}, {}, {}), Rotation({}, {}, {}), Scale({}, {}, {})", 
                transform1.Position.x, transform1.Position.y, transform1.Position.z,
                transform1.Rotation.x, transform1.Rotation.y, transform1.Rotation.z,
                transform1.Scale.x, transform1.Scale.y, transform1.Scale.z);

            // Serialize scene
            SceneSerializer serializer(&scene);
            std::string filename = "test_scene.json";
            
            HE_CORE_INFO("Serializing scene to '{}'...", filename);
            if (serializer.SerializeScene(filename, SerializationFormat::JSON)) {
                HE_CORE_INFO("✅ Scene serialization successful!");
            } else {
                HE_CORE_ERROR("❌ Scene serialization failed!");
                return;
            }

            // Create new scene and deserialize
            Scene loadedScene;
            SceneSerializer deserializer(&loadedScene);
            
            HE_CORE_INFO("Deserializing scene from '{}'...", filename);
            if (deserializer.DeserializeScene(filename, SerializationFormat::JSON)) {
                HE_CORE_INFO("✅ Scene deserialization successful!");
                
                // Verify loaded data
                auto& loadedRegistry = loadedScene.GetEntityManager().GetRegistry();
                uint32_t entityCount = 0;
                uint32_t transformCount = 0;
                
                for (auto entity : loadedRegistry.storage<entt::entity>()) {
                    entityCount++;
                    if (loadedRegistry.all_of<TransformComponent>(entity)) {
                        transformCount++;
                        auto& transform = loadedRegistry.get<TransformComponent>(entity);
                        HE_CORE_INFO("- Loaded Entity: Position({}, {}, {}), Rotation({}, {}, {}), Scale({}, {}, {})", 
                            transform.Position.x, transform.Position.y, transform.Position.z,
                            transform.Rotation.x, transform.Rotation.y, transform.Rotation.z,
                            transform.Scale.x, transform.Scale.y, transform.Scale.z);
                    }
                }
                
                HE_CORE_INFO("Loaded scene contains {} entities with {} transform components", 
                    entityCount, transformCount);
            } else {
                HE_CORE_ERROR("❌ Scene deserialization failed!");
                return;
            }

            HE_CORE_INFO("========== Test Complete ==========");
        }

        // Test single component serialization
        static void RunComponentTest() {
            HE_CORE_INFO("========== Component Serialization Test ==========");

            // Create a Transform component
            TransformComponent originalTransform;
            originalTransform.Position = {10.0f, 20.0f, 30.0f};
            originalTransform.Rotation = {45.0f, 90.0f, 0.0f};
            originalTransform.Scale = {2.0f, 3.0f, 1.5f};

            HE_CORE_INFO("Original Transform:");
            HE_CORE_INFO("- Position: ({}, {}, {})", 
                originalTransform.Position.x, originalTransform.Position.y, originalTransform.Position.z);
            HE_CORE_INFO("- Rotation: ({}, {}, {})", 
                originalTransform.Rotation.x, originalTransform.Rotation.y, originalTransform.Rotation.z);
            HE_CORE_INFO("- Scale: ({}, {}, {})", 
                originalTransform.Scale.x, originalTransform.Scale.y, originalTransform.Scale.z);

            // Serialize to JSON string
            auto backend = SerializationManager::Instance().CreateBackend(SerializationFormat::JSON);
            backend->Reset();
            backend->BeginObject();
            
            Serializer<TransformComponent>::Serialize(*backend, "transform", originalTransform);
            
            backend->EndObject();
            std::string jsonString = backend->ToString();
            
            HE_CORE_INFO("Serialized JSON:");
            HE_CORE_INFO("{}", jsonString);

            // Deserialize
            auto loadBackend = SerializationManager::Instance().CreateBackend(SerializationFormat::JSON);
            loadBackend->FromString(jsonString);
            
            TransformComponent loadedTransform;
            if (Serializer<TransformComponent>::Deserialize(*loadBackend, "transform", loadedTransform)) {
                HE_CORE_INFO("✅ Component deserialization successful!");
                HE_CORE_INFO("Loaded Transform:");
                HE_CORE_INFO("- Position: ({}, {}, {})", 
                    loadedTransform.Position.x, loadedTransform.Position.y, loadedTransform.Position.z);
                HE_CORE_INFO("- Rotation: ({}, {}, {})", 
                    loadedTransform.Rotation.x, loadedTransform.Rotation.y, loadedTransform.Rotation.z);
                HE_CORE_INFO("- Scale: ({}, {}, {})", 
                    loadedTransform.Scale.x, loadedTransform.Scale.y, loadedTransform.Scale.z);
            } else {
                HE_CORE_ERROR("❌ Component deserialization failed!");
            }

            HE_CORE_INFO("========== Component Test Complete ==========");
        }
    };

}
