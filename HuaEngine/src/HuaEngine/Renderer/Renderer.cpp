#include "enginepch.h"
#include "Renderer.h"
#include "RenderCommand.h"

namespace HE {
	void Renderer::Init() {
		RenderCommand::Init();
	}

	void Renderer::Begin() {

	}

	void Renderer::End() {

	}

	void Renderer::Submit(const Ref<VertexArray>& vertexArray) {
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

}