#include "enginepch.h"
#include "RenderCommand.h"

namespace HE {
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

	void RenderCommand::DrawIndexed(const Ref<VertexArray>& vertexArray) {
		m_RendererAPI->DrawIndexed(vertexArray);
	}

}