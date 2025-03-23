#include "enginepch.h"
#include "Renderer.h"
#include "RenderCommand.h"

namespace HE {
	void Renderer::Begin() {

	}

	void Renderer::End() {

	}

	void Renderer::Submit(const std::shared_ptr<VertexArray>& vertexArray) {
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

}