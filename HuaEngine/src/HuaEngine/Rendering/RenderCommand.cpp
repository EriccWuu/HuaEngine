#include "enginepch.h"
#include "RenderCommand.h"

namespace HE::Rendering {
	RendererAPI* RenderCommand::m_RendererAPI = RendererAPI::Create();

	void RenderCommand::Init() {
		m_RendererAPI->Init();
	}

	void RenderCommand::Clear() {
		m_RendererAPI->Clear();
	}

	void RenderCommand::SetClearColor(const glm::vec4& clearColor) {
		m_RendererAPI->SetClearColor(clearColor);
	}

	void RenderCommand::SetViewport(const float width, const float height) {
		m_RendererAPI->SetViewport(width, height);
	}

	void RenderCommand::DrawIndexed(const Ref<VertexArray>& vertexArray) {
		m_RendererAPI->DrawIndexed(vertexArray);
	}

}