#pragma once

#include "HuaEngine/Rendering/RHI/CommandList.h"
#include "HuaEngine/Rendering/RHI/RenderDevice.h"

namespace HE::Rendering {
	class Camera;
	class FrameBuffer;

	class OpenGLCommandList final : public CommandList {
	public:
		void BeginRenderTarget(FrameBuffer& target) override;
		void ClearColor(const glm::vec4& color) override;
		void BeginFrame(Camera& camera) override;
		void DrawIndexed(MaterialInstance& material, VertexArray& vertexArray, const glm::mat4& transform) override;
		void EndFrame() override;
		void EndRenderTarget() override;

	private:
		FrameBuffer* m_CurrentTarget = nullptr;
		Camera* m_CurrentCamera = nullptr;
	};

	class OpenGLRenderDevice final : public RenderDevice {
	public:
		OpenGLRenderDevice();

		CommandList& GetImmediateCommandList() override;

	private:
		OpenGLCommandList m_ImmediateCommandList;
	};
}
