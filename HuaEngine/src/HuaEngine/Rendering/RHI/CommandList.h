#pragma once

#include <cstdint>
#include <string>

#include "HuaEngine/Rendering/RHI/FrameObjectBinding.h"
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

		// Normal render path state submission.
		virtual void SetShaderProgram(ShaderProgram& shaderProgram) = 0;
		// Normal render path state submission.
		virtual void SetVertexBufferView(VertexBufferView& vertexBufferView) = 0;
		// Normal render path frame/view parameter submission.
		virtual void SetFrameBinding(const FrameBinding& binding) = 0;
		// Temporary helper until frame/object/material parameter sets are introduced.
		virtual void SetTexture(uint32_t slot, TextureResource& texture) = 0;
		// Temporary helper until frame/object/material parameter sets are introduced.
		virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
		// Normal render path state submission.
		virtual void SetMaterialBinding(const MaterialBinding& binding) = 0;
		// Normal render path per-object parameter submission.
		virtual void SetObjectBinding(const ObjectBinding& binding) = 0;
		// Normal render path draw. Frame and object bindings must be submitted first.
		virtual void DrawIndexed(uint32_t indexCount) = 0;
		// Compatibility helper until normal render paths submit ObjectBinding explicitly.
		virtual void DrawIndexed(uint32_t indexCount, const glm::mat4& transform) = 0;

		// Legacy fallback for old MaterialInstance and VertexArray shells.
		// Remove after all render paths provide ShaderProgramRef, VertexBufferViewRef,
		// and MaterialBindingRef.
		virtual void DrawIndexed(MaterialInstance& material, VertexArray& vertexArray, const glm::mat4& transform) = 0;

		virtual void EndFrame() = 0;
		virtual void EndRenderTarget() = 0;
	};
}
