#pragma once
#include "RendererAPI.h"

namespace HE {
	class RenderCommand {
	public:
		static void Clear();
		static void SetClearColor(const glm::vec4& clearColor);
		static void DrawIndexed(const std::shared_ptr<VertexArray>& vaertexArray);

	private:
		static RendererAPI* m_RendererAPI;
	};
}