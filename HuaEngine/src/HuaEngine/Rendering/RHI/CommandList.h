#pragma once

#include <cstdint>
#include <string>

#include "glm/glm.hpp"

namespace HE::Rendering {
	class Camera;
	class FrameBuffer;
	class MaterialBinding;
	class MaterialInstance;
	class RenderTarget;
	class ShaderProgram;
	class TextureResource;
	class VertexArray;
	class VertexBufferView;

	class CommandList {
	public:
		virtual ~CommandList() = default;

		virtual void BeginRenderTarget(FrameBuffer& target) = 0;
		virtual void BeginRenderTarget(RenderTarget& target) = 0;
		virtual void ClearColor(const glm::vec4& color) = 0;
		virtual void BeginFrame(Camera& camera) = 0;

		virtual void SetShaderProgram(ShaderProgram& shaderProgram) = 0;
		virtual void SetVertexBufferView(VertexBufferView& vertexBufferView) = 0;
		virtual void SetTexture(uint32_t slot, TextureResource& texture) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
		virtual void SetMaterialBinding(const MaterialBinding& binding) = 0;
		virtual void DrawIndexed(uint32_t indexCount, const glm::mat4& transform) = 0;

		// Compatibility path for legacy MaterialInstance and VertexArray shells.
		virtual void DrawIndexed(MaterialInstance& material, VertexArray& vertexArray, const glm::mat4& transform) = 0;

		virtual void EndFrame() = 0;
		virtual void EndRenderTarget() = 0;
	};
}
