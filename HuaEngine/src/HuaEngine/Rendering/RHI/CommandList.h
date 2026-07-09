#pragma once

#include <cstdint>
#include "HuaEngine/Rendering/RHI/FrameObjectBinding.h"
#include "glm/glm.hpp"

namespace HE::Rendering {
	class Camera;
	class MaterialBinding;
	class PipelineState;
	class RenderTarget;
	class ShaderProgram;
	class VertexBufferView;

	class CommandList {
	public:
		virtual ~CommandList() = default;

		virtual void BeginRenderTarget(RenderTarget& target) = 0;
		virtual void ClearColor(const glm::vec4& color) = 0;
		virtual void BeginFrame(Camera& camera) = 0;

		// Compatibility path for the OpenGL migration period. Prefer SetPipelineState for draw submission.
		virtual void SetShaderProgram(ShaderProgram& shaderProgram) = 0;
		// Normal render path pipeline state submission.
		virtual void SetPipelineState(PipelineState& pipelineState) = 0;
		// Normal render path state submission.
		virtual void SetVertexBufferView(VertexBufferView& vertexBufferView) = 0;
		// Normal render path frame/view parameter submission.
		virtual void SetFrameBinding(const FrameBinding& binding) = 0;
		// Normal render path state submission.
		virtual void SetMaterialBinding(const MaterialBinding& binding) = 0;
		// Normal render path per-object parameter submission.
		virtual void SetObjectBinding(const ObjectBinding& binding) = 0;
		// Normal render path draw. Frame and object bindings must be submitted first.
		virtual void DrawIndexed(uint32_t indexCount) = 0;

		virtual void EndFrame() = 0;
		virtual void EndRenderTarget() = 0;
	};
}
