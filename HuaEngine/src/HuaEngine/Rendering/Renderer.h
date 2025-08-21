#pragma once

#include "RendererAPI.h"
#include "Shader.h"
#include "Camera.h"

namespace HE {

	class Renderer {
	public:
		static void Init();
		static void Begin(Ref<Camera> camera);
		static void End();
		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform);

	private:
		static Ref<Camera> m_Camera;
	};
}