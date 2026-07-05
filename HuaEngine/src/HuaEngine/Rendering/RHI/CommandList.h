#pragma once

#include "glm/glm.hpp"

namespace HE::Rendering {
	class Camera;
	class FrameBuffer;
	class MaterialInstance;
	class VertexArray;

	class CommandList {
	public:
		virtual ~CommandList() = default;

		virtual void BeginRenderTarget(FrameBuffer& target) = 0;
		virtual void ClearColor(const glm::vec4& color) = 0;
		virtual void BeginFrame(Camera& camera) = 0;
		virtual void DrawIndexed(MaterialInstance& material, VertexArray& vertexArray, const glm::mat4& transform) = 0;
		virtual void EndFrame() = 0;
		virtual void EndRenderTarget() = 0;
	};
}
