#pragma once

#include "HuaEngine/Core/Core.h"
#include "HuaEngine/ECS/Components.h"
#include "HuaEngine/Rendering/Camera.h"
#include "HuaEngine/Rendering/VertexArray.h"
#include "HuaEngine/Rendering/Shader.h"
#include "HuaEngine/Rendering/Texture.h"
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