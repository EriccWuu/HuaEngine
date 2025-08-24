#pragma once

#include "RendererAPI.h"
#include "Shader/Shader.h"
#include "Camera.h"
#include "Material/Material.h"

namespace HE::Rendering {

	class Renderer {
	public:
		static void Init();
		static void Begin(Ref<Camera> camera);
		static void End();
		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform);
		static void Submit(const Ref<MaterialInstance>& material, const Ref<VertexArray>& vertexArray, const glm::mat4& transform);

	private:
		static Ref<Camera> m_Camera;
	};
}