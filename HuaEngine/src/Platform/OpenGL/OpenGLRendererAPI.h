#pragma once

#include "HuaEngine/Renderer/RendererAPI.h"

namespace HE {
	class OpenGLRendererAPI : public RendererAPI {
	public:
		virtual void Init() override;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) override;
		virtual void SetClearColor(const glm::vec4& clearColor) override;
		virtual void Clear() override;
	};
}