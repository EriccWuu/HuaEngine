#pragma once

#include "HuaEngine/Renderer/RendererAPI.h"

namespace HE {
	class OpenGLRendererAPI : public RendererAPI {
	public:
		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) override;
		virtual void SetClearColor(const glm::vec4& clearColor) override;
		virtual void Clear() override;
	};
}