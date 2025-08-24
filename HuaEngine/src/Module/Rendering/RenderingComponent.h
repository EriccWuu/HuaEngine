#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/Shader/Shader.h"
#include "HuaEngine/Rendering/Texture.h"
#include "HuaEngine/Rendering/Material/Material.h"
#include "HuaEngine/Rendering/Mesh/Mesh.h"
#include "HuaEngine/Rendering/Mesh/MeshManager.h"
#include "HuaEngine/Reflection/Reflection.h"

namespace HE {
	struct CameraComponent : Component {
		CameraComponent() = default;
		CameraComponent(const Ref<Camera>& camera) 
			: Camera(camera) {}

		Ref<Camera> Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;
	};

	// 新的材质组件
	struct MaterialComponent : Component {
		MaterialComponent() = default;
		MaterialComponent(const Ref<MaterialInstance>& materialInstance)
			: MaterialInstance(materialInstance) {}

		Ref<MaterialInstance> MaterialInstance;
	};

	// 保留 RendererComponent 用于向后兼容（标记为已弃用）
	struct RendererComponent : Component {
		RendererComponent() = default;
		RendererComponent(const Ref<Shader>& shader, const Ref<Texture>& texture)
			: Shader(shader), Texture(texture) {}

		Ref<Shader> Shader;
		Ref<Texture> Texture;
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
		Ref<Mesh> GetMesh() const {
			if (!MeshAssetName.empty()) {
				return MeshManager::Instance().GetMesh(MeshAssetName);
			}
			return nullptr;
		}

		// 获取 VertexArray（延迟加载）
		Ref<VertexArray> GetVertexArray() {
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

srefl_class(HE::CameraComponent,
	fields(
		field(Primary),
		field(FixedAspectRatio)
	)
)

srefl_class(HE::MaterialComponent,
	fields(
		field(MaterialInstance)
	)
)

srefl_class(HE::MeshComponent,
	fields(
		field(MeshAssetName)
	)
)