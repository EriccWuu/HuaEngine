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

namespace HE::Rendering {
	struct CameraComponent : Component {
		CameraComponent() = default;
		CameraComponent(const Ref<HE::Rendering::Camera>& camera) 
			: Camera(camera) {}

		Ref<HE::Rendering::Camera> Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;
	};

	// 新的材质组件
	struct MaterialComponent : Component {
		MaterialComponent() = default;
		MaterialComponent(const Ref<HE::Rendering::MaterialInstance>& materialInstance)
			: MaterialInstance(materialInstance) {}

		Ref<HE::Rendering::MaterialInstance> MaterialInstance;
	};

	// 保留 RendererComponent 用于向后兼容（标记为已弃用）
	struct RendererComponent : Component {
		RendererComponent() = default;
		RendererComponent(const Ref<HE::Rendering::Shader>& shader, const Ref<HE::Rendering::Texture>& texture)
			: Shader(shader), Texture(texture) {}

		Ref<HE::Rendering::Shader> Shader;
		Ref<HE::Rendering::Texture> Texture;
	};

	struct MeshComponent : Component {
		MeshComponent() = default;
		MeshComponent(const std::string& meshName)
			: MeshAssetName(meshName) {}
		MeshComponent(const Ref<VertexArray>& vertexArray)
			: m_CachedVertexArray(vertexArray) {}

		std::string MeshAssetName;     // 网格资产名称（用于序列化）
		Ref<VertexArray> m_CachedVertexArray;  // 运行时缓存的 VertexArray（不序列化）

		// 获取网格资产
		Ref<HE::Rendering::Mesh> GetMesh() const {
			if (!MeshAssetName.empty()) {
				return HE::Rendering::MeshManager::Instance().GetMesh(MeshAssetName);
			}
			return nullptr;
		}

		// 获取 VertexArray（延迟加载）
		Ref<HE::Rendering::VertexArray> GetVertexArray() {
			// 如果已经有缓存的 VertexArray，直接返回
			if (m_CachedVertexArray) {
				return m_CachedVertexArray;
			}

			// 尝试从资产管理器获取
			auto mesh = GetMesh();
			if (mesh) {
				m_CachedVertexArray = mesh->GetVertexArray();
				return m_CachedVertexArray;
			}

			return nullptr;
		}

		// 设置网格资产
		void SetMesh(const std::string& meshName) {
			MeshAssetName = meshName;
			m_CachedVertexArray = nullptr;  // 清除缓存，下次访问时重新加载
		}

		// 设置网格资产（直接使用 Mesh 对象）
		void SetMesh(Ref<Mesh> mesh) {
			if (mesh) {
				MeshAssetName = mesh->GetName();
				m_CachedVertexArray = mesh->GetVertexArray();
			}
		}

		// 检查是否有有效的网格
		bool HasValidMesh() const {
			return !MeshAssetName.empty() && GetMesh() != nullptr;
		}
	};
}

srefl_class(HE::Rendering::CameraComponent,
	fields(
		field(Primary),
		field(FixedAspectRatio)
	)
)

srefl_class(HE::Rendering::MaterialComponent,
	fields(
		field(MaterialInstance)
	)
)

srefl_class(HE::Rendering::MeshComponent,
	fields(
		field(MeshAssetName)
	)
)