#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/Shader/Shader.h"
#include "HuaEngine/Rendering/Texture.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/Mesh.h"
#include "HuaEngine/Reflection/Reflection.h"
#include "HuaEngine/Reflection/ReflectionMarkers.h"

namespace HE::Rendering {
	HE_REFLECT_ENUM(DisplayName="Material Blend Mode")
	enum class MaterialBlendMode {
		Opaque,
		Masked,
		Transparent
	};

	HE_REFLECT_COMPONENT(DisplayName="Camera", Category="Rendering")
	struct CameraComponent : Component {
		CameraComponent() = default;
		CameraComponent(const Ref<HE::Rendering::Camera>& camera) 
			: Camera(camera) {}

		Ref<HE::Rendering::Camera> Camera;
		HE_REFLECT_FIELD()
		bool Primary = true;
		HE_REFLECT_FIELD()
		bool FixedAspectRatio = false;
	};

	// Material component
	HE_REFLECT_COMPONENT(DisplayName="Material", Category="Rendering")
	struct MaterialComponent : Component {
		MaterialComponent() = default;
		MaterialComponent(const Ref<HE::Rendering::MaterialInstance>& materialInstance)
			: MaterialInstance(materialInstance) {}

		HE_REFLECT_FIELD()
		Ref<HE::Rendering::MaterialInstance> MaterialInstance;
		HE_REFLECT_FIELD()
		MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
	};

	// Legacy RendererComponent for backward compatibility (deprecated)
	struct RendererComponent : Component {
		RendererComponent() = default;
		RendererComponent(const Ref<HE::Rendering::Shader>& shader, const Ref<HE::Rendering::Texture>& texture)
			: Shader(shader), Texture(texture) {}

		Ref<HE::Rendering::Shader> Shader;
		Ref<HE::Rendering::Texture> Texture;
	};

	HE_REFLECT_COMPONENT(DisplayName="Mesh", Category="Rendering")
	struct MeshComponent : Component {
		MeshComponent() = default;
		MeshComponent(const std::string& meshName)
			: MeshAssetName(meshName) {}
		MeshComponent(const Ref<VertexArray>& vertexArray)
			: m_CachedVertexArray(vertexArray) {}

		HE_REFLECT_FIELD()
		std::string MeshAssetName;             // Mesh asset name (for serialization)
		Ref<VertexArray> m_CachedVertexArray;  // Runtime cached VertexArray (not serialized)

		// Get mesh asset
		Ref<HE::Rendering::Mesh> GetMesh() const {
			if (!MeshAssetName.empty()) {
				return HE::Rendering::MeshManager::Instance().GetMesh(MeshAssetName);
			}
			return nullptr;
		}

		// Get VertexArray (lazy loading)
		Ref<HE::Rendering::VertexArray> GetVertexArray() {
			// Return cached VertexArray if available
			if (m_CachedVertexArray) {
				return m_CachedVertexArray;
			}

			// Try to get from asset manager
			auto mesh = GetMesh();
			if (mesh) {
				m_CachedVertexArray = mesh->GetVertexArray();
				return m_CachedVertexArray;
			}

			return nullptr;
		}

		// Set mesh asset by name
		void SetMesh(const std::string& meshName) {
			MeshAssetName = meshName;
			m_CachedVertexArray = nullptr;  // Clear cache, reload on next access
		}

		// Set mesh asset (using Mesh object directly)
		void SetMesh(Ref<Mesh> mesh) {
			if (mesh) {
				MeshAssetName = mesh->GetName();
				m_CachedVertexArray = mesh->GetVertexArray();
			}
		}

		// Check if mesh is valid
		bool HasValidMesh() const {
			return !MeshAssetName.empty() && GetMesh() != nullptr;
		}
	};
}
