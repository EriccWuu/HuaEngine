#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/Shader/Shader.h"
#include "HuaEngine/Rendering/Texture.h"
#include "HuaEngine/Rendering/Material/Material.h"
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
		MeshComponent(const Ref<VertexArray>& vertexArray)
			: VertexArray(vertexArray) {}

		Ref<VertexArray> VertexArray;
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
		// MaterialInstance 是 Ref 类型，暂时不序列化
		// 后续可以添加材质名称字符串用于序列化
	)
)