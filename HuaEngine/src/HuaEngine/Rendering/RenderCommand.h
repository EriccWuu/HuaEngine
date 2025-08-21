#pragma once
#include "RendererAPI.h"

namespace HE {
	class RenderCommand {
	public:
		static void Init();
		static void Clear();
		static void SetClearColor(const glm::vec4& clearColor);
		static void SetViewport(const float width, const float height);
		static void DrawIndexed(const Ref<VertexArray>& vaertexArray);

	private:
		static RendererAPI* m_RendererAPI;
	};
}